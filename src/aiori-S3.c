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

/******************************************************************************
 *
 * Implementation of abstract IOR interface, for the Amazon S3 API.
 * EMC/ViPR supports some useful extensions to S3, which we also implement
 * here.  There are 3 different mixes:
 *
 * (1) "Pure S3" uses S3 "Multi-Part Upload" to do N:1 writes.  N:N writes
 *     fail, in the case where IOR "transfer-size" differs from
 *     "block-size', because this implies an "append", and append is not
 *     supported in S3.  [TBD: The spec also says multi-part upload can't
 *     have any individual part greater than 5MB, or more then 10k total
 *     parts.  Failing these conditions may produce obscure errors. Should
 *     we enforce? ]
 *
 *     --> Select this option with the '-a S3' command-line arg to IOR
 *
 *
 * (2) "EMC S3 Extensions" uses the EMC byte-range support for N:1
 *     writes, eliminating Multi-Part Upload.  EMC expects this will
 *     perform better than MPU, and it avoids some problems that are
 *     imposed by the S3 MPU spec.  [See comments at EMC_Xfer().]
 *
 *     --> Select this option with the '-a EMC_S3' command-line arg to IOR
 *
 *
 * NOTE: Putting EMC's S3-extensions in the same file with the S3 API
 *       allows us to share some code that would otherwise be duplicated
 *       (e.g. s3_connect(), etc).  This should also help us avoid losing
 *       bug fixes that are discovered in one interface or the other.  In
 *       some cases, S3 is incapable of supporting all the needs of IOR.
 *       (For example, see notes about "append", above S3_Xfer().
 *
 ******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>				/* strnstr() */

#include <errno.h>
#include <assert.h>
/*
#ifdef HAVE_LUSTRE_LUSTRE_USER_H
#include <lustre/lustre_user.h>
#endif
*/

#include "ior.h"
#include "aiori.h"
#include "iordef.h"

#include <curl/curl.h>

#include <libxml/parser.h>      // from libxml2
#include <libxml/tree.h>

#include "aws4c.h"              // extended vers of "aws4c" lib for S3 via libcurl
#include "aws4c_extra.h"        // utilities, e.g. for parsing XML in responses




/* buffer is used to generate URLs, err_msgs, etc */
#define            BUFF_SIZE  1024
static char        buff[BUFF_SIZE];

const int          ETAG_SIZE = 32;

CURLcode           rc;

/* Any objects we create or delete will be under this bucket */
const char* bucket_name = "ior";

/* TODO: The following stuff goes into options! */
/* REST/S3 variables */
//    CURL*       curl;             /* for libcurl "easy" fns (now managed by aws4c) */
#   define      IOR_CURL_INIT        0x01 /* curl top-level inits were perfomed once? */
#   define      IOR_CURL_NOCONTINUE  0x02
#   define      IOR_CURL_S3_EMC_EXT  0x04 /* allow EMC extensions to S3? */

#ifdef USE_S3_AIORI
#  include <curl/curl.h>
#  include "aws4c.h"
#else
   typedef void     CURL;       /* unused, but needs a type */
   typedef void     IOBuf;      /* unused, but needs a type */
#endif

    IOBuf*      io_buf;              /* aws4c places parsed header values here */
    IOBuf*      etags;               /* accumulate ETags for N:1 parts */

///////////////////////////////////////////////

/**************************** P R O T O T Y P E S *****************************/
static void*        S3_Create(char*, IOR_param_t*);
static void*        S3_Open(char*, IOR_param_t*);
static IOR_offset_t S3_Xfer(int, void*, IOR_size_t*, IOR_offset_t, IOR_param_t*);
static void         S3_Close(void*, IOR_param_t*);

static void*        EMC_Create(char*, IOR_param_t*);
static void*        EMC_Open(char*, IOR_param_t*);
static IOR_offset_t EMC_Xfer(int, void*, IOR_size_t*, IOR_offset_t, IOR_param_t*);
static void         EMC_Close(void*, IOR_param_t*);

static void         S3_Delete(char*, IOR_param_t*);
static void         S3_Fsync(void*, IOR_param_t*);
static IOR_offset_t S3_GetFileSize(IOR_param_t*, MPI_Comm, char*);
static void S3_init();
static void S3_finalize();
static int S3_check_params(IOR_param_t *);


/************************** D E C L A R A T I O N S ***************************/

// "Pure S3"
//     N:1 writes use multi-part upload
//     N:N fails if "transfer-size" != "block-size" (because that requires "append")
ior_aiori_t s3_aiori = {
	.name = "S3",
	.name_legacy = NULL,
	.create = S3_Create,
	.open = S3_Open,
	.xfer = S3_Xfer,
	.close = S3_Close,
	.delete = S3_Delete,
	.get_version = aiori_get_version,
	.fsync = S3_Fsync,
	.get_file_size = S3_GetFileSize,
	.initialize = S3_init,
	.finalize = S3_finalize,
  .check_params = S3_check_params
};

// "S3", plus EMC-extensions enabled
//     N:1 writes use multi-part upload
//     N:N succeeds (because EMC-extensions support "append")
ior_aiori_t s3_plus_aiori = {
	.name = "S3_plus",
	.create = S3_Create,
	.open = S3_Open,
	.xfer = S3_Xfer,
	.close = S3_Close,
	.delete = S3_Delete,
	.set_version = S3_SetVersion,
	.fsync = S3_Fsync,
	.get_file_size = S3_GetFileSize,
	.initialize = S3_init,
	.finalize = S3_finalize
};

// Use EMC-extensions for N:1 write, as well
//     N:1 writes use EMC byte-range
//     N:N succeeds because EMC-extensions support "append"
ior_aiori_t s3_emc_aiori = {
	.name = "S3_EMC",
	.create = EMC_Create,
	.open = EMC_Open,
	.xfer = EMC_Xfer,
	.close = EMC_Close,
	.delete = S3_Delete,
	.set_version = S3_SetVersion,
	.fsync = S3_Fsync,
	.get_file_size = S3_GetFileSize,
	.initialize = S3_init,
	.finalize = S3_finalize
};


