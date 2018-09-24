/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Definitions and prototypes of abstract I/O interface
*
\******************************************************************************/

#ifndef _AIORI_H
#define _AIORI_H

#include <mpi.h>

#ifndef MPI_FILE_NULL
#   include <mpio.h>
#endif /* not MPI_FILE_NULL */

#include <sys/stat.h>

#include "ior.h"
#include "iordef.h"                                     /* IOR Definitions */
#include "option.h"

/*************************** D E F I N I T I O N S ****************************/

/* -- file open flags -- */
#define IOR_RDONLY        0x01    /* read only */
#define IOR_WRONLY        0x02    /* write only */
#define IOR_RDWR          0x04    /* read/write */
#define IOR_APPEND        0x08    /* append */
#define IOR_CREAT         0x10    /* create */
#define IOR_TRUNC         0x20    /* truncate */
#define IOR_EXCL          0x40    /* exclusive */
#define IOR_DIRECT        0x80    /* bypass I/O buffers */

/* -- file mode flags -- */
#define IOR_IRWXU         0x0001  /* read, write, execute perm: owner */
#define IOR_IRUSR         0x0002  /* read permission: owner */
#define IOR_IWUSR         0x0004  /* write permission: owner */
#define IOR_IXUSR         0x0008  /* execute permission: owner */
#define IOR_IRWXG         0x0010  /* read, write, execute perm: group */
#define IOR_IRGRP         0x0020  /* read permission: group */
#define IOR_IWGRP         0x0040  /* write permission: group */
#define IOR_IXGRP         0x0080  /* execute permission: group */
#define IOR_IRWXO         0x0100  /* read, write, execute perm: other */
#define IOR_IROTH         0x0200  /* read permission: other */
#define IOR_IWOTH         0x0400  /* write permission: other */
#define IOR_IXOTH         0x0800 /* execute permission: other */

typedef struct ior_aiori_statfs {
        uint64_t f_bsize;
        uint64_t f_blocks;
        uint64_t f_bfree;
        uint64_t f_bavail;
        uint64_t f_files;
        uint64_t f_ffree;
} ior_aiori_statfs_t;


typedef struct ior_aiori {
        char *name;
        char *name_legacy;
        void *(*create)(char *, IOR_param_t *);
        void *(*open)(char *, IOR_param_t *);
        IOR_offset_t (*xfer)(int, void *, IOR_size_t *,
                             IOR_offset_t, IOR_param_t *);
        void (*close)(void *, IOR_param_t *);
        void (*delete)(char *, IOR_param_t *);
        char* (*get_version)();
        void (*fsync)(void *, IOR_param_t *);
        IOR_offset_t (*get_file_size)(IOR_param_t *, MPI_Comm, char *);
        int (*statfs) (const char *, ior_aiori_statfs_t *, IOR_param_t * param);
        int (*mkdir) (const char *path, mode_t mode, IOR_param_t * param);
        int (*rmdir) (const char *path, IOR_param_t * param);
        int (*access) (const char *path, int mode, IOR_param_t * param);
        int (*stat) (const char *path, struct stat *buf, IOR_param_t * param);
        void (*initialize)(); /* called once per program before MPI is started */
        void (*finalize)(); /* called once per program after MPI is shutdown */
        option_help * (*get_options)();
} ior_aiori_t;

extern ior_aiori_t dummy_aiori;
extern ior_aiori_t hdf5_aiori;
extern ior_aiori_t hdfs_aiori;
extern ior_aiori_t ime_aiori;
extern ior_aiori_t mpiio_aiori;
extern ior_aiori_t ncmpi_aiori;
extern ior_aiori_t posix_aiori;
extern ior_aiori_t mmap_aiori;
extern ior_aiori_t s3_aiori;
extern ior_aiori_t s3_plus_aiori;
extern ior_aiori_t s3_emc_aiori;
extern ior_aiori_t rados_aiori;

void aiori_initialize(IOR_test_t * tests);
void aiori_finalize(IOR_test_t * tests);
const ior_aiori_t *aiori_select (const char *api);
int aiori_count (void);
void aiori_supported_apis(char * APIs, char * APIs_legacy);
const char *aiori_default (void);

/* some generic POSIX-based backend calls */
char * aiori_get_version();
int aiori_posix_statfs (const char *path, ior_aiori_statfs_t *stat_buf, IOR_param_t * param);
int aiori_posix_mkdir (const char *path, mode_t mode, IOR_param_t * param);
int aiori_posix_rmdir (const char *path, IOR_param_t * param);
int aiori_posix_access (const char *path, int mode, IOR_param_t * param);
int aiori_posix_stat (const char *path, struct stat *buf, IOR_param_t * param);

void *POSIX_Create(char *testFileName, IOR_param_t * param);
void *POSIX_Open(char *testFileName, IOR_param_t * param);
IOR_offset_t POSIX_GetFileSize(IOR_param_t * test, MPI_Comm testComm, char *testFileName);
void POSIX_Delete(char *testFileName, IOR_param_t * param);
void POSIX_Close(void *fd, IOR_param_t * param);

/* NOTE: these 3 MPI-IO functions are exported for reuse by HDF5/PNetCDF */
void MPIIO_Delete(char *testFileName, IOR_param_t * param);
IOR_offset_t MPIIO_GetFileSize(IOR_param_t * test, MPI_Comm testComm,
                               char *testFileName);
int MPIIO_Access(const char *, int, IOR_param_t *);

#endif /* not _AIORI_H */
