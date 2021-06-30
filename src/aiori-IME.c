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
#include <errno.h>                              /* sys_errlist */
#include <fcntl.h>                              /* IO operations */

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"
#include "ime_native.h"

#define IME_UNUSED(x) (void)(x)                 /* Silence compiler warnings */

#ifndef   O_BINARY                              /* Required on Windows */
#  define O_BINARY 0
#endif

/**************************** P R O T O T Y P E S *****************************/

aiori_fd_t         *IME_Create(char *, int, aiori_mod_opt_t *);
aiori_fd_t         *IME_Open(char *, int, aiori_mod_opt_t *);
void                IME_Close(aiori_fd_t *, aiori_mod_opt_t *);
void                IME_Delete(char *, aiori_mod_opt_t *);
char               *IME_GetVersion();
void                IME_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
int                 IME_Access(const char *, int, aiori_mod_opt_t *);
IOR_offset_t        IME_GetFileSize(aiori_mod_opt_t *, char *);
IOR_offset_t        IME_Xfer(int, aiori_fd_t *, IOR_size_t *, IOR_offset_t,
                             IOR_offset_t, aiori_mod_opt_t *);
int                 IME_Statfs(const char *, ior_aiori_statfs_t *,
                               aiori_mod_opt_t *);
int                 IME_Rmdir(const char *, aiori_mod_opt_t *);
int                 IME_Mkdir(const char *, mode_t, aiori_mod_opt_t *);
int                 IME_Stat(const char *, struct stat *, aiori_mod_opt_t *);
void                IME_Xferhints(aiori_xfer_hint_t *params);

#if (IME_NATIVE_API_VERSION >= 132)
int                 IME_Mknod(char *);
void                IME_Sync(aiori_mod_opt_t *param);
#endif

void                IME_Initialize();
void                IME_Finalize();

/****************************** O P T I O N S *********************************/

typedef struct{
        int direct_io;
} ime_options_t;

option_help *IME_Options(aiori_mod_opt_t **init_backend_options,
                         aiori_mod_opt_t *init_values)
{
        ime_options_t *o = malloc(sizeof(ime_options_t));

        if (init_values != NULL)
                memcpy(o, init_values, sizeof(ime_options_t));
        else
                o->direct_io = 0;

        *init_backend_options = (aiori_mod_opt_t*)o;

        option_help h[] = {
            {0, "ime.odirect", "Direct I/O Mode", OPTION_FLAG, 'd', & o->direct_io},
            LAST_OPTION
        };
        option_help *help = malloc(sizeof(h));
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
        .xfer_hints    = IME_Xferhints,
        .close         = IME_Close,
        .delete        = IME_Delete,
        .get_version   = IME_GetVersion,
        .fsync         = IME_Fsync,
        .get_file_size = IME_GetFileSize,
        .access        = IME_Access,
        .statfs        = IME_Statfs,
        .rmdir         = IME_Rmdir,
        .mkdir         = IME_Mkdir,
        .stat          = IME_Stat,
        .initialize    = IME_Initialize,
        .finalize      = IME_Finalize,
        .get_options   = IME_Options,
#if (IME_NATIVE_API_VERSION >= 132)
        .sync          = IME_Sync,
        .mknod         = IME_Mknod,
#endif
        .enable_mdtest = true,
};

static aiori_xfer_hint_t *hints = NULL;
static bool ime_initialized = false;


/***************************** F U N C T I O N S ******************************/

void IME_Xferhints(aiori_xfer_hint_t *params)
{
        hints = params;
}

/*
 * Initialize IME (before MPI is started).
 */
void IME_Initialize()
{
        if (ime_initialized)
                return;

        ime_native_init();
        ime_initialized = true;
}

/*
 * Finlize IME (after MPI is shutdown).
 */
void IME_Finalize()
{
        if (!ime_initialized)
                return;

        (void)ime_native_finalize();
        ime_initialized = false;
}

/*
 * Try to access a file through the IME interface.
 */

int IME_Access(const char *path, int mode, aiori_mod_opt_t *module_options)
{
        IME_UNUSED(module_options);

        return ime_native_access(path, mode);
}

/*
 * Create and open a file through the IME interface.
 */
aiori_fd_t *IME_Create(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        return IME_Open(testFileName, flags, param);
}

/*
 * Open a file through the IME interface.
 */
aiori_fd_t *IME_Open(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        int fd_oflag = O_BINARY;
        int *fd;

        if (hints->dryRun)
                return NULL;

        fd = (int *)malloc(sizeof(int));
        if (fd == NULL)
                ERR("Unable to malloc file descriptor");

        ime_options_t *o = (ime_options_t*) param;
        if (o->direct_io == TRUE)
                set_o_direct_flag(&fd_oflag);

        if (flags & IOR_RDONLY)
                fd_oflag |= O_RDONLY;
        if (flags & IOR_WRONLY)
                fd_oflag |= O_WRONLY;
        if (flags & IOR_RDWR)
                fd_oflag |= O_RDWR;
        if (flags & IOR_APPEND)
                fd_oflag |= O_APPEND;
        if (flags & IOR_CREAT)
                fd_oflag |= O_CREAT;
        if (flags & IOR_EXCL)
                fd_oflag |= O_EXCL;
        if (flags & IOR_TRUNC)
                fd_oflag |= O_TRUNC;

        *fd = ime_native_open(testFileName, fd_oflag, 0664);
        if (*fd < 0) {
                free(fd);
                ERR("cannot open file");
        }

        return (aiori_fd_t*) fd;
}

