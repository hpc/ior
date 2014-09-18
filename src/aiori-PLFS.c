/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/*
 * Copyright (c) 2009, Los Alamos National Security, LLC All rights reserved.
 * Copyright 2009. Los Alamos National Security, LLC. This software was produced
 * under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National
 * Laboratory (LANL), which is operated by Los Alamos National Security, LLC for
 * the U.S. Department of Energy. The U.S. Government has rights to use,
 * reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS
 * ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is
 * modified to produce derivative works, such modified software should be
 * clearly marked, so as not to confuse it with the version available from
 * LANL.
 *
 * Additionally, redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following conditions are
 * met:
 *
 * •   Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * •   Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * •   Neither the name of Los Alamos National Security, LLC, Los Alamos National
 * Laboratory, LANL, the U.S. Government, nor the names of its contributors may be
 * used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL SECURITY, LLC OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/******************************************************************************\
*
* Implement of abstract I/O interface for PLFS.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <sys/ioctl.h>          /* necessary for: */
#define __USE_GNU               /* O_DIRECT and */
#include <fcntl.h>              /* IO operations */
#undef __USE_GNU
#endif                          /* __linux__ */

#include <errno.h>
#include <fcntl.h>              /* IO operations */
#include <sys/stat.h>
#include <assert.h>
/*
#ifdef HAVE_LUSTRE_LUSTRE_USER_H
#include <lustre/lustre_user.h>
#endif
*/

#include "ior.h"
#include "aiori.h"
#include "iordef.h"

#ifndef   open64                /* necessary for TRU64 -- */
#  define open64  open            /* unlikely, but may pose */
#endif  /* not open64 */                        /* conflicting prototypes */

#ifndef   lseek64               /* necessary for TRU64 -- */
#  define lseek64 lseek           /* unlikely, but may pose */
#endif  /* not lseek64 */                        /* conflicting prototypes */

#ifndef   O_BINARY              /* Required on Windows    */
#  define O_BINARY 0
#endif

#include "plfs.h"
#include "utilities.h"


/**************************** P R O T O T Y P E S *****************************/
static void *PLFS_Create(char *, IOR_param_t *);
static void *PLFS_Open(char *, IOR_param_t *);
static IOR_offset_t PLFS_Xfer(int, void *, IOR_size_t *,
                               IOR_offset_t, IOR_param_t *);
static void PLFS_Close(void *, IOR_param_t *);
static void PLFS_Delete(char *, IOR_param_t *);
static void PLFS_SetVersion(IOR_param_t *);
static void PLFS_Fsync(void *, IOR_param_t *);
static IOR_offset_t PLFS_GetFileSize(IOR_param_t *, MPI_Comm, char *);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t plfs_aiori = {
        "PLFS",
        PLFS_Create,
        PLFS_Open,
        PLFS_Xfer,
        PLFS_Close,
        PLFS_Delete,
        PLFS_SetVersion,
        PLFS_Fsync,
        PLFS_GetFileSize
};

/***************************** F U N C T I O N S ******************************/


/*
 * Create or open the file. Pass TRUE if creating and FALSE if opening an existing file.
 */

