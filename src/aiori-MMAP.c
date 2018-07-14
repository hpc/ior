/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*
* Implement of abstract I/O interface for MMAP.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>              /* IO operations */
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>

#include "ior.h"
#include "aiori.h"
#include "iordef.h"
#include "utilities.h"

/**************************** P R O T O T Y P E S *****************************/
static void *MMAP_Create(char *, IOR_param_t *);
static void *MMAP_Open(char *, IOR_param_t *);
static IOR_offset_t MMAP_Xfer(int, void *, IOR_size_t *,
                               IOR_offset_t, IOR_param_t *);
static void MMAP_Close(void *, IOR_param_t *);
static void MMAP_Fsync(void *, IOR_param_t *);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t mmap_aiori = {
        .name = "MMAP",
        .create = MMAP_Create,
        .open = MMAP_Open,
        .xfer = MMAP_Xfer,
        .close = MMAP_Close,
        .delete = POSIX_Delete,
        .get_version = aiori_get_version,
        .fsync = MMAP_Fsync,
        .get_file_size = POSIX_GetFileSize,
};

/***************************** F U N C T I O N S ******************************/

static void ior_mmap_file(int *file, IOR_param_t *param)
{
        int flags = PROT_READ;
        IOR_offset_t size = param->expectedAggFileSize;

        if (param->open == WRITE)
                flags |= PROT_WRITE;

        param->mmap_ptr = mmap(NULL, size, flags, MAP_SHARED,
                               *file, 0);
        if (param->mmap_ptr == MAP_FAILED)
                ERR("mmap() failed");

        if (param->randomOffset)
                flags = POSIX_MADV_RANDOM;
        else
                flags = POSIX_MADV_SEQUENTIAL;
        if (posix_madvise(param->mmap_ptr, size, flags) != 0)
                ERR("madvise() failed");

        if (posix_madvise(param->mmap_ptr, size, POSIX_MADV_DONTNEED) != 0)
                ERR("madvise() failed");

        return;
}

/*
 * Creat and open a file through the POSIX interface, then setup mmap.
 */
static void *MMAP_Create(char *testFileName, IOR_param_t * param)
{
        int *fd;

        fd = POSIX_Create(testFileName, param);
        if (ftruncate(*fd, param->expectedAggFileSize) != 0)
                ERR("ftruncate() failed");
        ior_mmap_file(fd, param);
        return ((void *)fd);
}

/*
 * Open a file through the POSIX interface and setup mmap.
 */
static void *MMAP_Open(char *testFileName, IOR_param_t * param)
{
        int *fd;

        fd = POSIX_Open(testFileName, param);
        ior_mmap_file(fd, param);
        return ((void *)fd);
}

/*
 * Write or read access to file using mmap
 */
static IOR_offset_t MMAP_Xfer(int access, void *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_param_t * param)
{
        if (access == WRITE) {
                memcpy(param->mmap_ptr + param->offset, buffer, length);
        } else {
                memcpy(buffer, param->mmap_ptr + param->offset, length);
        }

        if (param->fsyncPerWrite == TRUE) {
                if (msync(param->mmap_ptr + param->offset, length, MS_SYNC) != 0)
                        ERR("msync() failed");
                if (posix_madvise(param->mmap_ptr + param->offset, length,
                                  POSIX_MADV_DONTNEED) != 0)
                        ERR("madvise() failed");
        }
        return (length);
}

/*
 * Perform msync().
 */
static void MMAP_Fsync(void *fd, IOR_param_t * param)
{
        if (msync(param->mmap_ptr, param->expectedAggFileSize, MS_SYNC) != 0)
                EWARN("msync() failed");
}

/*
 * Close a file through the POSIX interface, after tear down the mmap.
 */
static void MMAP_Close(void *fd, IOR_param_t * param)
{
        if (munmap(param->mmap_ptr, param->expectedAggFileSize) != 0)
                ERR("munmap failed");
        param->mmap_ptr = NULL;
        POSIX_Close(fd, param);
}
