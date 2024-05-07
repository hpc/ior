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
#include <errno.h>
#include <rados/librados.h>

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"

#define RADOS_ERR(__err_str, __ret) do { \
        errno = -__ret; \
        ERR(__err_str); \
} while(0)

/**************************** P R O T O T Y P E S *****************************/

static void RADOS_Initialize(aiori_mod_opt_t *);
static void RADOS_Finalize(aiori_mod_opt_t *);
static aiori_fd_t *RADOS_Create(char *, int flags, aiori_mod_opt_t *);
static aiori_fd_t *RADOS_Open(char *, int flags, aiori_mod_opt_t *);
static IOR_offset_t RADOS_Xfer(int, aiori_fd_t *, IOR_size_t *,
                           IOR_offset_t, IOR_offset_t, aiori_mod_opt_t *);
static void RADOS_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void RADOS_Delete(char *, aiori_mod_opt_t *);
static void RADOS_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static IOR_offset_t RADOS_GetFileSize(aiori_mod_opt_t *, char *);
static int RADOS_StatFS(const char *, ior_aiori_statfs_t *, aiori_mod_opt_t *);
static int RADOS_MkDir(const char *, mode_t, aiori_mod_opt_t *);
static int RADOS_RmDir(const char *, aiori_mod_opt_t *);
static int RADOS_Access(const char *, int, aiori_mod_opt_t *);
static int RADOS_Stat(const char *, struct stat *, aiori_mod_opt_t *);
static int RADOS_check_params(aiori_mod_opt_t * options);

/************************** O P T I O N S *****************************/
typedef struct {
  char * user;
  char * conf;
  char * pool;
} RADOS_options_t;
/***************************** F U N C T I O N S ******************************/

static option_help * RADOS_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  RADOS_options_t * o = malloc(sizeof(RADOS_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(RADOS_options_t));
  }else{
    memset(o, 0, sizeof(RADOS_options_t));
    /* initialize the options properly */
    o->user = NULL;
    o->conf = NULL;
    o->pool = NULL;
  }

  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
    {0, "rados.user", "Username for the RADOS cluster", OPTION_OPTIONAL_ARGUMENT, 's', & o->user},
    {0, "rados.conf", "Config file for the RADOS cluster", OPTION_OPTIONAL_ARGUMENT, 's', & o->conf},
    {0, "rados.pool", "RADOS pool to use for I/O", OPTION_OPTIONAL_ARGUMENT, 's', & o->pool},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

/************************** D E C L A R A T I O N S ***************************/
ior_aiori_t rados_aiori = {
        .name = "RADOS",
        .name_legacy = NULL,
        .initialize = RADOS_Initialize,
        .finalize = RADOS_Finalize,
        .create = RADOS_Create,
        .open = RADOS_Open,
        .xfer = RADOS_Xfer,
        .close = RADOS_Close,
        .remove = RADOS_Delete,
        .get_version = aiori_get_version,
        .fsync = RADOS_Fsync,
        .get_file_size = RADOS_GetFileSize,
        .statfs = RADOS_StatFS,
        .mkdir = RADOS_MkDir,
        .rmdir = RADOS_RmDir,
        .access = RADOS_Access,
        .stat = RADOS_Stat,
        .get_options = RADOS_options,
        .check_params = RADOS_check_params
};

static rados_t       rados_cluster;     /* RADOS cluster handle */
static rados_ioctx_t rados_ioctx;       /* I/O context for our pool in the RADOS cluster */

/***************************** F U N C T I O N S ******************************/

static int RADOS_check_params(aiori_mod_opt_t * options){
  RADOS_options_t *o = (RADOS_options_t*) options;
  if (!(o->user))
      ERR("RADOS user must be specified");
  if (!(o->conf))
      ERR("RADOS conf must be specified");
  if (!(o->pool))
      ERR("RADOS pool must be specified");
  return 0;
}

static void RADOS_Initialize(aiori_mod_opt_t * options)
{
        RADOS_options_t *o = (RADOS_options_t*) options;
        int ret;

        /* create RADOS cluster handle */
        ret = rados_create(&rados_cluster, o->user);
        if (ret)
                RADOS_ERR("unable to create RADOS cluster handle", ret);

        /* set the handle using the Ceph config */
        ret = rados_conf_read_file(rados_cluster, o->conf);
        if (ret)
                RADOS_ERR("unable to read RADOS config file", ret);

        /* connect to the RADOS cluster */
        ret = rados_connect(rados_cluster);
        if (ret)
                RADOS_ERR("unable to connect to the RADOS cluster", ret);

        /* create an io context for the pool we are operating on */
        ret = rados_ioctx_create(rados_cluster, o->pool, &rados_ioctx);
        if (ret)
                RADOS_ERR("unable to create an I/O context for the RADOS cluster", ret);

        return;
}

static void RADOS_Finalize(aiori_mod_opt_t * options)
{
        /* ioctx destroy */
        rados_ioctx_destroy(rados_ioctx);

        /* shutdown */
        rados_shutdown(rados_cluster);
}

