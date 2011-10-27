/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: defaults.h,v $
*   $Revision: 1.3 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       This is a header file that contains the default settings necessary for
*       IOR.
*
\******************************************************************************/
/********************* Modifications to IOR-2.10.1 *****************************
* hodson - 8/18/2008: Added Default values for the following variables         *
*    int TestNum;                    * test reference number                   *
*    int taskPerNodeOffset;          * task node offset for reading files      *
*    int reorderTasksRandom;         * reorder tasks for random file read back *
*    int reorderTasksRandomSeed;     * reorder tasks for random file read seed *
*    int fsyncPerWrite;              * fsync() after each write                *
*******************************************************************************/

#ifndef _IOR_DEFAULTS_H
#define _IOR_DEFAULTS_H

#include "aiori.h"


/*************************** D E F I N I T I O N S ****************************/

/******************************************************************************/
/*
 * Default parameter settings for a test script.  This should be the minimum
 * test runnable.
 */

IOR_param_t defaultParameters = {
    "",              /* debug info string */
    IOR_IRUSR |      /* file permissions */
    IOR_IWUSR |  
    IOR_IRGRP |  
    IOR_IWGRP,
    IOR_RDWR |       /* open flags POSIX/MPIIO */ 
    IOR_CREAT,
    -1,              /* test reference number */
    "POSIX",         /* api */
    "",              /* api version */
    "HOST(OSTYPE)",  /* platform */
    "testFile",      /* test file */
    "",              /* filename for fpp read check */
    "",              /* hints file */
    "",              /* options */
    0,               /* numTasks */
    1,               /* nodes */
    1,               /* tasks per node */
    1,               /* repetitions */
    -1,              /* rep counter */
    0,               /* multiple files */
    0,               /* intertest delay */
    NULL, NULL,      /* write results array */
    NULL, NULL,      /* read results array */
    WRITE,           /* used in HDF5 for create(WRITE) and open(READ) */
    TRUE,            /* read flag */
    TRUE,            /* write flag */
    FALSE,           /* file-per-proc flag */
    FALSE,           /* reorder tasks */
    1,               /* task file offset for read */
    FALSE,           /* reorder tasks random */
    0,               /* reorder tasks random seed offset value*/
    FALSE,           /* check write */
    FALSE,           /* check read */
    FALSE,           /* keep test file on exit */
    FALSE,           /* keep test file with errors */
    FALSE,           /* error found in data check */
    FALSE,           /* halt on error */
    FALSE,           /* collective I/O */
    1,               /* segment count */
    1048576,         /* block size */
    262144,          /* transfer size */
    0,               /* offset */
    NULL,            /* expected aggregate file size array */
    NULL,            /* stat'ed aggregate file size array */
    NULL,            /* xfered aggregate file size array */
    NULL,            /* aggregate file size array used for b/w */
    FALSE,           /* preallocate file size */
    FALSE,           /* use file view */
    FALSE,           /* use shared file pointer */
    FALSE,           /* use strided datatype */
    FALSE,           /* use O_DIRECT, bypassing I/O buffers */
    FALSE,           /* show hints */
    FALSE,           /* show help */
    FALSE,           /* unique directory for each file-per-process */
    FALSE,           /* do not delete test file before access */
    FALSE,           /* use file offset as stored signature */
    0,               /* deadline in seconds for any test phase (0=off) */
    0,               /* max time in minutes to run tests (0=off) */
    0,               /* warn on outlier N seconds from mean (0=off) */
    0,               /* verbosity */
    0,               /* set time stamp signature */
    0,               /* time stamp signature value */
    NULL,            /* additional fd for fpp read check */
    -1,              /* random seed for write/read check */
    0,               /* access is to random, not sequential, offsets */
    MPI_COMM_WORLD,  /* MPI communicator */

    /* POSIX */
    FALSE,           /* single transfer (no retry) */
    FALSE,           /* fsync after each POSIX write  */
    FALSE,           /* fsync after POSIX write close */

    /* MPI */
    0,               /* MPI transfer datatype */
    0,               /* MPI file view datatype */

    /* HDF5 */
    FALSE,           /* individual data sets */
    FALSE,           /* no fill */
    1,               /* alignment */

    /* NCMPI */
    0,               /* var_id handle for datasets */

    /* Lustre */
    0,               /* lustre_stripe_count */
    0,               /* lustre_stripe_size */
    -1,              /* lustre_start_ost */
    0,               /* lustre_ignore_locks */

#if USE_UNDOC_OPT
    0,               /* corrupt file(s) */
    0,               /* fill the file system */
    0,               /* include delete time */
    0,               /* multiple rereads of file */

    /* NFS */
    "",              /* absolute path to NFS servers */
    "",              /* prefix for NFS server name */
    0,               /* number of NFS servers to be used */
#endif /* USE_UNDOC_OPT */

    0,               /* test's unique ID */
    0                /* intraTestBarriers */
};
#endif /* not _IOR_DEFAULTS_H */