static void S3_init(){
  /* This is supposed to be done before *any* threads are created.
   * Could MPI_Init() create threads (or call multi-threaded
   * libraries)?  We'll assume so. */
  AWS4C_CHECK( aws_init() );
}

static void S3_finalize(){
  /* done once per program, after exiting all threads.
 	* NOTE: This fn doesn't return a value that can be checked for success. */
  aws_cleanup();
}

static int S3_check_params(IOR_param_t * test){
  /* N:1 and N:N */
  IOR_offset_t  NtoN = test->filePerProc;
  IOR_offset_t  Nto1 = ! NtoN;
  IOR_offset_t  s    = test->segmentCount;
  IOR_offset_t  t    = test->transferSize;
  IOR_offset_t  b    = test->blockSize;

  if (Nto1 && (s != 1) && (b != t)) {
    ERR("N:1 (strided) requires xfer-size == block-size");
    return 0;
  }

  return 1;
}

/* modelled on similar macros in iordef.h */
#define CURL_ERR(MSG, CURL_ERRNO, PARAM)										\
	do {																					\
		fprintf(stdout, "ior ERROR: %s: %s (curl-errno=%d) (%s:%d)\n",	\
				  MSG, curl_easy_strerror(CURL_ERRNO), CURL_ERRNO,			\
				  __FILE__, __LINE__);												\
		fflush(stdout);																\
		MPI_Abort((PARAM)->testComm, -1);										\
	} while (0)


#define CURL_WARN(MSG, CURL_ERRNO)													\
	do {																						\
		fprintf(stdout, "ior WARNING: %s: %s (curl-errno=%d) (%s:%d)\n",	\
				  MSG, curl_easy_strerror(CURL_ERRNO), CURL_ERRNO,				\
				  __FILE__, __LINE__);													\
		fflush(stdout);																	\
	} while (0)


/***************************** F U N C T I O N S ******************************/




/* ---------------------------------------------------------------------------
 * "Connect" to an S3 object-file-system.  We're really just initializing
 * libcurl.  We need this done before any interactions.  It is easy for
 * ior_aiori.open/create to assure that we connect, if we haven't already
 * done so.  However, there's not a simple way to assure that we
 * "disconnect" at the end.  For now, we'll make a special call at the end
 * of ior.c
 *
 * NOTE: It's okay to call this thing whenever you need to be sure the curl
 *       handle is initialized.
 *
 * NOTE: Our custom version of aws4c can be configured so that connections
 *       are reused, instead of opened and closed on every operation.  We
 *       do configure it that way, but you still need to call these
 *       connect/disconnet functions, in order to insure that aws4c has
 *       been configured.
 * ---------------------------------------------------------------------------
 */


static void s3_connect( IOR_param_t* param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> s3_connect\n"); /* DEBUGGING */
	}

	if ( param->curl_flags & IOR_CURL_INIT ) {
		if (param->verbose >= VERBOSE_2) {
			printf("<- s3_connect  [nothing to do]\n"); /* DEBUGGING */
		}
		return;
	}

	// --- Done once-only (per rank).  Perform all first-time inits.
	//
	// The aws library requires a config file, as illustrated below.  We
	// assume that the user running the test has an entry in this file,
	// using their login moniker (i.e. `echo $USER`) as the key, as
	// suggested in the example:
	//
	//     <user>:<s3_login_id>:<s3_private_key>
	//
	// This file must not be readable by other than user.
	//
	// NOTE: These inits could be done in init_IORParam_t(), in ior.c, but
	//       would require conditional compilation, there.

	aws_set_debug(param->verbose >= 4);
	aws_read_config(getenv("USER"));  // requires ~/.awsAuth
	aws_reuse_connections(1);

	// initalize IOBufs.  These are basically dynamically-extensible
	// linked-lists.  "growth size" controls the increment of new memory
	// allocated, whenever storage is used up.
	param->io_buf = aws_iobuf_new();
	aws_iobuf_growth_size(param->io_buf, 1024*1024*1);

	param->etags = aws_iobuf_new();
	aws_iobuf_growth_size(param->etags, 1024*1024*8);

   // WARNING: if you have http_proxy set in your environment, you may need
   //          to override it here.  TBD: add a command-line variable to
   //          allow you to define a proxy.
   //
	// our hosts are currently 10.140.0.15 - 10.140 0.18
	// TBD: Try DNS-round-robin server at vi-lb.ccstar.lanl.gov
   // TBD: try HAProxy round-robin at 10.143.0.1

#if 1
   //   snprintf(buff, BUFF_SIZE, "10.140.0.%d:9020", 15 + (rank % 4));
   //   s3_set_proxy(buff);
   //
   //   snprintf(buff, BUFF_SIZE, "10.140.0.%d", 15 + (rank % 4));
   //	s3_set_host(buff);

   snprintf(buff, BUFF_SIZE, "10.140.0.%d:9020", 15 + (rank % 4));
   s3_set_host(buff);

#else
/*
 * If you just want to go to one if the ECS nodes, put that IP
 * address in here directly with port 9020.
 *
 */
//   s3_set_host("10.140.0.15:9020");

/*
 * If you want to go to haproxy.ccstar.lanl.gov, this is its IP
 * address.
 *
 */
//   s3_set_proxy("10.143.0.1:80");
//   s3_set_host( "10.143.0.1:80");
#endif

	// make sure test-bucket exists
	s3_set_bucket((char*)bucket_name);

   if (rank == 0) {
      AWS4C_CHECK( s3_head(param->io_buf, "") );
      if ( param->io_buf->code == 404 ) {					// "404 Not Found"
         printf("  bucket '%s' doesn't exist\n", bucket_name);

         AWS4C_CHECK( s3_put(param->io_buf, "") );	/* creates URL as bucket + obj */
         AWS4C_CHECK_OK(     param->io_buf );		// assure "200 OK"
         printf("created bucket '%s'\n", bucket_name);
      }
      else {														// assure "200 OK"
         AWS4C_CHECK_OK( param->io_buf );
      }
   }
   MPI_CHECK(MPI_Barrier(param->testComm), "barrier error");


	// Maybe allow EMC extensions to S3
	s3_enable_EMC_extensions(param->curl_flags & IOR_CURL_S3_EMC_EXT);

	// don't perform these inits more than once
	param->curl_flags |= IOR_CURL_INIT;


	if (param->verbose >= VERBOSE_2) {
		printf("<- s3_connect  [success]\n");
	}
}

