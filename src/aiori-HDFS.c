/* -*- mode: c; indent-tabs-mode: t; -*-
 * vim:noexpandtab:
 *
 * Editing with tabs allows different users to pick their own indentation
 * appearance without changing the file.
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
* Implement of abstract I/O interface for HDFS.
*
* HDFS has the added concept of a "File System Handle" which has to be
* connected before files are opened.  We store this in the IOR_param_t
* object that is always passed to our functions.  The thing that callers
* think of as the "fd" is an hdfsFile, (a pointer).
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
# include <sys/ioctl.h>          /* necessary for: */
# define __USE_GNU               /* O_DIRECT and */
# include <fcntl.h>              /* IO operations */
# undef __USE_GNU
#endif                          /* __linux__ */

#include <errno.h>
#include <fcntl.h>              /* IO operations */
#include <sys/stat.h>
#include <assert.h>
/*
#ifdef HAVE_LUSTRE_USER
#  ifdef HAVE_LINUX_LUSTRE_LUSTRE_USER_H
#    include <linux/lustre/lustre_user.h>
#  elif defined(HAVE_LUSTRE_LUSTRE_USER_H)
#    include <lustre/lustre_user.h>
#  endif
#endif
*/
#include "ior.h"
#include "aiori.h"
#include "utilities.h"

#ifndef   open64                /* necessary for TRU64 -- */
# define open64  open            /* unlikely, but may pose */
#endif  /* not open64 */                        /* conflicting prototypes */

#ifndef   lseek64               /* necessary for TRU64 -- */
# define lseek64 lseek           /* unlikely, but may pose */
#endif  /* not lseek64 */                        /* conflicting prototypes */

#ifndef   O_BINARY              /* Required on Windows    */
# define O_BINARY 0
#endif

#include "hdfs.h"

/**************************** P R O T O T Y P E S *****************************/
static aiori_fd_t *HDFS_Create(char *testFileName, int flags, aiori_mod_opt_t * param);
static aiori_fd_t *HDFS_Open(char *testFileName, int flags, aiori_mod_opt_t * param);
static IOR_offset_t HDFS_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param);
static void HDFS_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void HDFS_Delete(char *testFileName, aiori_mod_opt_t * param);
static void HDFS_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static IOR_offset_t HDFS_GetFileSize(aiori_mod_opt_t *,char *);
static void hdfs_xfer_hints(aiori_xfer_hint_t * params);
static option_help * HDFS_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values);
static int HDFS_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options);
static int HDFS_rmdir (const char *path, aiori_mod_opt_t * options);
static int HDFS_access (const char *path, int mode, aiori_mod_opt_t * options);
static int HDFS_stat (const char *path, struct stat *buf, aiori_mod_opt_t * options);
static int HDFS_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options);

static aiori_xfer_hint_t * hints = NULL;

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t hdfs_aiori = {
	.name = "HDFS",
	.name_legacy = NULL,
	.create = HDFS_Create,
	.open = HDFS_Open,
	.xfer = HDFS_Xfer,
	.close = HDFS_Close,
	.remove = HDFS_Delete,
  .get_options = HDFS_options,
	.get_version = aiori_get_version,
  .xfer_hints = hdfs_xfer_hints,
	.fsync = HDFS_Fsync,
	.get_file_size = HDFS_GetFileSize,
  .statfs = HDFS_statfs,
  .mkdir = HDFS_mkdir,
  .rmdir = HDFS_rmdir,
  .access = HDFS_access,
  .stat = HDFS_stat,
  .enable_mdtest = true
};

/***************************** F U N C T I O N S ******************************/

void hdfs_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
}

/************************** O P T I O N S *****************************/
typedef struct {
  char      *  user;
  char      *  name_node;
  int          replicas;       /* n block replicas.  (0 gets default) */
  int          direct_io;
  IOR_offset_t block_size;     /* internal blk-size. (0 gets default) */
  // runtime options
  hdfsFS       fs;             /* file-system handle */
  tPort        name_node_port; /* (uint16_t) */
} hdfs_options_t;

static void hdfs_connect( hdfs_options_t* o );

