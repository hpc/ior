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
#ifdef HAVE_LUSTRE_LUSTRE_USER_H
#include <lustre/lustre_user.h>
#endif
*/

#include "ior.h"
#include "aiori.h"
#include "iordef.h"

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
static void *HDFS_Create(char *, IOR_param_t *);
static void *HDFS_Open(char *, IOR_param_t *);
static IOR_offset_t HDFS_Xfer(int, void *, IOR_size_t *,
                               IOR_offset_t, IOR_param_t *);
static void HDFS_Close(void *, IOR_param_t *);
static void HDFS_Delete(char *, IOR_param_t *);
static void HDFS_SetVersion(IOR_param_t *);
static void HDFS_Fsync(void *, IOR_param_t *);
static IOR_offset_t HDFS_GetFileSize(IOR_param_t *, MPI_Comm, char *);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t hdfs_aiori = {
	.name = "HDFS",
	.name_legacy = NULL,
	.create = HDFS_Create,
	.open = HDFS_Open,
	.xfer = HDFS_Xfer,
	.close = HDFS_Close,
	.delete = HDFS_Delete,
	.set_version = HDFS_SetVersion,
	.fsync = HDFS_Fsync,
	.get_file_size = HDFS_GetFileSize,
};

/***************************** F U N C T I O N S ******************************/

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
static void hdfs_connect( IOR_param_t* param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> hdfs_connect  [nn:\"%s\", port:%d, user:%s]\n",
					 param->hdfs_name_node,
					 param->hdfs_name_node_port,
					 param->hdfs_user );
	}

	if ( param->hdfs_fs ) {
		if (param->verbose >= VERBOSE_4) {
			printf("<- hdfs_connect  [nothing to do]\n"); /* DEBUGGING */
		}
		return;
	}

	/* initialize a builder, holding parameters for hdfsBuilderConnect() */
	struct hdfsBuilder* builder = hdfsNewBuilder();
	if ( ! builder )
		ERR_SIMPLE("couldn't create an hdfsBuilder");

	hdfsBuilderSetForceNewInstance ( builder ); /* don't use cached instance */

	hdfsBuilderSetNameNode    ( builder, param->hdfs_name_node );
	hdfsBuilderSetNameNodePort( builder, param->hdfs_name_node_port );
	hdfsBuilderSetUserName    ( builder, param->hdfs_user );

	/* NOTE: hdfsBuilderConnect() frees the builder */
	param->hdfs_fs = hdfsBuilderConnect( builder );
	if ( ! param->hdfs_fs )
		ERR_SIMPLE("hdsfsBuilderConnect failed");

	if (param->verbose >= VERBOSE_4) {
		printf("<- hdfs_connect  [success]\n");
	}
}

static void hdfs_disconnect( IOR_param_t* param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> hdfs_disconnect\n");
	}
	if ( param->hdfs_fs ) {
		hdfsDisconnect( param->hdfs_fs );
		param->hdfs_fs = NULL;
	}
	if (param->verbose >= VERBOSE_4) {
		printf("<- hdfs_disconnect\n");
	}
}


/*
 * Create or open the file. Pass TRUE if creating and FALSE if opening an existing file.
 * Return an hdfsFile.
 */

static void *HDFS_Create_Or_Open( char *testFileName, IOR_param_t *param, unsigned char createFile ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Create_Or_Open\n");
	}

	hdfsFile hdfs_file = NULL;
	int      fd_oflags = 0, hdfs_return;

	/* initialize file-system handle, if needed */
	hdfs_connect( param );

	/*
	 * Check for unsupported flags.
	 *
	 * If they want RDWR, we don't know if they're going to try to do both, so we
	 * can't default to either O_RDONLY or O_WRONLY. Thus, we error and exit.
	 *
	 * The other two, we just note that they are not supported and don't do them.
	 */

	if ( param->openFlags & IOR_RDWR ) {
		ERR( "Opening or creating a file in RDWR is not implemented in HDFS" );
	}

	if ( param->openFlags & IOR_EXCL ) {
		fprintf( stdout, "Opening or creating a file in Exclusive mode is not implemented in HDFS\n" );
	}

	if ( param->openFlags & IOR_APPEND ) {
		fprintf( stdout, "Opening or creating a file for appending is not implemented in HDFS\n" );
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
		hdfs_set_o_direct_flag( &fd_oflags );
	}

	/*
	 * For N-1 write, All other ranks wait for Rank 0 to open the file.
	 * this is bec 0 does truncate and rest don't
	 * it would be dangerous for all to do truncate as they might
	 * truncate each other's writes
	 */

	if (( param->openFlags & IOR_WRONLY ) &&
			( !param->filePerProc )						&&
			( rank != 0 )) {

		MPI_CHECK(MPI_Barrier(testComm), "barrier error");
	}

	/*
	 * Now rank zero can open and truncate, if necessary.
	 */

	if (param->verbose >= VERBOSE_4) {
		printf("\thdfsOpenFile(0x%llx, %s, 0%o, %d, %d, %d)\n",
					 param->hdfs_fs,
					 testFileName,
					 fd_oflags,							/* shown in octal to compare w/ <bits/fcntl.h> */
					 param->transferSize,
					 param->hdfs_replicas,
					 param->hdfs_block_size);
	}
	hdfs_file = hdfsOpenFile( param->hdfs_fs,
														testFileName,
														fd_oflags,
														param->transferSize,
														param->hdfs_replicas,
														param->hdfs_block_size);
	if ( ! hdfs_file ) {
		ERR( "Failed to open the file" );
	}

	/*
	 * For N-1 write, Rank 0 waits for the other ranks to open the file after it has.
	 */

	if (( param->openFlags & IOR_WRONLY ) &&
			( !param->filePerProc )						&&
			( rank == 0 )) {

		MPI_CHECK(MPI_Barrier(testComm), "barrier error");
	}

	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_Create_Or_Open\n");
	}
	return ((void *) hdfs_file );
}