static aiori_fd_t *RADOS_Create_Or_Open(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        int ret;
        char *oid;

        oid = strdup(testFileName);
        if (!oid)
                ERR("unable to allocate RADOS oid");

        if (flags & IOR_CREAT)
        {
                rados_write_op_t create_op;
                int rados_create_flag;

                if (flags & IOR_EXCL)
                        rados_create_flag = LIBRADOS_CREATE_EXCLUSIVE;
                else
                        rados_create_flag = LIBRADOS_CREATE_IDEMPOTENT;

                /* create a RADOS "write op" for creating the object */
                create_op = rados_create_write_op();
                rados_write_op_create(create_op, rados_create_flag, NULL);
                ret = rados_write_op_operate(create_op, rados_ioctx, oid,
                                       NULL, 0);
                rados_release_write_op(create_op);
                if (ret)
                        RADOS_ERR("unable to create RADOS object", ret);
        }
        else
        {
                /* XXX actually, we should probably assert oid existence here? */
        }

        return (aiori_fd_t *)oid;
}

static aiori_fd_t *RADOS_Create(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        return RADOS_Create_Or_Open(testFileName, flags, param);
}

static aiori_fd_t *RADOS_Open(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        return RADOS_Create_Or_Open(testFileName, flags, param);
}

static IOR_offset_t RADOS_Xfer(int access, aiori_fd_t *fd, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param)
{
        int ret;
        char *oid = (char *)fd;

        if (access == WRITE)
        {
                rados_write_op_t write_op;

                write_op = rados_create_write_op();
                rados_write_op_write(write_op, (const char *)buffer,
                                     length, offset);
                ret = rados_write_op_operate(write_op, rados_ioctx,
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
                rados_read_op_read(read_op, offset, length, (char *)buffer,
                                   &bytes_read, &read_ret);
                ret = rados_read_op_operate(read_op, rados_ioctx, oid, 0);
                rados_release_read_op(read_op);
                if (ret || read_ret || ((IOR_offset_t)bytes_read != length))
                        RADOS_ERR("unable to read RADOS object", ret);
        }

        return length;
}

static void RADOS_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        return;
}

static void RADOS_Close(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        char *oid = (char *)fd;

        /* object does not need to be "closed" */
        free(oid);

        return;
}

static void RADOS_Delete(char *testFileName, aiori_mod_opt_t * param)
{
        int ret;
        char *oid = testFileName;
        rados_write_op_t remove_op;

        /* remove the object */
        remove_op = rados_create_write_op();
        rados_write_op_remove(remove_op);
        ret = rados_write_op_operate(remove_op, rados_ioctx,
                                     oid, NULL, 0);
        rados_release_write_op(remove_op);
        if (ret)
                RADOS_ERR("unable to remove RADOS object", ret);

        return;
}

static IOR_offset_t RADOS_GetFileSize(aiori_mod_opt_t *param, char *testFileName)
{
        int ret;
        char *oid = testFileName;
        rados_read_op_t stat_op;
        uint64_t oid_size;
        int stat_ret;
        IOR_offset_t aggSizeFromStat, tmpMin, tmpMax, tmpSum;

        /* stat the object */
        stat_op = rados_create_read_op();
        rados_read_op_stat(stat_op, &oid_size, NULL, &stat_ret);
        ret = rados_read_op_operate(stat_op, rados_ioctx, oid, 0);
        rados_release_read_op(stat_op);
        if (ret || stat_ret)
                RADOS_ERR("unable to stat RADOS object", stat_ret);
        aggSizeFromStat = oid_size;

        return aggSizeFromStat;
}

static int RADOS_StatFS(const char *oid, ior_aiori_statfs_t *stat_buf,
                        aiori_mod_opt_t * param)
{
        WARN("statfs not supported in RADOS backend!");
        return -1;
}

static int RADOS_MkDir(const char *oid, mode_t mode, aiori_mod_opt_t * param)
{
        WARN("mkdir not supported in RADOS backend!");
        return -1;
}

static int RADOS_RmDir(const char *oid, aiori_mod_opt_t * param)
{
        WARN("rmdir not supported in RADOS backend!");
        return -1;
}

static int RADOS_Access(const char *oid, int mode, aiori_mod_opt_t * param)
{
        rados_read_op_t read_op;
        int ret;
        int prval;
        uint64_t oid_size;

        /* use read_op stat to check for oid existence */
        read_op = rados_create_read_op();
        rados_read_op_stat(read_op, &oid_size, NULL, &prval);
        ret = rados_read_op_operate(read_op, rados_ioctx, oid, 0);
        rados_release_read_op(read_op);

        if (ret | prval)
                return -1;
        else
                return 0;
}

static int RADOS_Stat(const char *oid, struct stat *buf, aiori_mod_opt_t * param)
{
        WARN("stat not supported in RADOS backend!");
        return -1;
}