/*
 * Write or read access to file using the IM interface.
 */
IOR_offset_t IME_Xfer(int access, aiori_fd_t *file, IOR_size_t *buffer,
                      IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t *param)
{
        int xferRetries = 0;
        long long remaining = (long long)length;
        char *ptr = (char *)buffer;
        int fd = *(int *)file;
        long long rc;

        if (hints->dryRun)
                return length;

        while (remaining > 0) {
                /* write/read file */
                if (access == WRITE) { /* WRITE */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout, "task %d writing to offset %lld\n",
                                        rank, offset + length - remaining);
                        }

                        rc = ime_native_pwrite(fd, ptr, remaining, offset);

                        if (hints->fsyncPerWrite)
                                IME_Fsync(file, param);
                } else {               /* READ or CHECK */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout, "task %d reading from offset %lld\n",
                                        rank, offset + length - remaining);
                        }

                        rc = ime_native_pread(fd, ptr, remaining, offset);
                        if (rc == 0)
                                ERR("hit EOF prematurely");
                        else if (rc < 0)
                                ERR("read failed");
                 }

                if (rc < remaining) {
                        fprintf(stdout, "WARNING: Task %d, partial %s, %lld of "
                                "%lld bytes at offset %lld\n",
                                rank, access == WRITE ? "write" : "read", rc,
                                remaining, offset + length - remaining );

                        if (hints->singleXferAttempt) {
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
void IME_Fsync(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
        if (ime_native_fsync(*(int *)fd) != 0)
                WARN("cannot perform fsync on file");
}

/*
 * Close a file through the IME interface.
 */
void IME_Close(aiori_fd_t *file, aiori_mod_opt_t *param)
{
        if (hints->dryRun)
                return;

        if (ime_native_close(*(int*)file) != 0)
                ERRF("Cannot close file descriptor: %d", *(int*)file);

        free(file);
}

/*
 * Delete a file through the IME interface.
 */
void IME_Delete(char *testFileName, aiori_mod_opt_t *param)
{
        if (hints->dryRun)
                return;

        if (ime_native_unlink(testFileName) != 0)
                EWARNF("[RANK %03d]: cannot delete file \"%s\"\n",
                       rank, testFileName);
}

/*
 * Determine API version.
 */
char *IME_GetVersion()
{
        static char ver[1024] = {};
#if (IME_NATIVE_API_VERSION >= 120)
        strcpy(ver, ime_native_version());
#else
        strcpy(ver, "not supported");
#endif
        return ver;
}

int IME_Statfs(const char *path, ior_aiori_statfs_t *stat_buf,
               aiori_mod_opt_t *module_options)
{
        IME_UNUSED(module_options);

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
        IME_UNUSED(path);
        IME_UNUSED(stat_buf);

        WARN("statfs is currently not supported in IME backend!");
        return -1;
#endif
}

int IME_Mkdir(const char *path, mode_t mode, aiori_mod_opt_t * module_options)
{
        IME_UNUSED(module_options);

#if (IME_NATIVE_API_VERSION >= 130)
        return ime_native_mkdir(path, mode);
#else
        IME_UNUSED(path);
        IME_UNUSED(mode);

        WARN("mkdir not supported in IME backend!");
        return -1;
#endif
}

int IME_Rmdir(const char *path, aiori_mod_opt_t *module_options)
{
        IME_UNUSED(module_options);

#if (IME_NATIVE_API_VERSION >= 130)
        return ime_native_rmdir(path);
#else
        IME_UNUSED(path);

        WARN("rmdir not supported in IME backend!");
        return -1;
#endif
}

/*
 * Perform stat() through the IME interface.
 */
int IME_Stat(const char *path, struct stat *buf,
             aiori_mod_opt_t *module_options)
{
    IME_UNUSED(module_options);

    return ime_native_stat(path, buf);
}

/*
 * Use IME stat() to return aggregate file size.
 */
IOR_offset_t IME_GetFileSize(aiori_mod_opt_t *test, char *testFileName)
{
        struct stat stat_buf;

        if (hints->dryRun)
                return 0;

        if (ime_native_stat(testFileName, &stat_buf) != 0)
                ERRF("cannot get status of written file %s",
                      testFileName);
        return stat_buf.st_size;
}

#if (IME_NATIVE_API_VERSION >= 132)
/*
 * Create a file through mknod interface.
 */
int IME_Mknod(char *testFileName)
{
        int ret = ime_native_mknod(testFileName, S_IFREG | S_IRUSR, 0);
        if (ret < 0)
                ERR("mknod failed");

        return ret;
}

/*
 * Use IME sync to flush page cache of all opened files.
 */
void IME_Sync(aiori_mod_opt_t *param)
{
        int ret = ime_native_sync(0);
        if (ret != 0)
                FAIL("Error executing the sync command.");
}
#endif
