/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
* (C) 2015 The University of Chicago                                           *
* (C) 2020 Red Hat, Inc.                                                       *
*                                                                              *
* See COPYRIGHT in top-level directory.                                        *
*                                                                              *
********************************************************************************
*
* Implement abstract I/O interface for CEPHFS.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <cephfs/libcephfs.h>

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"

#define CEPH_O_RDONLY          00000000
#define CEPH_O_WRONLY          00000001
#define CEPH_O_RDWR            00000002
#define CEPH_O_CREAT           00000100
#define CEPH_O_EXCL            00000200
#define CEPH_O_TRUNC           00001000
#define CEPH_O_LAZY            00020000
#define CEPH_O_DIRECTORY       00200000
#define CEPH_O_NOFOLLOW        00400000

/************************** O P T I O N S *****************************/
struct cephfs_options{
  char * user;
  char * conf;
  char * prefix;
};

static struct cephfs_options o = {
  .user = NULL,
  .conf = NULL,
  .prefix = NULL,
};

static option_help options [] = {
      {0, "cephfs.user", "Username for the ceph cluster", OPTION_REQUIRED_ARGUMENT, 's', & o.user},
      {0, "cephfs.conf", "Config file for the ceph cluster", OPTION_REQUIRED_ARGUMENT, 's', & o.conf},
      {0, "cephfs.prefix", "mount prefix", OPTION_OPTIONAL_ARGUMENT, 's', & o.prefix},
      LAST_OPTION
};

static struct ceph_mount_info *cmount;

/**************************** P R O T O T Y P E S *****************************/
static void CEPHFS_Init();
static void CEPHFS_Final();
static void *CEPHFS_Create(char *, IOR_param_t *);
static void *CEPHFS_Open(char *, IOR_param_t *);
static IOR_offset_t CEPHFS_Xfer(int, void *, IOR_size_t *,
                           IOR_offset_t, IOR_param_t *);
static void CEPHFS_Close(void *, IOR_param_t *);
static void CEPHFS_Delete(char *, IOR_param_t *);
static void CEPHFS_Fsync(void *, IOR_param_t *);
static IOR_offset_t CEPHFS_GetFileSize(IOR_param_t *, MPI_Comm, char *);
static int CEPHFS_StatFS(const char *, ior_aiori_statfs_t *, IOR_param_t *);
static int CEPHFS_MkDir(const char *, mode_t, IOR_param_t *);
static int CEPHFS_RmDir(const char *, IOR_param_t *);
static int CEPHFS_Access(const char *, int, IOR_param_t *);
static int CEPHFS_Stat(const char *, struct stat *, IOR_param_t *);
static void CEPHFS_Sync(IOR_param_t *);
static option_help * CEPHFS_options();

/************************** D E C L A R A T I O N S ***************************/
ior_aiori_t cephfs_aiori = {
        .name = "CEPHFS",
        .name_legacy = NULL,
        .initialize = CEPHFS_Init,
        .finalize = CEPHFS_Final,
        .create = CEPHFS_Create,
        .open = CEPHFS_Open,
        .xfer = CEPHFS_Xfer,
        .close = CEPHFS_Close,
        .delete = CEPHFS_Delete,
        .get_version = aiori_get_version,
        .fsync = CEPHFS_Fsync,
        .get_file_size = CEPHFS_GetFileSize,
        .statfs = CEPHFS_StatFS,
        .mkdir = CEPHFS_MkDir,
        .rmdir = CEPHFS_RmDir,
        .access = CEPHFS_Access,
        .stat = CEPHFS_Stat,
        .sync = CEPHFS_Sync,
        .get_options = CEPHFS_options,
};

#define CEPHFS_ERR(__err_str, __ret) do { \
        errno = -__ret; \
        ERR(__err_str); \
} while(0)

/***************************** F U N C T I O N S ******************************/
static const char* pfix(const char* path) {
        const char* npath = path;
        const char* prefix = o.prefix;
        while (*prefix) {
                if(*prefix++ != *npath++) {
                        return path;
                }
        }
        return npath;
}

