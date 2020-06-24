/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California.      *
*        Copyright (c) 2018, DataDirect Networks.                              *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Implement abstract I/O interface for DDN Infinite Memory Engine (IME).
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>                                  /* sys_errlist */
#include <fcntl.h>                                  /* IO operations */

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"
#include "ime_native.h"

#ifndef   O_BINARY                                  /* Required on Windows */
#  define O_BINARY 0
#endif

/**************************** P R O T O T Y P E S *****************************/

static void        *IME_Create(char *, IOR_param_t *);
static void        *IME_Open(char *, IOR_param_t *);
static void         IME_Close(void *, IOR_param_t *);
static void         IME_Delete(char *, IOR_param_t *);
static char        *IME_GetVersion();
static void         IME_Fsync(void *, IOR_param_t *);
static int          IME_Access(const char *, int, IOR_param_t *);
static IOR_offset_t IME_GetFileSize(IOR_param_t *, MPI_Comm, char *);
static IOR_offset_t IME_Xfer(int, void *, IOR_size_t *,
                             IOR_offset_t, IOR_param_t *);
static int          IME_StatFS(const char *, ior_aiori_statfs_t *,
                               IOR_param_t *);
static int          IME_RmDir(const char *, IOR_param_t *);
static int          IME_MkDir(const char *, mode_t, IOR_param_t *);
static int          IME_Stat(const char *, struct stat *, IOR_param_t *);

#if (IME_NATIVE_API_VERSION >= 132)
static int          IME_Mknod(char *);
static void         IME_Sync(IOR_param_t *);
#endif

static void         IME_Initialize();
static void         IME_Finalize();


/************************** O P T I O N S *****************************/
typedef struct{
  int direct_io;
} ime_options_t;


option_help * IME_options(void ** init_backend_options, void * init_values){
  ime_options_t * o = malloc(sizeof(ime_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(ime_options_t));
  }else{
    o->direct_io = 0;
  }

  *init_backend_options = o;

  option_help h [] = {
    {0, "ime.odirect", "Direct I/O Mode", OPTION_FLAG, 'd', & o->direct_io},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

/************************** D E C L A R A T I O N S ***************************/

extern int      rank;
extern int      rankOffset;
extern int      verbose;
extern MPI_Comm testComm;

ior_aiori_t ime_aiori = {
        .name          = "IME",
        .name_legacy   = "IM",
        .create        = IME_Create,
        .open          = IME_Open,
        .xfer          = IME_Xfer,
        .close         = IME_Close,
        .delete        = IME_Delete,
        .get_version   = IME_GetVersion,
        .fsync         = IME_Fsync,
        .get_file_size = IME_GetFileSize,
        .access        = IME_Access,
        .statfs        = IME_StatFS,
        .rmdir         = IME_RmDir,
        .mkdir         = IME_MkDir,
        .stat          = IME_Stat,
        .initialize    = IME_Initialize,
        .finalize      = IME_Finalize,
        .get_options   = IME_options,
#if (IME_NATIVE_API_VERSION >= 132)
        .sync          = IME_Sync,
        .mknod         = IME_Mknod,
#endif
        .enable_mdtest = true,
};

/***************************** F U N C T I O N S ******************************/

/*
 * Initialize IME (before MPI is started).
 */
static void IME_Initialize()
{
        ime_native_init();
}

/*
 * Finlize IME (after MPI is shutdown).
 */
static void IME_Finalize()
{
        (void)ime_native_finalize();
}

/*
 * Try to access a file through the IME interface.
 */
static int IME_Access(const char *path, int mode, IOR_param_t *param)
{
        (void)param;

        return ime_native_access(path, mode);
}

/*
 * Creat and open a file through the IME interface.
 */
static void *IME_Create(char *testFileName, IOR_param_t *param)
{
        return IME_Open(testFileName, param);
}

/*
 * Open a file through the IME interface.
 */
static void *IME_Open(char *testFileName, IOR_param_t *param)
{
        int fd_oflag = O_BINARY;
        int *fd;

        fd = (int *)malloc(sizeof(int));
        if (fd == NULL)
                ERR("Unable to malloc file descriptor");

        ime_options_t * o = (ime_options_t*) param->backend_options;
        if (o->direct_io == TRUE){
          set_o_direct_flag(&fd_oflag);
        }

        if (param->openFlags & IOR_RDONLY)
                fd_oflag |= O_RDONLY;
        if (param->openFlags & IOR_WRONLY)
                fd_oflag |= O_WRONLY;
        if (param->openFlags & IOR_RDWR)
                fd_oflag |= O_RDWR;
        if (param->openFlags & IOR_APPEND)
                fd_oflag |= O_APPEND;
        if (param->openFlags & IOR_CREAT)
                fd_oflag |= O_CREAT;
        if (param->openFlags & IOR_EXCL)
                fd_oflag |= O_EXCL;
        if (param->openFlags & IOR_TRUNC)
                fd_oflag |= O_TRUNC;

        *fd = ime_native_open(testFileName, fd_oflag, 0664);
        if (*fd < 0) {
                free(fd);
                ERR("cannot open file");
        }

        return((void *)fd);
}

/*
 * Write or read access to file using the IM interface.
 */
static IOR_offset_t IME_Xfer(int access, void *file, IOR_size_t *buffer,
                             IOR_offset_t length, IOR_param_t *param)
{
        int xferRetries = 0;
        long long remaining = (long long)length;
        char *ptr = (char *)buffer;
        int fd = *(int *)file;
        long long rc;

        while (remaining > 0) {
                /* write/read file */
                if (access == WRITE) { /* WRITE */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout, "task %d writing to offset %lld\n",
                                        rank, param->offset + length - remaining);
                        }

                        rc = ime_native_pwrite(fd, ptr, remaining, param->offset);

                        if (param->fsyncPerWrite)
                                IME_Fsync(&fd, param);
                } else {               /* READ or CHECK */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout, "task %d reading from offset %lld\n",
                                        rank, param->offset + length - remaining);
                        }

                        rc = ime_native_pread(fd, ptr, remaining, param->offset);
                        if (rc == 0)
                                ERR("hit EOF prematurely");
                        else if (rc < 0)
                                ERR("read failed");
                 }

                if (rc < remaining) {
                        fprintf(stdout, "WARNING: Task %d, partial %s, %lld of "
                                "%lld bytes at offset %lld\n",
                                rank, access == WRITE ? "write" : "read", rc,
                                remaining, param->offset + length - remaining );

                        if (param->singleXferAttempt) {
                                MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1),
                                          "barrier error");
                        }

                        if (xferRetries > MAX_RETRY) {
                                ERR( "too many retries -- aborting" );
                        }
                } else if (rc > remaining) /* this should never happen */
                        ERR("too many bytes transferred!?!");

                assert(rc >= 0);
                assert(rc <= remaining);
                remaining -= rc;
                ptr += rc;
                xferRetries++;
        }

        return(length);
}