option_help * HDFS_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  hdfs_options_t * o = malloc(sizeof(hdfs_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(hdfs_options_t));
  }else{
    memset(o, 0, sizeof(hdfs_options_t));
    char *hdfs_user;
    hdfs_user = getenv("USER");
    if (!hdfs_user){
      hdfs_user = "";
    }
    o->user = strdup(hdfs_user);
    o->name_node      = "default";
  }

  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
    {0, "hdfs.odirect", "Direct I/O Mode", OPTION_FLAG, 'd', & o->direct_io},
    {0, "hdfs.user", "Username", OPTION_OPTIONAL_ARGUMENT, 's', & o->user},
    {0, "hdfs.name_node", "Namenode", OPTION_OPTIONAL_ARGUMENT, 's', & o->name_node},
    {0, "hdfs.replicas", "Number of replicas", OPTION_OPTIONAL_ARGUMENT, 'd', & o->replicas},
    {0, "hdfs.block_size", "Blocksize", OPTION_OPTIONAL_ARGUMENT, 'l', & o->block_size},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}


int HDFS_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options){
  hdfs_options_t * o = (hdfs_options_t*) options;
  hdfs_connect(o);
  return hdfsCreateDirectory(o->fs, path);
}

int HDFS_rmdir (const char *path, aiori_mod_opt_t * options){
  hdfs_options_t * o = (hdfs_options_t*) options;
  hdfs_connect(o);
  return hdfsDelete(o->fs, path, 1);
}

int HDFS_access (const char *path, int mode, aiori_mod_opt_t * options){
  hdfs_options_t * o = (hdfs_options_t*) options;
  hdfs_connect(o);
  return hdfsExists(o->fs, path);
}

int HDFS_stat (const char *path, struct stat *buf, aiori_mod_opt_t * options){
  hdfsFileInfo * stat;
  hdfs_options_t * o = (hdfs_options_t*) options;
  hdfs_connect(o);
  stat = hdfsGetPathInfo(o->fs, path);
  if(stat == NULL){
    return 1;
  }
  memset(buf, 0, sizeof(struct stat));
  buf->st_atime = stat->mLastAccess;
  buf->st_size = stat->mSize;
  buf->st_mtime = stat->mLastMod;
  buf->st_mode = stat->mPermissions;

  hdfsFreeFileInfo(stat, 1);
  return 0;
}

int HDFS_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options){
  hdfs_options_t * o = (hdfs_options_t*) options;
  hdfs_connect(o);

  stat->f_bsize = hdfsGetDefaultBlockSize(o->fs);
  stat->f_blocks = hdfsGetCapacity(o->fs) / hdfsGetDefaultBlockSize(o->fs);
  stat->f_bfree =  stat->f_blocks - hdfsGetUsed(o->fs) / hdfsGetDefaultBlockSize(o->fs);
  stat->f_bavail = 1;
  stat->f_files = 1;
  stat->f_ffree = 1;
  return 0;
}

/* This is identical to the one in aiori-POSIX.c  Doesn't seem like
 * it would be appropriate in utilities.c.
 */

void hdfs_set_o_direct_flag(int *fd)
{
/* note that TRU64 needs O_DIRECTIO, SunOS uses directio(),
   and everyone else needs O_DIRECT */
#ifndef O_DIRECT
#	ifndef O_DIRECTIO
		WARN("cannot use O_DIRECT");
#		define O_DIRECT 000000
#	else                          /* O_DIRECTIO */
#		define O_DIRECT O_DIRECTIO
#	endif                         /* not O_DIRECTIO */
#endif                          /* not O_DIRECT */

	*fd |= O_DIRECT;
}


/*
 * "Connect" to an HDFS file-system.  HDFS requires this be done before and
 * files are opened.  It is easy for ior_aiori.open/create to assure that
 * we connect, if we haven't already done so.  However, there's not a
 * simple way to assure that we disconnect after the last file-close.  For
 * now, we'll make a special call at the end of ior.c
 *
 * NOTE: It's okay to call this thing whenever you need to be sure the HDFS
 *       filesystem is connected.
 */