/*
 * Create and open a file through the HDFS interface.
 */

static void *HDFS_Create( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Create\n");
	}

	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_Create\n");
	}
	return HDFS_Create_Or_Open( testFileName, param, TRUE );
}

/*
 * Open a file through the HDFS interface.
 */
static void *HDFS_Open( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Open\n");
	}

	if ( param->openFlags & IOR_CREAT ) {
		if (param->verbose >= VERBOSE_4) {
			printf("<- HDFS_Open( ... TRUE)\n");
		}
		return HDFS_Create_Or_Open( testFileName, param, TRUE );
	}
	else {
		if (param->verbose >= VERBOSE_4) {
			printf("<- HDFS_Open( ... FALSE)\n");
		}
		return HDFS_Create_Or_Open( testFileName, param, FALSE );
	}
}

/*
 * Write or read to file using the HDFS interface.
 */

static IOR_offset_t HDFS_Xfer(int access, void *file, IOR_size_t * buffer,
                              IOR_offset_t length, IOR_param_t * param) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Xfer(acc:%d, file:0x%llx, buf:0x%llx, len:%llu, 0x%llx)\n",
					 access, file, buffer, length, param);
	}

	int       xferRetries = 0;
	long long remaining = (long long)length;
	char*     ptr = (char *)buffer;
	long long rc;
	off_t     offset = param->offset;
	hdfsFS    hdfs_fs = param->hdfs_fs;		/* (void*) */
	hdfsFile  hdfs_file = (hdfsFile)file; /* (void*) */


	while ( remaining > 0 ) {

		/* write/read file */
		if (access == WRITE) {	/* WRITE */
			if (verbose >= VERBOSE_4) {
				fprintf( stdout, "task %d writing to offset %lld\n",
						rank,
						param->offset + length - remaining);
			}

			if (param->verbose >= VERBOSE_4) {
				printf("\thdfsWrite( 0x%llx, 0x%llx, 0x%llx, %lld)\n",
							 hdfs_fs, hdfs_file, ptr, remaining ); /* DEBUGGING */
			}
			rc = hdfsWrite( hdfs_fs, hdfs_file, ptr, remaining );
			if ( rc < 0 ) {
				ERR( "hdfsWrite() failed" );
			}

			offset += rc;

			if ( param->fsyncPerWrite == TRUE ) {
				HDFS_Fsync( hdfs_file, param );
			}
		}
		else {				/* READ or CHECK */
			if (verbose >= VERBOSE_4) {
				fprintf( stdout, "task %d reading from offset %lld\n",
						rank,
						param->offset + length - remaining );
			}

			if (param->verbose >= VERBOSE_4) {
				printf("\thdfsRead( 0x%llx, 0x%llx, 0x%llx, %lld)\n",
							 hdfs_fs, hdfs_file, ptr, remaining ); /* DEBUGGING */
			}
			rc = hdfsRead( hdfs_fs, hdfs_file, ptr, remaining );

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

	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_Xfer\n");
	}
	return ( length );
}

/*
 * Perform hdfs_sync().
 */

static void HDFS_Fsync( void *fd, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Fsync\n");
	}
	hdfsFS   hdfs_fs   = param->hdfs_fs;	/* (void *) */
	hdfsFile hdfs_file = (hdfsFile)fd;		/* (void *) */

#if 0
	if (param->verbose >= VERBOSE_4) {
		printf("\thdfsHSync(0x%llx, 0x%llx)\n", hdfs_fs, hdfs_file);
	}
	if ( hdfsHSync( hdfs_fs, hdfs_file ) != 0 ) {
		EWARN( "hdfsHSync() failed" );
	}
