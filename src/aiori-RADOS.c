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

/************************** O P T I O N S *****************************/
struct rados_options{
  char * user;
  char * conf;
  char * pool;
};

static struct rados_options o = {
  .user = NULL,
  .conf = NULL,
  .pool = NULL,
};

static option_help options [] = {
      {0, "rados.user", "Username for the RADOS cluster", OPTION_REQUIRED_ARGUMENT, 's', & o.user},
      {0, "rados.conf", "Config file for the RADOS cluster", OPTION_REQUIRED_ARGUMENT, 's', & o.conf},
      {0, "rados.pool", "RADOS pool to use for I/O", OPTION_REQUIRED_ARGUMENT, 's', & o.pool},
      LAST_OPTION
};


/**************************** P R O T O T Y P E S *****************************/
static void *RADOS_Create(char *, IOR_param_t *);
static void *RADOS_Open(char *, IOR_param_t *);
static IOR_offset_t RADOS_Xfer(int, void *, IOR_size_t *,
                           IOR_offset_t, IOR_param_t *);
static void RADOS_Close(void *, IOR_param_t *);
static void RADOS_Delete(char *, IOR_param_t *);
static void RADOS_Fsync(void *, IOR_param_t *);
static IOR_offset_t RADOS_GetFileSize(IOR_param_t *, MPI_Comm, char *);
static int RADOS_StatFS(const char *, ior_aiori_statfs_t *, IOR_param_t *);
static int RADOS_MkDir(const char *, mode_t, IOR_param_t *);
static int RADOS_RmDir(const char *, IOR_param_t *);
static int RADOS_Access(const char *, int, IOR_param_t *);
static int RADOS_Stat(const char *, struct stat *, IOR_param_t *);
static option_help * RADOS_options();

/************************** D E C L A R A T I O N S ***************************/
ior_aiori_t rados_aiori = {
        .name = "RADOS",
        .name_legacy = NULL,
        .create = RADOS_Create,
        .open = RADOS_Open,
        .xfer = RADOS_Xfer,
        .close = RADOS_Close,
        .delete = RADOS_Delete,
        .get_version = aiori_get_version,
        .fsync = RADOS_Fsync,
        .get_file_size = RADOS_GetFileSize,
        .statfs = RADOS_StatFS,
        .mkdir = RADOS_MkDir,
        .rmdir = RADOS_RmDir,
        .access = RADOS_Access,
        .stat = RADOS_Stat,
        .get_options = RADOS_options,
};

#define RADOS_ERR(__err_str, __ret) do { \
        errno = -__ret; \
        ERR(__err_str); \
} while(0)

/***************************** F U N C T I O N S ******************************/
static option_help * RADOS_options(){
  return options;
}

static void RADOS_Cluster_Init(IOR_param_t * param)
{
        int ret;

        /* create RADOS cluster handle */
        ret = rados_create(&param->rados_cluster, o.user);
        if (ret)
                RADOS_ERR("unable to create RADOS cluster handle", ret);

        /* set the handle using the Ceph config */
        ret = rados_conf_read_file(param->rados_cluster, o.conf);
        if (ret)
                RADOS_ERR("unable to read RADOS config file", ret);

        /* connect to the RADOS cluster */
        ret = rados_connect(param->rados_cluster);
        if (ret)
                RADOS_ERR("unable to connect to the RADOS cluster", ret);

        /* create an io context for the pool we are operating on */
        ret = rados_ioctx_create(param->rados_cluster, o.pool, &param->rados_ioctx);
        if (ret)
                RADOS_ERR("unable to create an I/O context for the RADOS cluster", ret);

        return;
}

static void RADOS_Cluster_Finalize(IOR_param_t * param)
{
        /* ioctx destroy */
        rados_ioctx_destroy(param->rados_ioctx);

        /* shutdown */
        rados_shutdown(param->rados_cluster);
}