/*
 * Perform fsync().
 */
static void IME_Fsync(void *fd, IOR_param_t *param)
{
        if (ime_native_fsync(*(int *)fd) != 0)
                WARN("cannot perform fsync on file");
}

/*
 * Close a file through the IME interface.
 */
static void IME_Close(void *fd, IOR_param_t *param)
{
        if (ime_native_close(*(int *)fd) != 0)
        {
                free(fd);
                ERR("cannot close file");
        }
        else
                free(fd);
}

/*
 * Delete a file through the IME interface.
 */
static void IME_Delete(char *testFileName, IOR_param_t *param)
{
        char errmsg[256];
        sprintf(errmsg, "[RANK %03d]:cannot delete file %s\n",
                rank, testFileName);
        if (ime_native_unlink(testFileName) != 0)
                WARN(errmsg);
}

/*
 * Determine API version.
 */
static char *IME_GetVersion()
{
        static char ver[1024] = {};
#if (IME_NATIVE_API_VERSION >= 120)
        strcpy(ver, ime_native_version());
#else
        strcpy(ver, "not supported");
#endif
        return ver;
}

static int IME_StatFS(const char *path, ior_aiori_statfs_t *stat_buf,
                      IOR_param_t *param)
{
        (void)param;

#if (IME_NATIVE_API_VERSION >= 130)
        struct statvfs statfs_buf;

        int ret = ime_native_statvfs(path, &statfs_buf);
        if (ret)
            return ret;

        stat_buf->f_bsize = statfs_buf.f_bsize;
        stat_buf->f_blocks = statfs_buf.f_blocks;
        stat_buf->f_bfree = statfs_buf.f_bfree;
        stat_buf->f_files = statfs_buf.f_files;
        stat_buf->f_ffree = statfs_buf.f_ffree;

        return 0;
#else
        (void)path;
        (void)stat_buf;

        WARN("statfs is currently not supported in IME backend!");
        return -1;
#endif
}


static int IME_MkDir(const char *path, mode_t mode, IOR_param_t *param)
{
        (void)param;

#if (IME_NATIVE_API_VERSION >= 130)
        return ime_native_mkdir(path, mode);
#else
        (void)path;
        (void)mode;

        WARN("mkdir not supported in IME backend!");
        return -1;
#endif
}

static int IME_RmDir(const char *path, IOR_param_t *param)
{
        (void)param;

#if (IME_NATIVE_API_VERSION >= 130)
        return ime_native_rmdir(path);
#else
        (void)path;

        WARN("rmdir not supported in IME backend!");
        return -1;
#endif
}

/*
 * Perform stat() through the IME interface.
 */
static int IME_Stat(const char *path, struct stat *buf, IOR_param_t *param)
{
    (void)param;

    return ime_native_stat(path, buf);
}

/*
 * Use IME stat() to return aggregate file size.
 */
static IOR_offset_t IME_GetFileSize(IOR_param_t *test, MPI_Comm testComm,
                                    char *testFileName)
{
        struct stat stat_buf;
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;

        if (ime_native_stat(testFileName, &stat_buf) != 0) {
                ERR("cannot get status of written file");
        }
        aggFileSizeFromStat = stat_buf.st_size;

        if (test->filePerProc) {
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

        return(aggFileSizeFromStat);
}

#if (IME_NATIVE_API_VERSION >= 132)
/*
 * Create a file through mknod interface.
 */
static int IME_Mknod(char *testFileName)
{
    int ret = ime_native_mknod(testFileName, S_IFREG | S_IRUSR, 0);
    if (ret < 0)
        ERR("mknod failed");

    return ret;
}

/*
 * Use IME sync to flush page cache of all opened files.
 */
static void IME_Sync(IOR_param_t * param)
{
    int ret = ime_native_sync(0);
    if (ret != 0)
        FAIL("Error executing the sync command.");
}
#endif
