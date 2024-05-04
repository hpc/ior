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

#include <errno.h>
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
  char * remote_prefix;
  int olazy;
};

static struct cephfs_options o = {
  .user = NULL,
  .conf = NULL,
  .prefix = NULL,
  .remote_prefix = NULL,
  .olazy = 0,
};

static option_help options [] = {
      {0, "cephfs.user", "Username for the ceph cluster", OPTION_OPTIONAL_ARGUMENT, 's', & o.user},
      {0, "cephfs.conf", "Config file for the ceph cluster", OPTION_OPTIONAL_ARGUMENT, 's', & o.conf},
      {0, "cephfs.prefix", "Mount prefix", OPTION_OPTIONAL_ARGUMENT, 's', & o.prefix},
      {0, "cephfs.remote_prefix", "Remote mount prefix", OPTION_OPTIONAL_ARGUMENT, 's', & o.remote_prefix},
      {0, "cephfs.olazy", "Enable Lazy I/O", OPTION_FLAG, 'd', & o.olazy},
      LAST_OPTION
};

static struct ceph_mount_info *cmount;

/**************************** P R O T O T Y P E S *****************************/
static void CEPHFS_Init();
static void CEPHFS_Final();
void CEPHFS_xfer_hints(aiori_xfer_hint_t * params);
static aiori_fd_t *CEPHFS_Create(char *path, int flags, aiori_mod_opt_t *options);
static aiori_fd_t *CEPHFS_Open(char *path, int flags, aiori_mod_opt_t *options);
static IOR_offset_t CEPHFS_Xfer(int access, aiori_fd_t *file, IOR_size_t *buffer,
                           IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t *options);
static void CEPHFS_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void CEPHFS_Delete(char *path, aiori_mod_opt_t *);
static void CEPHFS_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static IOR_offset_t CEPHFS_GetFileSize(aiori_mod_opt_t *, char *);
static int CEPHFS_StatFS(const char *path, ior_aiori_statfs_t *stat, aiori_mod_opt_t *options);
static int CEPHFS_MkDir(const char *path, mode_t mode, aiori_mod_opt_t *options);
static int CEPHFS_RmDir(const char *path, aiori_mod_opt_t *options);
static int CEPHFS_Access(const char *path, int mode, aiori_mod_opt_t *options);
static int CEPHFS_Stat(const char *path, struct stat *buf, aiori_mod_opt_t *options);
static void CEPHFS_Sync(aiori_mod_opt_t *);
static option_help * CEPHFS_options();

static aiori_xfer_hint_t * hints = NULL;

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
        .remove = CEPHFS_Delete,
        .get_options = CEPHFS_options,
        .get_version = aiori_get_version,
        .xfer_hints = CEPHFS_xfer_hints,
        .fsync = CEPHFS_Fsync,
        .get_file_size = CEPHFS_GetFileSize,
        .statfs = CEPHFS_StatFS,
        .mkdir = CEPHFS_MkDir,
        .rmdir = CEPHFS_RmDir,
        .access = CEPHFS_Access,
        .stat = CEPHFS_Stat,
        .sync = CEPHFS_Sync,
        .enable_mdtest = true,
};

#define CEPHFS_ERR(__err_str, __ret) do { \
        errno = -__ret; \
        ERR(__err_str); \
} while(0)

/***************************** F U N C T I O N S ******************************/