static
void
s3_disconnect( IOR_param_t* param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> s3_disconnect\n");
	}

	// nothing to do here, if using new aws4c ...

	if (param->verbose >= VERBOSE_2) {
		printf("<- s3_disconnect\n");
	}
}



// After finalizing an S3 multi-part-upload, you must reset some things
// before you can use multi-part-upload again.  This will also avoid (one
// particular set of) memory-leaks.
void
s3_MPU_reset(IOR_param_t* param) {
	aws_iobuf_reset(param->io_buf);
	aws_iobuf_reset(param->etags);
	param->part_number = 0;
}


/* ---------------------------------------------------------------------------
 * direct support for the IOR S3 interface
 * ---------------------------------------------------------------------------
 */

/*
 * One doesn't "open" an object, in REST semantics.  All we really care
 * about is whether caller expects the object to have zero-size, when we
 * return.  If so, we conceptually delete it, then recreate it empty.
 *
 * ISSUE: If the object is going to receive "appends" (supported in EMC S3
 *       extensions), the object has to exist before the first append
 *       operation.  On the other hand, there appears to be a bug in the
 *       EMC implementation, such that if an object ever receives appends,
 *       and then is deleted, and then recreated, the recreated object will
 *       always return "500 Server Error" on GET (whether it has been
 *       appended or not).
 *
 *       Therefore, a safer thing to do here is write zero-length contents,
 *       instead of deleting.
 *
 * NOTE: There's also no file-descriptor to return, in REST semantics.  On
 *       the other hand, we keep needing the file *NAME*.  Therefore, we
 *       will return the file-name, and let IOR pass it around to our
 *       functions, in place of what IOR understands to be a
 *       file-descriptor.
 *
 */

static
void *
S3_Create_Or_Open_internal(char*         testFileName,
                           IOR_param_t*  param,
                           unsigned char createFile,
									int           multi_part_upload_p ) {

	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Create_Or_Open('%s', ,%d, %d)\n",
				 testFileName, createFile, multi_part_upload_p);
	}

	/* initialize curl, if needed */
	s3_connect( param );

	/* Check for unsupported flags */
	if ( param->openFlags & IOR_EXCL ) {
		fprintf( stdout, "Opening in Exclusive mode is not implemented in S3\n" );
	}
	if ( param->useO_DIRECT == TRUE ) {
		fprintf( stdout, "Direct I/O mode is not implemented in S3\n" );
	}

	// easier to think
	int n_to_n = param->filePerProc;
	int n_to_1 = ! n_to_n;

	/* check whether object needs reset to zero-length */
	int needs_reset = 0;
	if (! multi_part_upload_p)
		needs_reset = 1;			  /* so "append" can work */
	else if ( param->openFlags & IOR_TRUNC )
		needs_reset = 1;			  /* so "append" can work */
	else if (createFile) {
		// AWS4C_CHECK( s3_head(param->io_buf, testFileName) );
		// if ( ! AWS4C_OK(param->io_buf) )
			needs_reset = 1;
	}

	if ( param->open == WRITE ) {

		/* initializations for N:1 or N:N writes using multi-part upload */
		if (multi_part_upload_p) {

			// For N:N, all ranks do their own MPU open/close.  For N:1, only
			// rank0 does that. Either way, the response from the server
			// includes an "uploadId", which must be used to upload parts to
			// the same object.
			if ( n_to_n || (rank == 0) ) {

				// rank0 handles truncate
				if ( needs_reset) {
					aws_iobuf_reset(param->io_buf);
					AWS4C_CHECK( s3_put(param->io_buf, testFileName) ); /* 0-length write */
					AWS4C_CHECK_OK( param->io_buf );
				}

				// POST request with URL+"?uploads" initiates multi-part upload
				snprintf(buff, BUFF_SIZE, "%s?uploads", testFileName);
				IOBuf* response = aws_iobuf_new();
				AWS4C_CHECK( s3_post2(param->io_buf, buff, NULL, response) );
				AWS4C_CHECK_OK( param->io_buf );

				// parse XML returned from server, into a tree structure
				aws_iobuf_realloc(response);
				xmlDocPtr doc = xmlReadMemory(response->first->buf,
														response->first->len,
														NULL, NULL, 0);
				if (doc == NULL)
					ERR_SIMPLE("Rank0 Failed to find POST response\n");

				// navigate parsed XML-tree to find UploadId
				xmlNode* root_element = xmlDocGetRootElement(doc);
				const char* upload_id = find_element_named(root_element, (char*)"UploadId");
				if (! upload_id)
					ERR_SIMPLE("couldn't find 'UploadId' in returned XML\n");

				if (param->verbose >= VERBOSE_3)
					printf("got UploadId = '%s'\n", upload_id);

				const size_t upload_id_len = strlen(upload_id);
				if (upload_id_len > MAX_UPLOAD_ID_SIZE) {
					snprintf(buff, BUFF_SIZE,
								"UploadId length %d exceeds expected max (%d)",
								upload_id_len, MAX_UPLOAD_ID_SIZE);
					ERR_SIMPLE(buff);
				}

				// save the UploadId we found
				memcpy(param->UploadId, upload_id, upload_id_len);
				param->UploadId[upload_id_len] = 0;

				// free storage for parsed XML tree
				xmlFreeDoc(doc);
				aws_iobuf_free(response);

				// For N:1, share UploadId across all ranks
				if (n_to_1)
					MPI_Bcast(param->UploadId, MAX_UPLOAD_ID_SIZE, MPI_BYTE, 0, param->testComm);
			}
			else
				// N:1, and we're not rank0. recv UploadID from Rank 0
				MPI_Bcast(param->UploadId, MAX_UPLOAD_ID_SIZE, MPI_BYTE, 0, param->testComm);
		}

		/* initializations for N:N or N:1 writes using EMC byte-range extensions */
		else {

			/* maybe reset to zero-length, so "append" can work */
			if (needs_reset) {

            if (verbose >= VERBOSE_3) {
               fprintf( stdout, "rank %d resetting\n",
                        rank);
            }

				aws_iobuf_reset(param->io_buf);
				AWS4C_CHECK( s3_put(param->io_buf, testFileName) );
				AWS4C_CHECK_OK( param->io_buf );
			}
		}
	}


	if (param->verbose >= VERBOSE_2) {
		printf("<- S3_Create_Or_Open\n");
	}
	return ((void *) testFileName );
}



