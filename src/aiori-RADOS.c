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

static void *RADOS_Create_Or_Open(char *testFileName, IOR_param_t * param, int create_flag)
{
        int ret;
        char *oid;

        oid = strdup(testFileName);
        if (!oid)
                ERR("unable to allocate RADOS oid");

        /* create RADOS cluster handle */
        /* XXX: HARDCODED RADOS USER NAME */
        ret = rados_create(&param->rados_cluster, "admin");
        if (ret)
                ERR("unable to create RADOS cluster handle");

        /* set the handle using the Ceph config */
        /* XXX: HARDCODED RADOS CONF PATH */
        ret = rados_conf_read_file(param->rados_cluster, "/etc/ceph/ceph.conf");
        if (ret)
                ERR("unable to read RADOS config file");

        /* connect to the RADOS cluster */
        ret = rados_connect(param->rados_cluster);
        if (ret)
                ERR("unable to connect to the RADOS cluster");

        /* create an io context for the pool we are operating on */
        /* XXX: HARDCODED RADOS POOL NAME */
        ret = rados_ioctx_create(param->rados_cluster, "cephfs_data",
                                 &param->rados_ioctx);
        if (ret)
                ERR("unable to create an I/O context for the RADOS cluster");

        /* if create flag is given, create the object */
        if (create_flag)
        {
                rados_write_op_t create_op;

                /* create a RADOS "write op" for creating the ojbect */
                create_op = rados_create_write_op();
                rados_write_op_create(create_op, LIBRADOS_CREATE_EXCLUSIVE, NULL);
                ret = rados_write_op_operate(create_op, param->rados_ioctx, oid,
                                       NULL, 0);
                rados_release_write_op(create_op);
                if (ret)
                        ERR("unable to create RADOS object");
        }
        else
        {
                /* XXX actually, we should probably assert oid existence here? */
        }

        return (void *)oid;
}

static void *RADOS_Create(char *testFileName, IOR_param_t * param)
{
        return RADOS_Create_Or_Open(testFileName, param, TRUE);
}

static void *RADOS_Open(char *testFileName, IOR_param_t * param)
{
        if (param->openFlags & IOR_CREAT)
                return RADOS_Create_Or_Open(testFileName, param, TRUE);
        else
                return RADOS_Create_Or_Open(testFileName, param, FALSE);
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
        /* object does not need to be "closed", but we should tear the cluster down */

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