static option_help * CEPHFS_options(){
  return options;
}

static void CEPHFS_Init()
{
        /* Short circuit if the options haven't been filled yet. */
        if (!o.user || !o.conf || !o.prefix) {
                WARN("CEPHFS_Init() called before options have been populated!");
                return;
        }

        /* Short circuit if the mount handle already exists */ 
        if (cmount) {
                return;
        }

        int ret;
        /* create CEPHFS mount handle */
        ret = ceph_create(&cmount, o.user);
        if (ret) {
                CEPHFS_ERR("unable to create CEPHFS mount handle", ret);
        }

        /* set the handle using the Ceph config */
        ret = ceph_conf_read_file(cmount, o.conf);
        if (ret) {
                CEPHFS_ERR("unable to read ceph config file", ret);
        }

        /* mount the handle */
        ret = ceph_mount(cmount, "/");
        if (ret) {
                CEPHFS_ERR("unable to mount cephfs", ret);
                ceph_shutdown(cmount);

        }

        Inode *root;

        /* try retrieving the root cephfs inode */
        ret = ceph_ll_lookup_root(cmount, &root);
        if (ret) {
                CEPHFS_ERR("uanble to retrieve root cephfs inode", ret);
                ceph_shutdown(cmount);

        }

        return;
}

static void CEPHFS_Final()
{
        /* shutdown */
        int ret = ceph_unmount(cmount);
        if (ret < 0) {
		CEPHFS_ERR("ceph_umount failed", ret);
	}
        ret = ceph_release(cmount);
        if (ret < 0) {
                CEPHFS_ERR("ceph_release failed", ret);
        }
	cmount = NULL;
}

static void *CEPHFS_Create(char *testFileName, IOR_param_t * param)
{
        return CEPHFS_Open(testFileName, param);
}

static void *CEPHFS_Open(char *testFileName, IOR_param_t * param)
{
        const char *file = pfix(testFileName);
        int* fd;
        fd = (int *)malloc(sizeof(int));

        mode_t mode = 0664;
        int flags = (int) 0;

        /* set IOR file flags to CephFS flags */
        /* -- file open flags -- */
        if (param->openFlags & IOR_RDONLY) {
                flags |= CEPH_O_RDONLY;
        }
        if (param->openFlags & IOR_WRONLY) {
                flags |= CEPH_O_WRONLY;
        }
        if (param->openFlags & IOR_RDWR) {
                flags |= CEPH_O_RDWR;
        }
        if (param->openFlags & IOR_APPEND) {
                fprintf(stdout, "File append not implemented in CephFS\n");
        }
        if (param->openFlags & IOR_CREAT) {
                flags |= CEPH_O_CREAT;
        }
        if (param->openFlags & IOR_EXCL) {
                flags |= CEPH_O_EXCL;
        }
        if (param->openFlags & IOR_TRUNC) {
                flags |= CEPH_O_TRUNC;
        }
        if (param->openFlags & IOR_DIRECT) {
                fprintf(stdout, "O_DIRECT not implemented in CephFS\n");
        }
        *fd = ceph_open(cmount, file, flags, mode);
        if (*fd < 0) {
                CEPHFS_ERR("ceph_open failed", *fd);
        }
        return (void *) fd;
}

static IOR_offset_t CEPHFS_Xfer(int access, void *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_param_t * param)
{
        uint64_t size = (uint64_t) length;
        char *buf = (char *) buffer;
        int fd = *(int *) file;
        int ret;

        if (access == WRITE)
        {
                ret = ceph_write(cmount, fd, buf, size, param->offset);
                if (ret < 0) {
                        CEPHFS_ERR("unable to write file to CephFS", ret);
                } else if (ret < size) {
                        CEPHFS_ERR("short write to CephFS", ret);
                }
                if (param->fsyncPerWrite == TRUE) {
                        CEPHFS_Fsync(&fd, param);
                }
        }
        else /* READ */
        {
                ret = ceph_read(cmount, fd, buf, size, param->offset);
                if (ret < 0) {
                        CEPHFS_ERR("unable to read file from CephFS", ret);
                } else if (ret < size) {
                        CEPHFS_ERR("short read from CephFS", ret);
                }

        }
        return length;
}

