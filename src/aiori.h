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

#include <sys/stat.h>
#include <stdbool.h>

#include "iordef.h"                                     /* IOR Definitions */
#include "aiori-debug.h"
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

/*
 This structure contains information about the expected IO pattern that may be used to optimize data access. Optimally, it should be stored for each file descriptor, at the moment it can only be set globally per aiori backend module.
 */
typedef struct aiori_xfer_hint_t{
  int dryRun;                      /* do not perform any I/Os just run evtl. inputs print dummy output */
  int filePerProc;                 /* single file or file-per-process */
  int collective;                  /* collective I/O */
  int numTasks;                    /* number of tasks for test */
  int numNodes;                    /* number of nodes for test */
  int randomOffset;                /* access is to random offsets */
  int fsyncPerWrite;               /* fsync() after each write */
  IOR_offset_t segmentCount;       /* number of segments (or HDF5 datasets) */
  IOR_offset_t blockSize;          /* contiguous bytes to write per task */
  IOR_offset_t transferSize;       /* size of transfer in bytes */
  IOR_offset_t expectedAggFileSize; /* calculated aggregate file size */
  int singleXferAttempt;           /* do not retry transfer if incomplete */
} aiori_xfer_hint_t;

/* this is a dummy structure to create some type safety */
struct aiori_mod_opt_t{
  void * dummy;
};

typedef struct aiori_fd_t{
  void * dummy;
} aiori_fd_t;

typedef struct ior_aiori {
        char *name;
        char *name_legacy;
        aiori_fd_t *(*create)(char *, int iorflags, aiori_mod_opt_t *);
        int (*mknod)(char *);
        aiori_fd_t *(*open)(char *, int iorflags, aiori_mod_opt_t *);
        /*
         Allow to set generic transfer options that shall be applied to any subsequent IO call.
        */
        void (*xfer_hints)(aiori_xfer_hint_t * params);
        IOR_offset_t (*xfer)(int access, aiori_fd_t *, IOR_size_t *,
                             IOR_offset_t size, IOR_offset_t offset, aiori_mod_opt_t * module_options);
        void (*close)(aiori_fd_t *, aiori_mod_opt_t * module_options);
        void (*remove)(char *, aiori_mod_opt_t * module_options);
        char* (*get_version)(void);
        void (*fsync)(aiori_fd_t *, aiori_mod_opt_t * module_options);
        IOR_offset_t (*get_file_size)(aiori_mod_opt_t * module_options, char * filename);
        int (*statfs) (const char *, ior_aiori_statfs_t *, aiori_mod_opt_t * module_options);
        int (*mkdir) (const char *path, mode_t mode, aiori_mod_opt_t * module_options);
        int (*rmdir) (const char *path, aiori_mod_opt_t * module_options);
        int (*access) (const char *path, int mode, aiori_mod_opt_t * module_options);
        int (*stat) (const char *path, struct stat *buf, aiori_mod_opt_t * module_options);
        void (*initialize)(aiori_mod_opt_t * options); /* called once per program before MPI is started */
        void (*finalize)(aiori_mod_opt_t * options); /* called once per program after MPI is shutdown */
        int (*rename) (const char *oldpath, const char *newpath, aiori_mod_opt_t * module_options);
        option_help * (*get_options)(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t* init_values); /* initializes the backend options as well and returns the pointer to the option help structure */
        int (*check_params)(aiori_mod_opt_t *); /* check if the provided module_optionseters for the given test and the module options are correct, if they aren't print a message and exit(1) or return 1*/
        void (*sync)(aiori_mod_opt_t * ); /* synchronize every pending operation for this storage */
        bool enable_mdtest;
} ior_aiori_t;

enum bench_type {
    IOR,
    MDTEST
};

extern ior_aiori_t dummy_aiori;
extern ior_aiori_t aio_aiori;
extern ior_aiori_t daos_aiori;
extern ior_aiori_t dfs_aiori;
extern ior_aiori_t hdf5_aiori;
extern ior_aiori_t hdfs_aiori;
extern ior_aiori_t ime_aiori;
extern ior_aiori_t mpiio_aiori;
extern ior_aiori_t ncmpi_aiori;
extern ior_aiori_t posix_aiori;
extern ior_aiori_t pmdk_aiori;
extern ior_aiori_t mmap_aiori;
extern ior_aiori_t S3_libS3_aiori;
extern ior_aiori_t s3_4c_aiori;
extern ior_aiori_t s3_plus_aiori;
extern ior_aiori_t s3_emc_aiori;
extern ior_aiori_t rados_aiori;
extern ior_aiori_t cephfs_aiori;
extern ior_aiori_t gfarm_aiori;
extern ior_aiori_t chfs_aiori;
extern ior_aiori_t finchfs_aiori;
extern ior_aiori_t libnfs_aiori;

const ior_aiori_t *aiori_select (const char *api);
int aiori_count (void);
void aiori_supported_apis(char * APIs, char * APIs_legacy, enum bench_type type);
options_all_t * airoi_create_all_module_options(option_help * global_options);

void * airoi_update_module_options(const ior_aiori_t * backend, options_all_t * module_defaults);

const char *aiori_default (void);

/* some generic POSIX-based backend calls */
char * aiori_get_version (void);
int aiori_posix_statfs (const char *path, ior_aiori_statfs_t *stat_buf, aiori_mod_opt_t * module_options);
int aiori_posix_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * module_options);
int aiori_posix_rmdir (const char *path, aiori_mod_opt_t * module_options);
int aiori_posix_access (const char *path, int mode, aiori_mod_opt_t * module_options);
int aiori_posix_stat (const char *path, struct stat *buf, aiori_mod_opt_t * module_options);


/* NOTE: these MPI-IO pro are exported for reuse by HDF5/PNetCDF */

typedef struct { /* if you change this datatype, e.g., adding more options, make sure that all depending modules are updated */
  int showHints;                   /* show hints */
  int useFileView;                 /* use MPI_File_set_view */
  int preallocate;                 /* preallocate file size */
  int useSharedFilePointer;        /* use shared file pointer */
  int useStridedDatatype;          /* put strided access into datatype */
  char * hintsFileName;            /* full name for hints file */
} mpiio_options_t;

void MPIIO_Delete(char *testFileName, aiori_mod_opt_t * module_options);
IOR_offset_t MPIIO_GetFileSize(aiori_mod_opt_t * options, char *testFileName);
int MPIIO_Access(const char *, int, aiori_mod_opt_t * module_options);
void MPIIO_xfer_hints(aiori_xfer_hint_t * params);

#endif /* not _AIORI_H */