static
void *
S3_Create( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Create\n");
	}

	if (param->verbose >= VERBOSE_2) {
		printf("<- S3_Create\n");
	}
	return S3_Create_Or_Open_internal( testFileName, param, TRUE, TRUE );
}
static
void *
EMC_Create( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> EMC_Create\n");
	}

	if (param->verbose >= VERBOSE_2) {
		printf("<- EMC_Create\n");
	}
	return S3_Create_Or_Open_internal( testFileName, param, TRUE, FALSE );
}






static
void *
S3_Open( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Open\n");
	}

	if ( param->openFlags & IOR_CREAT ) {
		if (param->verbose >= VERBOSE_2) {
			printf("<- S3_Open( ... TRUE)\n");
		}
		return S3_Create_Or_Open_internal( testFileName, param, TRUE, TRUE );
	}
	else {
		if (param->verbose >= VERBOSE_2) {
			printf("<- S3_Open( ... FALSE)\n");
		}
		return S3_Create_Or_Open_internal( testFileName, param, FALSE, TRUE );
	}
}
static
void *
EMC_Open( char *testFileName, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Open\n");
	}

	if ( param->openFlags & IOR_CREAT ) {
		if (param->verbose >= VERBOSE_2) {
			printf("<- EMC_Open( ... TRUE)\n");
		}
		return S3_Create_Or_Open_internal( testFileName, param, TRUE, FALSE );
	}
	else {
		if (param->verbose >= VERBOSE_2) {
			printf("<- EMC_Open( ... FALSE)\n");
		}
		return S3_Create_Or_Open_internal( testFileName, param, FALSE, FALSE );
	}
}


/*
 * transfer (more) data to an object.  <file> is just the obj name.
 *
 * For N:1, param->offset is understood as offset for a given client to
 * write into the "file".  This translates to a byte-range in the HTTP
 * request.  Each write in the N:1 case is treated as a complete "part",
 * so there is no such thing as a partial write.
 *
 * For N:N, when IOR "transfer-size" differs from "block-size", IOR treats
 * Xfer as a partial write (i.e. there are multiple calls to XFER, to write
 * any one of the "N" objects, as a series of "append" operations).  This
 * is not supported in S3/REST.  Therefore, we depend on an EMC extension,
 * in this case.  This EMC extension allows appends using a byte-range
 * header spec of "Range: bytes=-1-".  aws4c now provides
 * s3_enable_EMC_extensions(), to allow this behavior.  If EMC-extensions
 * are not enabled, the aws4c library will generate a run-time error, in
 * this case.
 *
 * Each write-request returns an ETag which is a hash of the data.  (The
 * ETag could also be computed directly, if we wanted.)  We must save the
 * etags for later use by S3_close().
 *
 * WARNING: "Pure" S3 doesn't allow byte-ranges for writes to an object.
 *      Thus, you also can not append to an object.  In the context of IOR,
 *      this causes objects to have only the size of the most-recent write.
 *      Thus, If the IOR "transfer-size" is different from the IOR
 *      "block-size", the files will be smaller than the amount of data
 *      that was written to them.
 *
 *      EMC does support "append" to an object.  In order to allow this,
 *      you must enable the EMC-extensions in the aws4c library, by calling
 *      s3_set_emc_compatibility() with a non-zero argument.
 *
 * NOTE: I don't think REST allows us to read/write an amount other than
 *       the size we request.  Maybe our callback-handlers (above) could
 *       tell us?  For now, this is assuming we only have to send one
 *       request, to transfer any amount of data.  (But see above, re EMC
 *       support for "append".)
 */
/* In the EMC case, instead of Multi-Part Upload we can use HTTP
 * "byte-range" headers to write parts of a single object.  This appears to
 * have several advantages over the S3 MPU spec:
 *
 * (a) no need for a special "open" operation, to capture an "UploadID".
 *     Instead we simply write byte-ranges, and the server-side resolves
 *     any races, producing a single winner.  In the IOR case, there should
 *     be no races, anyhow.
 *
 * (b) individual write operations don't have to refer to an ID, or to
 *     parse and save ETags returned from every write.
 *
 * (c) no need for a special "close" operation, in which all the saved
 *     ETags are gathered at a single rank, placed into XML, and shipped to
 *     the server, to finalize the MPU.  That special close appears to
 *     impose two scaling problems: (1) requires all ETags to be shipped at
 *     the BW available to a single process, (1) requires either that they
 *     all fit into memory of a single process, or be written to disk
 *     (imposes additional BW contraints), or make a more-complex
 *     interaction with a threaded curl writefunction, to present the
 *     appearance of a single thread to curl, whilst allowing streaming
 *     reception of non-local ETags.
 *
 * (d) no constraints on the number or size of individual parts.  (These
 *     exist in the S3 spec, the EMC's impl of the S3 multi-part upload is
 *     also free of these constraints.)
 *
 * Instead, parallel processes can write any number and/or size of updates,
 * using a "byte-range" header.  After each write returns, that part of the
 * global object is visible to any reader.  Places that are not updated
 * read as zeros.
 */


