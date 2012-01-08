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

#include "ior.h"
#include "iordef.h"                                     /* IOR Definitions */

/*************************** D E F I N I T I O N S ****************************/

                                /* -- file open flags -- */
#define IOR_RDONLY        1     /* read only */
#define IOR_WRONLY        2     /* write only */
#define IOR_RDWR          4     /* read/write */
#define IOR_APPEND        8     /* append */
#define IOR_CREAT         16    /* create */
#define IOR_TRUNC         32    /* truncate */
#define IOR_EXCL          64    /* exclusive */
#define IOR_DIRECT        128   /* bypass I/O buffers */
                                /* -- file mode flags -- */
#define IOR_IRWXU         1     /* read, write, execute perm: owner */
#define IOR_IRUSR         2     /* read permission: owner */
#define IOR_IWUSR         4     /* write permission: owner */
#define IOR_IXUSR         8     /* execute permission: owner */
#define IOR_IRWXG         16    /* read, write, execute perm: group */
#define IOR_IRGRP         32    /* read permission: group */
#define IOR_IWGRP         64    /* write permission: group */
#define IOR_IXGRP         128   /* execute permission: group */
#define IOR_IRWXO         256   /* read, write, execute perm: other */
#define IOR_IROTH         512   /* read permission: other */
#define IOR_IWOTH         1024  /* write permission: other */
#define IOR_IXOTH         2048  /* execute permission: other */

typedef struct ior_aiori {
        char *name;
        void *(*create)(char *, IOR_param_t *);
        void *(*open)(char *, IOR_param_t *);
        IOR_offset_t (*xfer)(int, void *, IOR_size_t *,
                             IOR_offset_t, IOR_param_t *);
        void (*close)(void *, IOR_param_t *);
        void (*delete)(char *, IOR_param_t *);
        void (*set_version)(IOR_param_t *);
        void (*fsync)(void *, IOR_param_t *);
        IOR_offset_t (*get_file_size)(IOR_param_t *, MPI_Comm, char *);
} ior_aiori_t;

ior_aiori_t posix_aiori;
ior_aiori_t mpiio_aiori;
ior_aiori_t hdf5_aiori;
ior_aiori_t ncmpi_aiori;

IOR_offset_t MPIIO_GetFileSize(IOR_param_t * test, MPI_Comm testComm,
                               char *testFileName);

#endif /* not _AIORI_H */