void hdfs_connect( hdfs_options_t* o ) {
	if (verbose >= VERBOSE_4) {
		printf("-> hdfs_connect  [nn:\"%s\", port:%d, user:%s]\n",
					 o->name_node,
					 o->name_node_port,
					 o->user );
	}

	if ( o->fs ) {
		if (verbose >= VERBOSE_4) {
			printf("<- hdfs_connect  [nothing to do]\n"); /* DEBUGGING */
		}
		return;
	}

	/* initialize a builder, holding parameters for hdfsBuilderConnect() */
	struct hdfsBuilder* builder = hdfsNewBuilder();
	if ( ! builder ){
		ERR("couldn't create an hdfsBuilder");
  }

	hdfsBuilderSetForceNewInstance ( builder ); /* don't use cached instance */

	hdfsBuilderSetNameNode    ( builder, o->name_node );
	hdfsBuilderSetNameNodePort( builder, o->name_node_port );
	hdfsBuilderSetUserName    ( builder, o->user );

	/* NOTE: hdfsBuilderConnect() frees the builder */
	o->fs = hdfsBuilderConnect( builder );
	if ( ! o->fs )
		ERR("hdsfsBuilderConnect failed");

	if (verbose >= VERBOSE_4) {
		printf("<- hdfs_connect  [success]\n");
	}
}

static void hdfs_disconnect( hdfs_options_t* o ) {
	if (verbose >= VERBOSE_4) {
		printf("-> hdfs_disconnect\n");
	}
	if ( o->fs ) {
		hdfsDisconnect( o->fs );
		o->fs = NULL;
	}
	if (verbose >= VERBOSE_4) {
		printf("<- hdfs_disconnect\n");
	}
}


/*
 * Create or open the file. Pass TRUE if creating and FALSE if opening an existing file.
 * Return an hdfsFile.
 */

static void *HDFS_Create_Or_Open( char *testFileName, int flags, aiori_mod_opt_t *param, unsigned char createFile ) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_Create_Or_Open\n");
	}
  hdfs_options_t * o = (hdfs_options_t*) param;

	hdfsFile hdfs_file = NULL;
	int      fd_oflags = 0, hdfs_return;

	/* initialize file-system handle, if needed */
	hdfs_connect( o );

	/*
	 * Check for unsupported flags.
	 *
	 * If they want RDWR, we don't know if they're going to try to do both, so we
	 * can't default to either O_RDONLY or O_WRONLY. Thus, we error and exit.
	 *
	 * The other two, we just note that they are not supported and don't do them.
	 */

	if ( flags & IOR_RDWR ) {
		ERR( "Opening or creating a file in RDWR is not implemented in HDFS" );
	}

	if ( flags & IOR_EXCL ) {
		fprintf( stdout, "Opening or creating a file in Exclusive mode is not implemented in HDFS\n" );
	}

	if ( flags & IOR_APPEND ) {
		fprintf( stdout, "Opening or creating a file for appending is not implemented in HDFS\n" );
	}

	/*
	 * Setup the flags to be used.
	 */

	if ( createFile == TRUE ) {
		fd_oflags = O_CREAT;
	}

	if ( flags & IOR_WRONLY ) {
		if ( ! hints->filePerProc ) {

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

	if ( o->direct_io == TRUE ) {
		hdfs_set_o_direct_flag( &fd_oflags );
	}

	/*
	 * For N-1 write, All other ranks wait for Rank 0 to open the file.
	 * this is bec 0 does truncate and rest don't
	 * it would be dangerous for all to do truncate as they might
	 * truncate each other's writes
	 */

	if (( flags & IOR_WRONLY ) && ( ! hints->filePerProc )	&& ( rank != 0 )) {
		MPI_CHECK(MPI_Barrier(testComm), "barrier error");
	}

	/*
	 * Now rank zero can open and truncate, if necessary.
	 */

	if (verbose >= VERBOSE_4) {
		printf("\thdfsOpenFile(%p, %s, 0%o, %lld, %d, %lld)\n",
					 o->fs,
					 testFileName,
					 fd_oflags,							/* shown in octal to compare w/ <bits/fcntl.h> */
					 hints->transferSize,
					 o->replicas,
					 o->block_size);
	}
	hdfs_file = hdfsOpenFile( o->fs, testFileName, fd_oflags, hints->transferSize, o->replicas, o->block_size);
	if ( ! hdfs_file ) {
		ERR( "Failed to open the file" );
	}

	/*
	 * For N-1 write, Rank 0 waits for the other ranks to open the file after it has.
	 */

	if (( flags & IOR_WRONLY ) &&
			( !hints->filePerProc )						&&
			( rank == 0 )) {

		MPI_CHECK(MPI_Barrier(testComm), "barrier error");
	}

	if (verbose >= VERBOSE_4) {
		printf("<- HDFS_Create_Or_Open\n");
	}
	return ((void *) hdfs_file );
}

/*
 * Create and open a file through the HDFS interface.
 */

static aiori_fd_t *HDFS_Create(char *testFileName, int flags, aiori_mod_opt_t * param) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_Create\n");
	}

	if (verbose >= VERBOSE_4) {
		printf("<- HDFS_Create\n");
	}
	return HDFS_Create_Or_Open( testFileName, flags, param, TRUE );
}