static
IOR_offset_t
S3_Xfer_internal(int          access,
					  void*        file,
					  IOR_size_t*  buffer,
					  IOR_offset_t length,
					  IOR_param_t* param,
					  int          multi_part_upload_p ) {

	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Xfer(acc:%d, target:%s, buf:0x%llx, len:%llu, 0x%llx)\n",
				 access, (char*)file, buffer, length, param);
	}

	char*      fname = (char*)file; /* see NOTE above S3_Create_Or_Open() */
	size_t     remaining = (size_t)length;
	char*      data_ptr = (char *)buffer;
	off_t      offset = param->offset;

	// easier to think
	int        n_to_n    = param->filePerProc;
	int        n_to_1    = (! n_to_n);
	int        segmented = (param->segmentCount == 1);


	if (access == WRITE) {	/* WRITE */

		if (verbose >= VERBOSE_3) {
			fprintf( stdout, "rank %d writing length=%lld to offset %lld\n",
						rank,
                  remaining,
						param->offset + length - remaining);
		}


		if (multi_part_upload_p) {

			// For N:1, part-numbers must have a global ordering for the
			// components of the final object.  param->part_number is
			// incremented by 1 per write, on each rank.  This lets us use it
			// to compute a global part-numbering.
         //
         // In the N:N case, we only need to increment part-numbers within
			// each rank.
         //
         // In the N:1 case, the global order of part-numbers we're writing
         // depends on whether wer're writing strided or segmented, in
         // other words, how <offset> and <remaining> are acutally
         // positioning the parts being written. [See discussion at
         // S3_Close_internal().]
         //
			// NOTE: 's3curl.pl --debug' shows StringToSign having partNumber
			//       first, even if I put uploadId first in the URL.  Maybe
			//       that's what the server will do.  GetStringToSign() in
			//       aws4c is not clever about this, so we spoon-feed args in
			//       the proper order.

			size_t part_number;
			if (n_to_1) {
            if (segmented) {      // segmented
               size_t parts_per_rank = param->blockSize / param->transferSize;
               part_number = (rank * parts_per_rank) + param->part_number;
            }
            else                // strided
               part_number = (param->part_number * param->numTasks) + rank;
         }
         else
				part_number = param->part_number;
         ++ param->part_number;


         //         if (verbose >= VERBOSE_3) {
         //            fprintf( stdout, "rank %d of %d writing (%s,%s) part_number %lld\n",
         //                     rank,
         //                     param->numTasks,
         //                     (n_to_1 ? "N:1" : "N:N"),
         //                     (segmented ? "segmented" : "strided"),
         //                     part_number);
         //         }

			snprintf(buff, BUFF_SIZE,
						"%s?partNumber=%d&uploadId=%s",
						fname, part_number, param->UploadId);

			// For performance, we append <data_ptr> directly into the linked list
			// of data in param->io_buf.  We are "appending" rather than
			// "extending", so the added buffer is seen as written data, rather
			// than empty storage.
			//
			// aws4c parses some header-fields automatically for us (into members
			// of the IOBuf).  After s3_put2(), we can just read the etag from
			// param->io_buf->eTag.  The server actually returns literal
			// quote-marks, at both ends of the string.

			aws_iobuf_reset(param->io_buf);
			aws_iobuf_append_static(param->io_buf, data_ptr, remaining);
			AWS4C_CHECK( s3_put(param->io_buf, buff) );
			AWS4C_CHECK_OK( param->io_buf );

         //			if (verbose >= VERBOSE_3) {
         //				printf("rank %d: read ETag = '%s'\n", rank, param->io_buf->eTag);
         //				if (strlen(param->io_buf->eTag) != ETAG_SIZE+2) { /* quotes at both ends */
         //					fprintf(stderr, "Rank %d: ERROR: expected ETag to be %d hex digits\n",
         //							  rank, ETAG_SIZE);
         //					exit(1);
         //				}
         //			}

         if (verbose >= VERBOSE_3) {
            fprintf( stdout, "rank %d of %d (%s,%s) offset %lld, part# %lld --> ETag %s\n",
                     rank,
                     param->numTasks,
                     (n_to_1 ? "N:1" : "N:N"),
                     (segmented ? "segmented" : "strided"),
                     offset,
                     part_number,
                     param->io_buf->eTag); // incl quote-marks at [0] and [len-1]
         }
         if (strlen(param->io_buf->eTag) != ETAG_SIZE+2) { /* quotes at both ends */
					fprintf(stderr, "Rank %d: ERROR: expected ETag to be %d hex digits\n",
							  rank, ETAG_SIZE);
					exit(1);
         }

			// save the eTag for later
			//
			//		memcpy(etag, param->io_buf->eTag +1, strlen(param->io_buf->eTag) -2);
			//		etag[ETAG_SIZE] = 0;
			aws_iobuf_append(param->etags,
								  param->io_buf->eTag +1,
								  strlen(param->io_buf->eTag) -2);
			// DEBUGGING
			if (verbose >= VERBOSE_4) {
				printf("rank %d: part %d = ETag %s\n", rank, part_number, param->io_buf->eTag);
			}

			// drop ptrs to <data_ptr>, in param->io_buf
			aws_iobuf_reset(param->io_buf);
		}
		else {	 // use EMC's byte-range write-support, instead of MPU


			// NOTE: You must call 's3_enable_EMC_extensions(1)' for
			//       byte-ranges to work for writes.
			if (n_to_n)
				s3_set_byte_range(-1,-1); // EMC header "Range: bytes=-1-" means "append"
			else
				s3_set_byte_range(offset, remaining);

			// For performance, we append <data_ptr> directly into the linked list
			// of data in param->io_buf.  We are "appending" rather than
			// "extending", so the added buffer is seen as written data, rather
			// than empty storage.
			aws_iobuf_reset(param->io_buf);
			aws_iobuf_append_static(param->io_buf, data_ptr, remaining);
			AWS4C_CHECK   ( s3_put(param->io_buf, file) );
			AWS4C_CHECK_OK( param->io_buf );

			// drop ptrs to <data_ptr>, in param->io_buf
			aws_iobuf_reset(param->io_buf);
		}


		if ( param->fsyncPerWrite == TRUE ) {
			WARN("S3 doesn't support 'fsync'" ); /* does it? */
		}

	}
	else {				/* READ or CHECK */

		if (verbose >= VERBOSE_3) {
			fprintf( stdout, "rank %d reading from offset %lld\n",
						rank,
						param->offset + length - remaining );
		}

		// read specific byte-range from the object
      // [This is included in the "pure" S3 spec.]
		s3_set_byte_range(offset, remaining);

		// For performance, we append <data_ptr> directly into the linked
		// list of data in param->io_buf.  In this case (i.e. reading),
		// we're "extending" rather than "appending".  That means the
		// buffer represents empty storage, which will be filled by the
		// libcurl writefunction, invoked via aws4c.
		aws_iobuf_reset(param->io_buf);
		aws_iobuf_extend_static(param->io_buf, data_ptr, remaining);
		AWS4C_CHECK( s3_get(param->io_buf, file) );
		if (param->io_buf->code != 206) { /* '206 Partial Content' */
			snprintf(buff, BUFF_SIZE,
						"Unexpected result (%d, '%s')",
						param->io_buf->code, param->io_buf->result);
			ERR_SIMPLE(buff);
		}

		// drop refs to <data_ptr>, in param->io_buf
		aws_iobuf_reset(param->io_buf);
	}


	if (param->verbose >= VERBOSE_2) {
		printf("<- S3_Xfer\n");
	}
	return ( length );
}