#elif 0
	if (param->verbose >= VERBOSE_4) {
		printf("\thdfsHFlush(0x%llx, 0x%llx)\n", hdfs_fs, hdfs_file);
	}
	if ( hdfsHFlush( hdfs_fs, hdfs_file ) != 0 ) {
		EWARN( "hdfsHFlush() failed" );
	}
#else
	if (param->verbose >= VERBOSE_4) {
		printf("\thdfsFlush(0x%llx, 0x%llx)\n", hdfs_fs, hdfs_file);
	}
	if ( hdfsFlush( hdfs_fs, hdfs_file ) != 0 ) {
		EWARN( "hdfsFlush() failed" );
	}
#endif

	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_Fsync\n");
	}
}

/*
 * Close a file through the HDFS interface.
 */

static void HDFS_Close( void *fd, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Close\n");
	}

	hdfsFS   hdfs_fs   = param->hdfs_fs;	/* (void *) */
	hdfsFile hdfs_file = (hdfsFile)fd;  	/* (void *) */

	int open_flags;

	if ( param->openFlags & IOR_WRONLY ) {
		open_flags = O_CREAT | O_WRONLY;
	} else {
		open_flags = O_RDONLY;
	}

	if ( hdfsCloseFile( hdfs_fs, hdfs_file ) != 0 ) {
		ERR( "hdfsCloseFile() failed" );
	}

	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_Close\n");
	}
}

/*
 * Delete a file through the HDFS interface.
 *
 * NOTE: The signature for ior_aiori.delete doesn't include a parameter to
 * select recursive deletes.  We'll assume that that is never needed.
 */
static void HDFS_Delete( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_Delete\n");
	}

	char errmsg[256];

	/* initialize file-system handle, if needed */
	hdfs_connect( param );

	if ( ! param->hdfs_fs )
		ERR_SIMPLE( "Can't delete a file without an HDFS connection" );

	if ( hdfsDelete( param->hdfs_fs, testFileName, 0 ) != 0 ) {
		sprintf(errmsg,
		        "[RANK %03d]: hdfsDelete() of file \"%s\" failed\n",
		        rank, testFileName);

		EWARN( errmsg );
	}
	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_Delete\n");
	}
}

/*
 * Determine api version.
 */

static void HDFS_SetVersion( IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_SetVersion\n");
	}

	strcpy( param->apiVersion, param->api );
	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_SetVersion\n");
	}
}

/*
 * Use hdfsGetPathInfo() to get info about file?
 * Is there an fstat we can use on hdfs?
 * Should we just use POSIX fstat?
 */

static IOR_offset_t
HDFS_GetFileSize(IOR_param_t * param,
								 MPI_Comm      testComm,
								 char *        testFileName) {
	if (param->verbose >= VERBOSE_4) {
		printf("-> HDFS_GetFileSize(%s)\n", testFileName);
	}

	IOR_offset_t aggFileSizeFromStat;
	IOR_offset_t tmpMin, tmpMax, tmpSum;

  /* make sure file-system is connected */
	hdfs_connect( param );

	/* file-info struct includes size in bytes */
	if (param->verbose >= VERBOSE_4) {
		printf("\thdfsGetPathInfo(%s) ...", testFileName);fflush(stdout);
	}

	hdfsFileInfo* info = hdfsGetPathInfo( param->hdfs_fs, testFileName );
	if ( ! info )
		ERR_SIMPLE( "hdfsGetPathInfo() failed" );
	if (param->verbose >= VERBOSE_4) {
		printf("done.\n");fflush(stdout);
	}

	aggFileSizeFromStat = info->mSize;

	if ( param->filePerProc == TRUE ) {
		if (param->verbose >= VERBOSE_4) {
			printf("\tall-reduce (1)\n");
		}
		MPI_CHECK(
				MPI_Allreduce(
					&aggFileSizeFromStat, &tmpSum, 1, MPI_LONG_LONG_INT, MPI_SUM, testComm ),
				"cannot total data moved" );

		aggFileSizeFromStat = tmpSum;
	}
	else {
		if (param->verbose >= VERBOSE_4) {
			printf("\tall-reduce (2a)\n");
		}
		MPI_CHECK(
				MPI_Allreduce(
					&aggFileSizeFromStat, &tmpMin, 1, MPI_LONG_LONG_INT, MPI_MIN, testComm ),
				"cannot total data moved" );

		if (param->verbose >= VERBOSE_4) {
			printf("\tall-reduce (2b)\n");
		}
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

	if (param->verbose >= VERBOSE_4) {
		printf("<- HDFS_GetFileSize [%llu]\n", aggFileSizeFromStat);
	}
	return ( aggFileSizeFromStat );
}