void CEPHFS_xfer_hints(aiori_xfer_hint_t * params)
{
  hints = params;
}

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
        char *remote_prefix = "/";

        /* Short circuit if the options haven't been filled yet. */
        if (!o.user || !o.conf || !o.prefix) {
                WARN("CEPHFS_Init() called before options have been populated!");
                return;
        }
        if (o.remote_prefix != NULL) {
                remote_prefix = o.remote_prefix;
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
        ret = ceph_mount(cmount, remote_prefix);
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

static aiori_fd_t *CEPHFS_Create(char *path, int flags, aiori_mod_opt_t *options)
{
        return CEPHFS_Open(path, flags | IOR_CREAT, options);
}

static aiori_fd_t *CEPHFS_Open(char *path, int flags, aiori_mod_opt_t *options)
{
        const char *file = pfix(path);
        int* fd;
        fd = (int *)malloc(sizeof(int));

        mode_t mode = 0664;
        int ceph_flags = (int) 0;

        /* set IOR file flags to CephFS flags */
        /* -- file open flags -- */
        if (flags & IOR_RDONLY) {
                ceph_flags |= CEPH_O_RDONLY;
        }
        if (flags & IOR_WRONLY) {
                ceph_flags |= CEPH_O_WRONLY;
        }
        if (flags & IOR_RDWR) {
                ceph_flags |= CEPH_O_RDWR;
        }
        if (flags & IOR_APPEND) {
                CEPHFS_ERR("File append not implemented in CephFS", EINVAL);
        }
        if (flags & IOR_CREAT) {
                ceph_flags |= CEPH_O_CREAT;
        }
        if (flags & IOR_EXCL) {
                ceph_flags |= CEPH_O_EXCL;
        }
        if (flags & IOR_TRUNC) {
                ceph_flags |= CEPH_O_TRUNC;
        }
        if (flags & IOR_DIRECT) {
                CEPHFS_ERR("O_DIRECT not implemented in CephFS", EINVAL);
        }
        *fd = ceph_open(cmount, file, ceph_flags, mode);
        if (*fd < 0) {
                CEPHFS_ERR("ceph_open failed", *fd);
        }
        if (o.olazy == TRUE) {
                int ret = ceph_lazyio(cmount, *fd, 1);
                if (ret != 0) {
                        WARN("Error enabling lazy mode");
                }
        }
        return (void *) fd;
}

static IOR_offset_t CEPHFS_Xfer(int access, aiori_fd_t *file, IOR_size_t *buffer,
                           IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t *options)
{
        uint64_t size = (uint64_t) length;
        char *buf = (char *) buffer;
        int fd = *(int *) file;
        int ret;

        if (access == WRITE)
        {
                ret = ceph_write(cmount, fd, buf, size, offset);
                if (ret < 0) {
                        CEPHFS_ERR("unable to write file to CephFS", ret);
                } else if (ret < size) {
                        CEPHFS_ERR("short write to CephFS", ret);
                }
                if (hints->fsyncPerWrite == TRUE) {
                        CEPHFS_Fsync(file, options);
                }
        }
        else /* READ */
        {
                ret = ceph_read(cmount, fd, buf, size, offset);
                if (ret < 0) {
                        CEPHFS_ERR("unable to read file from CephFS", ret);
                } else if (ret < size) {
                        CEPHFS_ERR("short read from CephFS", ret);
                }

        }
        return length;
}

static void CEPHFS_Fsync(aiori_fd_t *file, aiori_mod_opt_t *options)
{
        int fd = *(int *) file;
        int ret = ceph_fsync(cmount, fd, 0);
        if (ret < 0) {
                CEPHFS_ERR("ceph_fsync failed", ret);
        }
}

static void CEPHFS_Close(aiori_fd_t *file, aiori_mod_opt_t *options)
{
        int fd = *(int *) file;
        int ret = ceph_close(cmount, fd);
        if (ret < 0) {
                CEPHFS_ERR("ceph_close failed", ret);
        }
        free(file);
        return;
}

static void CEPHFS_Delete(char *path, aiori_mod_opt_t *options)
{
        int ret = ceph_unlink(cmount, pfix(path));
        if (ret < 0) {
                CEPHFS_ERR("ceph_unlink failed", ret);
        }
        return;
}

static IOR_offset_t CEPHFS_GetFileSize(aiori_mod_opt_t *options, char *path)
{
        struct stat stat_buf;
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;

        int ret = ceph_stat(cmount, pfix(path), &stat_buf);
        if (ret < 0) {
                CEPHFS_ERR("ceph_stat failed", ret);
        }
        aggFileSizeFromStat = stat_buf.st_size;

        if (hints->filePerProc == TRUE) {
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

static int CEPHFS_StatFS(const char *path, ior_aiori_statfs_t *stat_buf, aiori_mod_opt_t *options)
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

static int CEPHFS_MkDir(const char *path, mode_t mode, aiori_mod_opt_t *options)
{
        return ceph_mkdir(cmount, pfix(path), mode);
}

static int CEPHFS_RmDir(const char *path, aiori_mod_opt_t *options)
{
        return ceph_rmdir(cmount, pfix(path));
}

static int CEPHFS_Access(const char *path, int mode, aiori_mod_opt_t *options)
{
        struct stat buf;
        return ceph_stat(cmount, pfix(path), &buf);
}

static int CEPHFS_Stat(const char *path, struct stat *buf, aiori_mod_opt_t *options)
{
        return ceph_stat(cmount, pfix(path), buf);
}

static void CEPHFS_Sync(aiori_mod_opt_t *options)
{
        int ret = ceph_sync_fs(cmount);
        if (ret < 0) {
                CEPHFS_ERR("ceph_sync_fs failed", ret);
        }

}