static
IOR_offset_t
S3_Xfer(int          access,
		  void*        file,
		  IOR_size_t*  buffer,
		  IOR_offset_t length,
		  IOR_param_t* param ) {
	S3_Xfer_internal(access, file, buffer, length, param, TRUE);
}
static
IOR_offset_t
EMC_Xfer(int          access,
		  void*        file,
		  IOR_size_t*  buffer,
		  IOR_offset_t length,
		  IOR_param_t* param ) {
	S3_Xfer_internal(access, file, buffer, length, param, FALSE);
}





/*
 * Does this even mean anything, for HTTP/S3 ?
 *
 * I believe all interactions with the server are considered complete at
 * the time we get a response, e.g. from s3_put().  Therefore, fsync is
 * kind of meaningless, for REST/S3.
 *
 * In future, we could extend our interface so as to allow a non-blocking
 * semantics, for example with the libcurl "multi" interface, and/or by
 * adding threaded callback handlers to obj_put().  *IF* we do that, *THEN*
 * we should revisit 'fsync'.
 *
 * Another special case is multi-part upload, where many parallel clients
 * may be writing to the same "file".  (It looks like param->filePerProc
 * would be the flag to check, for this.)  Maybe when you called 'fsync',
 * you meant that you wanted *all* the clients to be complete?  That's not
 * really what fsync would do.  In the N:1 case, this is accomplished by
 * S3_Close().  If you really wanted this behavior from S3_Fsync, we could
 * have S3_Fsync call S3_close.
 *
 * As explained above, we may eventually want to consider the following:
 *
 *      (1) thread interaction with any handlers that are doing ongoing
 *      interactions with the socket, to make sure they have finished all
 *      actions and gotten responses.
 *
 *      (2) MPI barrier for all clients involved in a multi-part upload.
 *      Presumably, for IOR, when we are doing N:1, all clients are
 *      involved in that transfer, so this would amount to a barrier on
 *      MPI_COMM_WORLD.
 */

static
void
S3_Fsync( void *fd, IOR_param_t * param ) {
	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Fsync  [no-op]\n");
	}

	if (param->verbose >= VERBOSE_2) {
		printf("<- S3_Fsync\n");
	}
}


/*
 * It seems the only kind of "close" that ever needs doing for S3 is in the
 * case of multi-part upload (i.e. N:1).  In this case, all the parties to
 * the upload must provide their ETags to a single party (e.g. rank 0 in an
 * MPI job).  Then the rank doing the closing can generate XML and complete
 * the upload.
 *
 * ISSUE: The S3 spec says that a multi-part upload can have at most 10,000
 *        parts.  Does EMC allow more than this?  (NOTE the spec also says
 *        parts must be at leaast 5MB, but EMC definitely allows smaller
 *        parts than that.)
 *
 * ISSUE: All Etags must be sent from a single rank, in a single
 *        transaction.  If the issue above (regarding 10k Etags) is
 *        resolved by a discovery that EMC supports more than 10k ETags,
 *        then, for large-enough files (or small-enough transfer-sizes) an
 *        N:1 write may generate more ETags than the single closing rank
 *        can hold in memory.  In this case, there are several options,
 *        outlined
 *
 *

 * See S3_Fsync() for some possible considerations.
 */