static void *PLFS_Create_Or_Open( char *testFileName, IOR_param_t *param, unsigned char createFile ) {

  Plfs_fd *plfs_fd = NULL;
  int fd_oflags = 0, plfs_return;


  /*
   * Check for unsupported flags.
   *
   * If they want RDWR, we don't know if they're going to try to do both, so we
   * can't default to either O_RDONLY or O_WRONLY. Thus, we error and exit.
   *
   * The other two, we just note that they are not supported and don't do them.
   */

  if ( param->openFlags & IOR_RDWR ) {
    ERR( "Opening or creating a file in RDWR is not implemented in PLFS" );
  }

  if ( param->openFlags & IOR_EXCL ) {
    fprintf( stdout, "Opening or creating a file in Exclusive mode is not implemented in PLFS\n" );
  }

  if ( param->openFlags & IOR_APPEND ) {
    fprintf( stdout, "Opening or creating a file for appending is not implemented in PLFS\n" );
  }

  /*
   * Setup the flags to be used.
   */

  if ( createFile == TRUE ) {
    fd_oflags = O_CREAT;
  }

  if ( param->openFlags & IOR_WRONLY ) {
    if ( !param->filePerProc ) {
      // in N-1 mode, only rank 0 truncates the file
      if ( rank != 0 ) {
        fd_oflags |= O_WRONLY;
      } else {
        fd_oflags |= O_TRUNC;
        fd_oflags |= O_WRONLY;
      }
    } else {
      // in N-N mode, everyone does truncate
      fd_oflags |= O_TRUNC;
      fd_oflags |= O_WRONLY;
    }
  } else {
    fd_oflags |= O_RDONLY;
  }

  /*
   * Now see if O_DIRECT is needed.
   */

  if ( param->useO_DIRECT == TRUE ) {
    set_o_direct_flag( &fd_oflags );
  }

  /*
   * For N-1 write, All other ranks wait for Rank 0 to open the file.
   * this is bec 0 does truncate and rest don't
   * it would be dangerous for all to do truncate as they might
   * truncate each other's writes
   */

  if (( param->openFlags & IOR_WRONLY ) && 
      ( !param->filePerProc )           &&   
      ( rank != 0 )) { 

    MPI_CHECK(MPI_Barrier(testComm), "barrier error");
  }

  /*
   * Now rank zero can open and truncate, if necessary.
   */

  plfs_return = plfs_open( &( plfs_fd ), testFileName, fd_oflags, rank, 0666, NULL );

  if ( plfs_return != 0 ) {
    ERR( "Failed to open the file" );
  }

  /*
   * For N-1 write, Rank 0 waits for the other ranks to open the file after it has.
   */

  if (( param->openFlags & IOR_WRONLY ) && 
      ( !param->filePerProc )           &&   
      ( rank == 0 )) { 

    MPI_CHECK(MPI_Barrier(testComm), "barrier error");
  }    

  return ((void *) plfs_fd );
}

/*
 * Create and open a file through the PLFS interface.
 */

static void *PLFS_Create( char *testFileName, IOR_param_t * param ) {

  return PLFS_Create_Or_Open( testFileName, param, TRUE );
}

/*
 * Open a file through the PLFS interface.
 */
static void *PLFS_Open( char *testFileName, IOR_param_t * param ) {

  if ( param->openFlags & IOR_CREAT ) {
    return PLFS_Create_Or_Open( testFileName, param, TRUE );
  } else {
    return PLFS_Create_Or_Open( testFileName, param, FALSE );
  }
}

/*
 * Write or read access to file using the PLFS interface.
 */

static IOR_offset_t PLFS_Xfer(int access, void *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_param_t * param) {

  int       xferRetries = 0;
  long long remaining = (long long)length;
  char *    ptr = (char *)buffer;
  long long rc;
  off_t     offset = param->offset;
  Plfs_fd * plfs_fd = (Plfs_fd *)file;


  while ( remaining > 0 ) {
    /* write/read file */
    if (access == WRITE) {  /* WRITE */
      if (verbose >= VERBOSE_4) {
        fprintf( stdout, "task %d writing to offset %lld\n",
            rank,
            param->offset + length - remaining);
      }

#if 0
      rc = plfs_write( plfs_fd, ptr, remaining, offset, rank ); /* PLFS 2.4 */
#else
      ssize_t bytes_written = 0;
      rc = plfs_write( plfs_fd, ptr, remaining, offset, rank, &bytes_written ); /* PLFS 2.5 */
#endif
      if ( rc < 0 ) {
        ERR( "plfs_write() failed" );
      }

      offset += rc;

      if ( param->fsyncPerWrite == TRUE ) {
        PLFS_Fsync( plfs_fd, param );
      }
    } else {        /* READ or CHECK */
      if (verbose >= VERBOSE_4) {
        fprintf( stdout, "task %d reading from offset %lld\n",
            rank,
            param->offset + length - remaining );
      }

#if 0
      rc = plfs_read( plfs_fd, ptr, remaining, param->offset ); /* PLFS 2.4 */
#else
      ssize_t bytes_read = 0;
      rc = plfs_read( plfs_fd, ptr, remaining, param->offset, &bytes_read ); /* PLFS 2.5 */
#endif

      if ( rc == 0 ) {
        ERR( "plfs_read() returned EOF prematurely" );
      }

      if ( rc < 0 ) {
        ERR( "plfs_read() failed" );
      }

      offset += rc;
    }

    if ( rc < remaining ) {
      fprintf(stdout, "WARNING: Task %d, partial %s, %lld of %lld bytes at offset %lld\n",
          rank,
          access == WRITE ? "plfs_write()" : "plfs_read()",
          rc, remaining,
          param->offset + length - remaining );

      if ( param->singleXferAttempt == TRUE ) {
        MPI_CHECK( MPI_Abort( MPI_COMM_WORLD, -1 ), "barrier error" );
      }

      if ( xferRetries > MAX_RETRY ) {
        ERR( "too many retries -- aborting" );
      }
    }

    assert( rc >= 0 );
    assert( rc <= remaining );
    remaining -= rc;
    ptr += rc;
    xferRetries++;
  }

  return ( length );
}