static void *RADOS_Create_Or_Open(char *testFileName, IOR_param_t * param, int create_flag)
{
        int ret;
        char *oid;

        RADOS_Cluster_Init(param);

        if (param->useO_DIRECT == TRUE)
                WARN("direct I/O mode is not implemented in RADOS\n");

        oid = strdup(testFileName);
        if (!oid)
                ERR("unable to allocate RADOS oid");

        if (create_flag)
        {
                rados_write_op_t create_op;
                int rados_create_flag;

                if (param->openFlags & IOR_EXCL)
                        rados_create_flag = LIBRADOS_CREATE_EXCLUSIVE;
                else
                        rados_create_flag = LIBRADOS_CREATE_IDEMPOTENT;

                /* create a RADOS "write op" for creating the object */
                create_op = rados_create_write_op();
                rados_write_op_create(create_op, rados_create_flag, NULL);
                ret = rados_write_op_operate(create_op, param->rados_ioctx, oid,
                                       NULL, 0);
                rados_release_write_op(create_op);
                if (ret)
                        RADOS_ERR("unable to create RADOS object", ret);
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
        int ret;
        char *oid = (char *)fd;

        if (access == WRITE)
        {
                rados_write_op_t write_op;

                write_op = rados_create_write_op();
                rados_write_op_write(write_op, (const char *)buffer,
                                     length, param->offset);
                ret = rados_write_op_operate(write_op, param->rados_ioctx,
                                             oid, NULL, 0);
                rados_release_write_op(write_op);
                if (ret)
                        RADOS_ERR("unable to write RADOS object", ret);
        }
        else /* READ */
        {
                int read_ret;
                size_t bytes_read;
                rados_read_op_t read_op;

                read_op = rados_create_read_op();
                rados_read_op_read(read_op, param->offset, length, (char *)buffer,
                                   &bytes_read, &read_ret);
                ret = rados_read_op_operate(read_op, param->rados_ioctx, oid, 0);
                rados_release_read_op(read_op);
                if (ret || read_ret || ((IOR_offset_t)bytes_read != length))
                        RADOS_ERR("unable to read RADOS object", ret);
        }

        return length;
}

static void RADOS_Fsync(void *fd, IOR_param_t * param)
{
        return;
}

static void RADOS_Close(void *fd, IOR_param_t * param)
{
        char *oid = (char *)fd;

        /* object does not need to be "closed", but we should tear the cluster down */
        RADOS_Cluster_Finalize(param);
        free(oid);

        return;
}

static void RADOS_Delete(char *testFileName, IOR_param_t * param)
{
        int ret;
        char *oid = testFileName;
        rados_write_op_t remove_op;

        /* we have to reestablish cluster connection here... */
        RADOS_Cluster_Init(param);

        /* remove the object */
        remove_op = rados_create_write_op();
        rados_write_op_remove(remove_op);
        ret = rados_write_op_operate(remove_op, param->rados_ioctx,
                                     oid, NULL, 0);
        rados_release_write_op(remove_op);
        if (ret)
                RADOS_ERR("unable to remove RADOS object", ret);

        RADOS_Cluster_Finalize(param);

        return;
}

static IOR_offset_t RADOS_GetFileSize(IOR_param_t * test, MPI_Comm testComm,
                                      char *testFileName)
{
        int ret;
        char *oid = testFileName;
        rados_read_op_t stat_op;
        uint64_t oid_size;
        int stat_ret;
        IOR_offset_t aggSizeFromStat, tmpMin, tmpMax, tmpSum;

        /* we have to reestablish cluster connection here... */
        RADOS_Cluster_Init(test);

        /* stat the object */
        stat_op = rados_create_read_op();
        rados_read_op_stat(stat_op, &oid_size, NULL, &stat_ret);
        ret = rados_read_op_operate(stat_op, test->rados_ioctx, oid, 0);
        rados_release_read_op(stat_op);
        if (ret || stat_ret)
                RADOS_ERR("unable to stat RADOS object", stat_ret);
        aggSizeFromStat = oid_size;

        if (test->filePerProc == TRUE)
        {
                MPI_CHECK(MPI_Allreduce(&aggSizeFromStat, &tmpSum, 1,
                                        MPI_LONG_LONG_INT, MPI_SUM, testComm),
                          "cannot total data moved");
                aggSizeFromStat = tmpSum;
        }
        else
        {
                MPI_CHECK(MPI_Allreduce(&aggSizeFromStat, &tmpMin, 1,
                                        MPI_LONG_LONG_INT, MPI_MIN, testComm),
                          "cannot total data moved");
                MPI_CHECK(MPI_Allreduce(&aggSizeFromStat, &tmpMax, 1,
                                        MPI_LONG_LONG_INT, MPI_MAX, testComm),
                          "cannot total data moved");
                if (tmpMin != tmpMax)
                {
                        if (rank == 0)
                                WARN("inconsistent file size by different tasks");

                        /* incorrect, but now consistent across tasks */
                        aggSizeFromStat = tmpMin;
                }
        }

        RADOS_Cluster_Finalize(test);

        return aggSizeFromStat;
}

static int RADOS_StatFS(const char *oid, ior_aiori_statfs_t *stat_buf,
                        IOR_param_t *param)
{
        WARN("statfs not supported in RADOS backend!");
        return -1;
}

static int RADOS_MkDir(const char *oid, mode_t mode, IOR_param_t *param)
{
        WARN("mkdir not supported in RADOS backend!");
        return -1;
}

static int RADOS_RmDir(const char *oid, IOR_param_t *param)
{
        WARN("rmdir not supported in RADOS backend!");
        return -1;
}

static int RADOS_Access(const char *oid, int mode, IOR_param_t *param)
{
        rados_read_op_t read_op;
        int ret;
        int prval;
        uint64_t oid_size;

        /* we have to reestablish cluster connection here... */
        RADOS_Cluster_Init(param);

        /* use read_op stat to check for oid existence */
        read_op = rados_create_read_op();
        rados_read_op_stat(read_op, &oid_size, NULL, &prval);
        ret = rados_read_op_operate(read_op, param->rados_ioctx, oid, 0);
        rados_release_read_op(read_op);

        RADOS_Cluster_Finalize(param);

        if (ret | prval)
                return -1;
        else
                return 0;
}

static int RADOS_Stat(const char *oid, struct stat *buf, IOR_param_t *param)
{
        WARN("stat not supported in RADOS backend!");
        return -1;
}