static
void
S3_Close_internal( void*         fd,
						 IOR_param_t*  param,
						 int           multi_part_upload_p ) {

	char* fname = (char*)fd; /* see NOTE above S3_Create_Or_Open() */

	// easier to think
	int n_to_n    = param->filePerProc;
	int n_to_1    = (! n_to_n);
   int segmented = (param->segmentCount == 1);

	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Close('%s', ,%d) %s\n",
				 fname,
             multi_part_upload_p,
             ((n_to_n) ? "N:N" : ((segmented) ? "N:1(seg)" : "N:1(str)")));
	}

	if (param->open == WRITE) {


		// finalizing Multi-Part Upload (for N:1 or N:N)
		if (multi_part_upload_p) {


			size_t etag_data_size = param->etags->write_count; /* local ETag data (bytes) */
			size_t etags_per_rank = etag_data_size / ETAG_SIZE;		/* number of local etags */

			// --- create XML containing ETags in an IOBuf for "close" request
			IOBuf* xml = NULL;


			if (n_to_1) {

				// for N:1, gather all Etags at Rank0
				MPI_Datatype mpi_size_t;
				if (sizeof(size_t) == sizeof(int))
					mpi_size_t = MPI_INT;
				else if (sizeof(size_t) == sizeof(long))
					mpi_size_t = MPI_LONG;
				else
					mpi_size_t = MPI_LONG_LONG;

				// Everybody should have the same number of ETags (?)
				size_t etag_count_max = 0;		 /* highest number on any proc */
				MPI_Allreduce(&etags_per_rank, &etag_count_max,
								  1, mpi_size_t, MPI_MAX, param->testComm);
				if (etags_per_rank != etag_count_max) {
					printf("Rank %d: etag count mismatch: max:%d, mine:%d\n",
							 rank, etag_count_max, etags_per_rank);
					MPI_Abort(param->testComm, 1);
				}

				// collect ETag data at Rank0
				aws_iobuf_realloc(param->etags);             /* force single contiguous buffer */
				char* etag_data = param->etags->first->buf;  /* per-rank data, contiguous */

				if (rank == 0)  {
					char* etag_ptr;
					int   i;
					int   j;
					int   rnk;

					char* etag_vec = (char*)malloc((param->numTasks * etag_data_size) +1);
					if (! etag_vec) {
						fprintf(stderr, "rank 0 failed to malloc %d bytes\n",
								  param->numTasks * etag_data_size);
						MPI_Abort(param->testComm, 1);
					}
					MPI_Gather(etag_data, etag_data_size, MPI_BYTE,
								  etag_vec,  etag_data_size, MPI_BYTE, 0, MPI_COMM_WORLD);

					// --- debugging: show the gathered etag data
					//     (This shows the raw concatenated etag-data from each node.)
					if (param->verbose >= VERBOSE_4) {

						printf("rank 0: gathered %d etags from all ranks:\n", etags_per_rank);
						etag_ptr=etag_vec;
						for (rnk=0; rnk<param->numTasks; ++rnk) {
							printf("\t[%d]: '", rnk);

							int ii;
							for (ii=0; ii<etag_data_size; ++ii)	/* NOT null-terminated! */
								printf("%c", etag_ptr[ii]);

							printf("'\n");
							etag_ptr += etag_data_size;
						}
					}


					// add XML for *all* the parts.  The XML must be ordered by
					// part-number.  Each rank wrote <etags_per_rank> parts,
					// locally.  At rank0, the etags for each rank are now
					// stored as a continguous block of text, with the blocks
					// stored in rank order in etag_vec.  In other words, our
					// internal rep at rank 0 matches the "segmented" format.
					// From this, we must select etags in an order matching how
					// they appear in the actual object, and give sequential
					// part-numbers to the resulting sequence.
					//
					// That ordering of parts in the actual written object
					// varies according to whether we wrote in the "segmented"
					// or "strided" format.
					//
					//     supposing N ranks, and P parts per rank:
					//
					// segmented:
					//
					//     all parts for a given rank are consecutive.
					//     rank r writes these parts:
					//
					//     rP, rP+1, ... (r+1)P -1
					//
					//     i.e. rank0 writes parts 0,1,2,3 ... P-1
					//
					//
					// strided:
					//
					//     rank r writes every P-th part, starting with r.
					//
					//     r, P+r, ... (P-1)P + r
					//
					//     i.e. rank0 writes parts 0,P,2P,3P ... (P-1)P
					//
					//
					// NOTE: If we knew ahead of time how many parts each rank was
					//       going to write, we could assign part-number ranges, per
					//       rank, and then have nice locality here.
					//
					//       Alternatively, we could have everyone format their own
					//       XML text and send that, instead of just the tags.  This
					//       would increase the amount of data being sent, but would
					//       reduce the work for rank0 to format everything.

               size_t  i_max;            // outer-loop
               size_t  j_max;            // inner loop
					size_t  start_multiplier; // initial offset in collected data
					size_t  stride;           // in etag_vec

					if (segmented) {          // segmented
                  i_max            = param->numTasks;
                  j_max            = etags_per_rank;
						start_multiplier = etag_data_size;		/* one rank's-worth of Etag data */
						stride           = ETAG_SIZE;				/* one ETag */
					}
					else {                    // strided
                  i_max            = etags_per_rank;
                  j_max            = param->numTasks;
						start_multiplier = ETAG_SIZE;				/* one ETag */
						stride           = etag_data_size;		/* one rank's-worth of Etag data */
					}


					xml = aws_iobuf_new();
					aws_iobuf_growth_size(xml, 1024 * 8);

					// write XML header ...
					aws_iobuf_append_str(xml, "<CompleteMultipartUpload>\n");

					int part = 0;
					for (i=0; i<i_max; ++i) {

						etag_ptr=etag_vec + (i * start_multiplier);

						for (j=0; j<j_max; ++j) {

							// etags were saved as contiguous text.  Extract the next one.
							char etag[ETAG_SIZE +1];
							memcpy(etag, etag_ptr, ETAG_SIZE);
							etag[ETAG_SIZE] = 0;

							// write XML for next part, with Etag ...
							snprintf(buff, BUFF_SIZE,
										"  <Part>\n"
										"    <PartNumber>%d</PartNumber>\n"
										"    <ETag>%s</ETag>\n"
										"  </Part>\n",
										part, etag);

							aws_iobuf_append_str(xml, buff);

							etag_ptr += stride;
							++ part;
						}
					}

					// write XML tail ...
					aws_iobuf_append_str(xml, "</CompleteMultipartUpload>\n");
				}

				else {
					MPI_Gather(etag_data, etag_data_size, MPI_BYTE,
								  NULL,      etag_data_size, MPI_BYTE, 0, MPI_COMM_WORLD);
				}
			}

			else {   /* N:N */

				xml = aws_iobuf_new();
				aws_iobuf_growth_size(xml, 1024 * 8);

				// write XML header ...
				aws_iobuf_append_str(xml, "<CompleteMultipartUpload>\n");

				// all parts of our object were written from this rank.
				char etag[ETAG_SIZE +1];
				int  part = 0;
				int  i;
				for (i=0; i<etags_per_rank; ++i) {

					// TBD: Instead of reading into etag, then sprintf'ing, then
					// copying into xml, we could just read directly into xml
					int sz = aws_iobuf_get_raw(param->etags, etag, ETAG_SIZE);
					if (sz != ETAG_SIZE) {
						snprintf(buff, BUFF_SIZE,
									"Read of ETag %d had length %d (not %d)\n",
									rank, i, sz, ETAG_SIZE);
						ERR_SIMPLE(buff);
					}
					etag[ETAG_SIZE] = 0;


					// write XML for next part, with Etag ...
					snprintf(buff, BUFF_SIZE,
								"  <Part>\n"
								"    <PartNumber>%d</PartNumber>\n"
								"    <ETag>%s</ETag>\n"
								"  </Part>\n",
								part, etag);

					aws_iobuf_append_str(xml, buff);

					++ part;
				}

				// write XML tail ...
				aws_iobuf_append_str(xml, "</CompleteMultipartUpload>\n");
			}



			// send request to finalize MPU
			if (n_to_n || (rank == 0)) {

				// DEBUGGING: show the XML we constructed
				if (param->verbose >= VERBOSE_3)
					debug_iobuf(xml, 1, 1);

				// --- POST our XML to the server.
				snprintf(buff, BUFF_SIZE,
							"%s?uploadId=%s",
							fname, param->UploadId);

				AWS4C_CHECK   ( s3_post(xml, buff) );
				AWS4C_CHECK_OK( xml );

				aws_iobuf_free(xml);
			}


			// everybody reset MPU info.  Allows another MPU, and frees memory.
			s3_MPU_reset(param);

			// Everybody meetup, so non-zero ranks won't go trying to stat the
			// N:1 file until rank0 has finished the S3 multi-part finalize.
			// The object will not appear to exist, until then.
			if (n_to_1)
				MPI_CHECK(MPI_Barrier(param->testComm), "barrier error");
		}
		else {

			// No finalization is needed, when using EMC's byte-range writing
         // support.  However, we do need to make sure everyone has
         // finished writing, before anyone starts reading.
			if (n_to_1) {
            MPI_CHECK(MPI_Barrier(param->testComm), "barrier error");
				if (param->verbose >= VERBOSE_2)
               printf("rank %d: passed barrier\n", rank);
         }
		}

		// After writing, reset the CURL connection, so that caches won't be
		// used for reads.
		aws_reset_connection();
	}


	if (param->verbose >= VERBOSE_2) {
		printf("<- S3_Close\n");
	}
}