static void CEPHFS_Fsync(void *file, IOR_param_t * param)
{
        int fd = *(int *) file;
        int ret = ceph_fsync(cmount, fd, 0);
        if (ret < 0) {
                CEPHFS_ERR("ceph_fsync failed", ret);
        }
}

static void CEPHFS_Close(void *file, IOR_param_t * param)
{
        int fd = *(int *) file;
        int ret = ceph_close(cmount, fd);
        if (ret < 0) {
                CEPHFS_ERR("ceph_close failed", ret);
        }
        free(file);
        return;
}

static void CEPHFS_Delete(char *testFileName, IOR_param_t * param)
{
        int ret = ceph_unlink(cmount, pfix(testFileName));
        if (ret < 0) {
                CEPHFS_ERR("ceph_unlink failed", ret);
        }
        return;
}

static IOR_offset_t CEPHFS_GetFileSize(IOR_param_t * param, MPI_Comm testComm,
                                      char *testFileName)
{
        struct stat stat_buf;
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;

        int ret = ceph_stat(cmount, pfix(testFileName), &stat_buf);
        if (ret < 0) {
                CEPHFS_ERR("ceph_stat failed", ret);
        }
        aggFileSizeFromStat = stat_buf.st_size;

        if (param->filePerProc == TRUE) {
                MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpSum, 1,
                                        MPI_LONG_LONG_INT, MPI_SUM, testComm),
                          "cannot total data moved");
                aggFileSizeFromStat = tmpSum;
        } else {
                MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMin, 1,
                                        MPI_LONG_LONG_INT, MPI_MIN, testComm),
                          "cannot total data moved");
                MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMax, 1,
                                        MPI_LONG_LONG_INT, MPI_MAX, testComm),
                          "cannot total data moved");
                if (tmpMin != tmpMax) {
                        if (rank == 0) {
                                WARN("inconsistent file size by different tasks");
                        }
                        /* incorrect, but now consistent across tasks */
                        aggFileSizeFromStat = tmpMin;
                }
        }

        return (aggFileSizeFromStat);

}

static int CEPHFS_StatFS(const char *path, ior_aiori_statfs_t *stat_buf,
                        IOR_param_t *param)
{
#if defined(HAVE_STATVFS)
        struct statvfs statfs_buf;
        int ret = ceph_statfs(cmount, pfix(path), &statfs_buf);
        if (ret < 0) {
                CEPHFS_ERR("ceph_statfs failed", ret);
                return -1;
        }

        stat_buf->f_bsize = statfs_buf.f_bsize;
        stat_buf->f_blocks = statfs_buf.f_blocks;
        stat_buf->f_bfree = statfs_buf.f_bfree;
        stat_buf->f_files = statfs_buf.f_files;
        stat_buf->f_ffree = statfs_buf.f_ffree;

        return 0;
#else
        WARN("ceph_statfs requires statvfs!");
        return -1;
#endif
}

static int CEPHFS_MkDir(const char *path, mode_t mode, IOR_param_t *param)
{
        return ceph_mkdir(cmount, pfix(path), mode);
}

static int CEPHFS_RmDir(const char *path, IOR_param_t *param)
{
        return ceph_rmdir(cmount, pfix(path));
}

static int CEPHFS_Access(const char *testFileName, int mode, IOR_param_t *param)
{
        struct stat buf;
        return ceph_stat(cmount, pfix(testFileName), &buf);
}

static int CEPHFS_Stat(const char *testFileName, struct stat *buf, IOR_param_t *param)
{
        return ceph_stat(cmount, pfix(testFileName), buf);
}

static void CEPHFS_Sync(IOR_param_t *param)
{
        int ret = ceph_sync_fs(cmount);
        if (ret < 0) {
                CEPHFS_ERR("ceph_sync_fs failed", ret);
        }

}