/*
 * Open a file through the HDFS interface.
 */
static aiori_fd_t *HDFS_Open(char *testFileName, int flags, aiori_mod_opt_t * param) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_Open\n");
	}

	if ( flags & IOR_CREAT ) {
		if (verbose >= VERBOSE_4) {
			printf("<- HDFS_Open( ... TRUE)\n");
		}
		return HDFS_Create_Or_Open( testFileName, flags, param, TRUE );
	}
	else {
		if (verbose >= VERBOSE_4) {
			printf("<- HDFS_Open( ... FALSE)\n");
		}
		return HDFS_Create_Or_Open( testFileName, flags, param, FALSE );
	}
}

/*
 * Write or read to file using the HDFS interface.
 */

static IOR_offset_t HDFS_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_Xfer(acc:%d, file:%p, buf:%p, len:%llu, %p)\n",
					 access, file, buffer, length, param);
	}
  hdfs_options_t * o = (hdfs_options_t*) param;
	int       xferRetries = 0;
	long long remaining = (long long)length;
	char*     ptr = (char *)buffer;
	long long rc;
	hdfsFS    hdfs_fs = o->fs;		/* (void*) */
	hdfsFile  hdfs_file = (hdfsFile)file; /* (void*) */


	while ( remaining > 0 ) {

		/* write/read file */
		if (access == WRITE) {	/* WRITE */
			if (verbose >= VERBOSE_4) {
				fprintf( stdout, "task %d writing to offset %lld\n",
						rank,
						offset + length - remaining);
			}

			if (verbose >= VERBOSE_4) {
				printf("\thdfsWrite( %p, %p, %p, %lld)\n",
							 hdfs_fs, hdfs_file, ptr, remaining ); /* DEBUGGING */
			}
			rc = hdfsWrite( hdfs_fs, hdfs_file, ptr, remaining );
			if ( rc < 0 ) {
				ERR( "hdfsWrite() failed" );
			}
			offset += rc;

			if ( hints->fsyncPerWrite == TRUE ) {
				HDFS_Fsync( file, param );
			}
		}
		else {				/* READ or CHECK */
			if (verbose >= VERBOSE_4) {
				fprintf( stdout, "task %d reading from offset %lld\n",
						rank, offset + length - remaining );
			}

			if (verbose >= VERBOSE_4) {
				printf("\thdfsRead( %p, %p, %p, %lld)\n",
							 hdfs_fs, hdfs_file, ptr, remaining ); /* DEBUGGING */
			}
			rc = hdfsPread(hdfs_fs, hdfs_file, offset, ptr, remaining);
			if ( rc == 0 ) {
				ERR( "hdfs_read() returned EOF prematurely" );
			}

			if ( rc < 0 ) {
				ERR( "hdfs_read() failed" );
			}

			offset += rc;
		}


		if ( rc < remaining ) {
			fprintf(stdout, "WARNING: Task %d, partial %s, %lld of %lld bytes at offset %lld\n",
					rank,
					access == WRITE ? "hdfsWrite()" : "hdfs_read()",
					rc, remaining,
					offset + length - remaining );

			if ( hints->singleXferAttempt == TRUE ) {
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

  if(access == WRITE){
    // flush user buffer, this makes the write visible to readers
    // it is the expected semantics of read/writes
    rc = hdfsHFlush(hdfs_fs, hdfs_file);
    if(rc != 0){
      WARN("Error during flush");
    }
  }

	if (verbose >= VERBOSE_4) {
		printf("<- HDFS_Xfer\n");
	}
	return ( length );
}

/*
 * Perform hdfs_sync().
 */
static void HDFS_Fsync(aiori_fd_t * fd, aiori_mod_opt_t * param) {
  hdfs_options_t * o = (hdfs_options_t*) param;
	hdfsFS   hdfs_fs   = o->fs;	/* (void *) */
	hdfsFile hdfs_file = (hdfsFile)fd;		/* (void *) */

	if (verbose >= VERBOSE_4) {
		printf("\thdfsFlush(%p, %p)\n", hdfs_fs, hdfs_file);
	}
	if ( hdfsHSync( hdfs_fs, hdfs_file ) != 0 ) {
    // Hsync is implemented to flush out data with newer Hadoop versions
		WARN( "hdfsFlush() failed" );
	}
}

/*
 * Close a file through the HDFS interface.
 */

static void HDFS_Close(aiori_fd_t * fd, aiori_mod_opt_t * param) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_Close\n");
	}
  hdfs_options_t * o = (hdfs_options_t*) param;

	hdfsFS   hdfs_fs   = o->fs;	/* (void *) */
	hdfsFile hdfs_file = (hdfsFile)fd;  	/* (void *) */

	if ( hdfsCloseFile( hdfs_fs, hdfs_file ) != 0 ) {
		ERR( "hdfsCloseFile() failed" );
	}

	if (verbose >= VERBOSE_4) {
		printf("<- HDFS_Close\n");
	}
}