static
void
S3_Close( void*         fd,
			 IOR_param_t*  param ) {
	S3_Close_internal(fd, param, TRUE);
}
static
void
EMC_Close( void*         fd,
			  IOR_param_t*  param ) {
	S3_Close_internal(fd, param, FALSE);
}




/*
 * Delete an object through the S3 interface.
 *
 * The only reason we separate out EMC version, is because EMC bug means a
 * file that was written with appends can't be deleted, recreated, and then
 * successfully read.
 */

static
void
S3_Delete( char *testFileName, IOR_param_t * param ) {

	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_Delete(%s)\n", testFileName);
	}

	/* maybe initialize curl */
	s3_connect( param );

#if 0
	// EMC BUG: If file was written with appends, and is deleted,
	//      Then any future recreation will result in an object that can't be read.
	//      this
	AWS4C_CHECK( s3_delete(param->io_buf, testFileName) );
#else
	// just replace with a zero-length object for now
	aws_iobuf_reset(param->io_buf);
	AWS4C_CHECK   ( s3_put(param->io_buf, testFileName) );
#endif

	AWS4C_CHECK_OK( param->io_buf );

	if (param->verbose >= VERBOSE_2)
		printf("<- S3_Delete\n");
}


static
void
EMC_Delete( char *testFileName, IOR_param_t * param ) {

	if (param->verbose >= VERBOSE_2) {
		printf("-> EMC_Delete(%s)\n", testFileName);
	}

	/* maybe initialize curl */
	s3_connect( param );

#if 0
	// EMC BUG: If file was written with appends, and is deleted,
	//      Then any future recreation will result in an object that can't be read.
	//      this
	AWS4C_CHECK( s3_delete(param->io_buf, testFileName) );
#else
	// just replace with a zero-length object for now
	aws_iobuf_reset(param->io_buf);
	AWS4C_CHECK   ( s3_put(param->io_buf, testFileName) );
#endif

	AWS4C_CHECK_OK( param->io_buf );

	if (param->verbose >= VERBOSE_2)
		printf("<- EMC_Delete\n");
}






/*
 * HTTP HEAD returns meta-data for a "file".
 *
 * QUESTION: What should the <size> parameter be, on a HEAD request?  Does
 * it matter?  We don't know how much data they are going to send, but
 * obj_get_callback protects us from overruns.  Will someone complain if we
 * request more data than the header actually takes?
 */

static
IOR_offset_t
S3_GetFileSize(IOR_param_t * param,
			   MPI_Comm      testComm,
			   char *        testFileName) {

	if (param->verbose >= VERBOSE_2) {
		printf("-> S3_GetFileSize(%s)\n", testFileName);
	}

	IOR_offset_t aggFileSizeFromStat; /* i.e. "long long int" */
	IOR_offset_t tmpMin, tmpMax, tmpSum;


	/* make sure curl is connected, and inits are done */
	s3_connect( param );

	/* send HEAD request.  aws4c parses some headers into IOBuf arg. */
	AWS4C_CHECK( s3_head(param->io_buf, testFileName) );
	if ( ! AWS4C_OK(param->io_buf) ) {
		fprintf(stderr, "rank %d: couldn't stat '%s': %s\n",
				  rank, testFileName, param->io_buf->result);
		MPI_Abort(param->testComm, 1);
	}
	aggFileSizeFromStat = param->io_buf->contentLen;

   if (param->verbose >= VERBOSE_2) {
      printf("\trank %d: file-size %llu\n", rank, aggFileSizeFromStat);
   }

	if ( param->filePerProc == TRUE ) {
		if (param->verbose >= VERBOSE_2) {
			printf("\tall-reduce (1)\n");
		}
		MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat,
		                        &tmpSum,             /* sum */
		                        1,
		                        MPI_LONG_LONG_INT,
		                        MPI_SUM,
		                        testComm ),
		          "cannot total data moved" );

		aggFileSizeFromStat = tmpSum;
	}
	else {
		if (param->verbose >= VERBOSE_2) {
			printf("\tall-reduce (2a)\n");
		}
		MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat,
		                        &tmpMin,             /* min */
		                        1,
		                        MPI_LONG_LONG_INT,
		                        MPI_MIN,
		                        testComm ),
		          "cannot total data moved" );

		if (param->verbose >= VERBOSE_2) {
			printf("\tall-reduce (2b)\n");
		}
		MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat,
		                        &tmpMax,             /* max */
		                        1,
		                        MPI_LONG_LONG_INT,
		                        MPI_MAX,
		                        testComm ),
		          "cannot total data moved" );

		if ( tmpMin != tmpMax ) {
			if ( rank == 0 ) {
				WARN( "inconsistent file size by different tasks" );
			}

			/* incorrect, but now consistent across tasks */
			aggFileSizeFromStat = tmpMin;
		}
	}

	if (param->verbose >= VERBOSE_2) {
		printf("<- S3_GetFileSize [%llu]\n", aggFileSizeFromStat);
	}
	return ( aggFileSizeFromStat );
}