/*
 * Perform plfs_sync().
 */

static void PLFS_Fsync( void *fd, IOR_param_t * param ) {

  Plfs_fd *plfs_fd = (Plfs_fd *)fd;

  if ( plfs_sync( plfs_fd ) != 0 ) {
    EWARN( "plfs_sync() failed" );
  }
}

/*
 * Close a file through the PLFS interface.
 */

static void PLFS_Close( void *fd, IOR_param_t * param ) {

  Plfs_fd * plfs_fd = (Plfs_fd *)fd;
  int       open_flags;
  long long rc;

  if ( param->openFlags & IOR_WRONLY ) {
    open_flags = O_CREAT | O_WRONLY;
  } else {
    open_flags = O_RDONLY;
  }

#if 0
  rc = plfs_close( plfs_fd, rank, param->id, open_flags, NULL ); /* PLFS 2.4 */
#else
  int ref_count = 0;
  rc = plfs_close( plfs_fd, rank, param->id, open_flags, NULL, &ref_count ); /* PLFS 2.5 */
#endif

  if ( rc != 0 ) {
    ERR( "plfs_close() failed" );
  }

  free( fd );
}

/*
 * Delete a file through the POSIX interface.
 */
static void PLFS_Delete( char *testFileName, IOR_param_t * param ) {

  char errmsg[256];

  if ( plfs_unlink( testFileName ) != 0 ) {
    sprintf(
        errmsg,
        "[RANK %03d]: plfs_unlink() of file \"%s\" failed\n",
        rank, testFileName);

    EWARN( errmsg );
  }
}

/*
 * Determine api version.
 */

static void PLFS_SetVersion( IOR_param_t * test ) {

  strcpy( test->apiVersion, test->api );
}

/*
 * Use plfs_getattr to return aggregate file size.
 */

static IOR_offset_t PLFS_GetFileSize(
        IOR_param_t * test, MPI_Comm testComm, char *testFileName) {

  int          plfs_return;
  Plfs_fd *    plfs_fd = NULL;
  struct stat  stat_buf;
  IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;
  long long    rc;


  plfs_return = plfs_open( &( plfs_fd ), testFileName, O_RDONLY, rank, 0666, NULL );

  if ( plfs_return != 0 ) {
    ERR( "plfs_open() failed before reading the file size attribute" );
  }

  if ( plfs_getattr( plfs_fd, testFileName, &stat_buf, 1 ) != 0 ) {
    ERR( "plfs_getattr() failed" );
  }

#if 0
  rc = plfs_close( plfs_fd, rank, test->id, O_RDONLY, NULL ); /* PLFS 2.4 */
#else
  int ref_count = 0;
  rc = plfs_close( plfs_fd, rank, test->id, O_RDONLY, NULL, &ref_count ); /* PLFS 2.5 */
#endif

  if ( rc != 0 ) {
    ERR( "plfs_close() failed after reading the file size attribute" );
  }

  aggFileSizeFromStat = stat_buf.st_size;

  if ( test->filePerProc == TRUE ) {
    MPI_CHECK(
        MPI_Allreduce(
          &aggFileSizeFromStat, &tmpSum, 1, MPI_LONG_LONG_INT, MPI_SUM, testComm ),
        "cannot total data moved" );

    aggFileSizeFromStat = tmpSum;
  } else {
    MPI_CHECK(
        MPI_Allreduce(
          &aggFileSizeFromStat, &tmpMin, 1, MPI_LONG_LONG_INT, MPI_MIN, testComm ),
        "cannot total data moved" );

    MPI_CHECK(
        MPI_Allreduce(
          &aggFileSizeFromStat, &tmpMax, 1, MPI_LONG_LONG_INT, MPI_MAX, testComm ),
        "cannot total data moved" );

    if ( tmpMin != tmpMax ) {
      if ( rank == 0 ) {
        WARN( "inconsistent file size by different tasks" );
      }

      /* incorrect, but now consistent across tasks */
      aggFileSizeFromStat = tmpMin;
    }
  }

  return ( aggFileSizeFromStat );
}