/*
 * Delete a file through the HDFS interface.
 *
 * NOTE: The signature for ior_aiori.remove doesn't include a parameter to
 * select recursive deletes.  We'll assume that that is never needed.
 */
static void HDFS_Delete( char *testFileName, aiori_mod_opt_t * param ) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_Delete\n");
	}

  hdfs_options_t * o = (hdfs_options_t*) param;
	char errmsg[256];

	/* initialize file-system handle, if needed */
	hdfs_connect(o);

	if ( ! o->fs )
		ERR( "Can't delete a file without an HDFS connection" );

	if ( hdfsDelete( o->fs, testFileName, 0 ) != 0 ) {
		sprintf(errmsg, "[RANK %03d]: hdfsDelete() of file \"%s\" failed\n",
		        rank, testFileName);

		WARN( errmsg );
	}
	if (verbose >= VERBOSE_4) {
		printf("<- HDFS_Delete\n");
	}
}

/*
 * Use hdfsGetPathInfo() to get info about file?
 * Is there an fstat we can use on hdfs?
 * Should we just use POSIX fstat?
 */

static IOR_offset_t HDFS_GetFileSize(aiori_mod_opt_t * param,
								 char *        testFileName) {
	if (verbose >= VERBOSE_4) {
		printf("-> HDFS_GetFileSize(%s)\n", testFileName);
	}
  hdfs_options_t * o = (hdfs_options_t*) param;

	IOR_offset_t aggFileSizeFromStat;
	IOR_offset_t tmpMin, tmpMax, tmpSum;

  /* make sure file-system is connected */
	hdfs_connect( o );

	/* file-info struct includes size in bytes */
	if (verbose >= VERBOSE_4) {
		printf("\thdfsGetPathInfo(%s) ...", testFileName);
    fflush(stdout);
	}

	hdfsFileInfo* info = hdfsGetPathInfo( o->fs, testFileName );
	if ( ! info )
		ERR( "hdfsGetPathInfo() failed" );
	if (verbose >= VERBOSE_4) {
		printf("done.\n");fflush(stdout);
	}

	aggFileSizeFromStat = info->mSize;

	if (verbose >= VERBOSE_4) {
		printf("<- HDFS_GetFileSize [%llu]\n", aggFileSizeFromStat);
	}
	return ( aggFileSizeFromStat );
}
