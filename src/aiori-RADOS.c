/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
* (C) 2015 The University of Chicago                                           *
*                                                                              *
* See COPYRIGHT in top-level directory.                                        *
*                                                                              *
********************************************************************************
*
* Implement abstract I/O interface for RADOS.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <rados/librados.h>

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"

/**************************** P R O T O T Y P E S *****************************/
static void *RADOS_Create(char *, IOR_param_t *);
static void *RADOS_Open(char *, IOR_param_t *);
static IOR_offset_t RADOS_Xfer(int, void *, IOR_size_t *,
                           IOR_offset_t, IOR_param_t *);
static void RADOS_Close(void *, IOR_param_t *);
static void RADOS_Delete(char *, IOR_param_t *);
static void RADOS_SetVersion(IOR_param_t *);
static void RADOS_Fsync(void *, IOR_param_t *);
static IOR_offset_t RADOS_GetFileSize(IOR_param_t *, MPI_Comm, char *);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t rados_aiori = {
        .name = "RADOS",
        .create = RADOS_Create,
        .open = RADOS_Open,
        .xfer = RADOS_Xfer,
        .close = RADOS_Close,
        .delete = RADOS_Delete,
        .set_version = RADOS_SetVersion,
        .fsync = RADOS_Fsync,
        .get_file_size = RADOS_GetFileSize,
};

/***************************** F U N C T I O N S ******************************/

static void *RADOS_Create(char *testFileName, IOR_param_t * param)
{
        /* connect */
        //rados_connect();

        /* create */

        return NULL;
}

static void *RADOS_Open(char *testFileName, IOR_param_t * param)
{
        return RADOS_Create(testFileName, param);
}

static IOR_offset_t RADOS_Xfer(int access, void *fd, IOR_size_t * buffer,
                              IOR_offset_t length, IOR_param_t * param)
{
        return 0;
}

static void RADOS_Fsync(void *fd, IOR_param_t * param)
{
        return;
}

static void RADOS_Close(void *fd, IOR_param_t * param)
{
        return;
}

static void RADOS_Delete(char *testFileName, IOR_param_t * param)
{
        return;
}

static void RADOS_SetVersion(IOR_param_t * test)
{
        return;
}

static IOR_offset_t RADOS_GetFileSize(IOR_param_t * test, MPI_Comm testComm,
                                      char *testFileName)
{
        return 0;
}
