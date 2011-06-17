/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: IOR.h,v $
*   $Revision: 1.2 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       This is a header file that contains the definitions and prototypes
*       needed for IOR.c.
*
\******************************************************************************/

#ifndef _IOR_H
#define _IOR_H

#include "aiori.h"                                 /* abstract IOR interfaces */
#include "iordef.h"                                /* IOR Definitions */


/*************************** D E F I N I T I O N S ****************************/

/* define the queuing structure for the test parameters */
typedef struct IOR_queue_t {
    IOR_param_t testParameters;
    struct IOR_queue_t * nextTest;
} IOR_queue_t;


/**************************** P R O T O T Y P E S *****************************/

/* functions used in IOR.c */
void           AioriBind        (char *);
void           CheckForOutliers (IOR_param_t *, double **, int, int);
void           CheckFileSize    (IOR_param_t *, IOR_offset_t, int);
char *         CheckTorF        (char *);
size_t         CompareBuffers   (void *, void *, size_t,
                                 IOR_offset_t, IOR_param_t *, int);
int            CountErrors      (IOR_param_t *, int, int);
int            CountTasksPerNode(int, MPI_Comm);
void *         CreateBuffer     (size_t);
IOR_queue_t *  CreateNewTest    (int);
void           DelaySecs        (int);
void           DisplayFreespace (IOR_param_t *);
void           DisplayOutliers  (int, double, char *, int, int);
void           DisplayUsage     (char **);
void           DistributeHints  (void);
void           FillBuffer       (void *, IOR_param_t *,
                                 unsigned long long, int);
void           FreeBuffers      (int, void *, void *, void *, IOR_offset_t *);
void           GetPlatformName  (char *);
void           GetTestFileName  (char *, IOR_param_t *);
double         GetTimeStamp     (void);
char *         HumanReadable    (IOR_offset_t, int);
IOR_offset_t   IOR_GetFileSize_POSIX (IOR_param_t *, MPI_Comm, char *);
IOR_offset_t   IOR_GetFileSize_MPIIO (IOR_param_t *, MPI_Comm, char *);
char *         LowerCase        (char *);
void           OutputToRoot     (int, MPI_Comm, char *);
void           PPDouble         (int, double, char *);
char *         PrependDir       (IOR_param_t *, char *);
IOR_queue_t *  ParseCommandLine (int, char **);
char **        ParseFileName    (char *, int *);
void           ReadCheck        (void *, void *, void *, void *, IOR_param_t *,
                                 IOR_offset_t, IOR_offset_t, IOR_offset_t *,
                                 IOR_offset_t *, int, int *);
void           ReduceIterResults(IOR_param_t *, double **, int, int);
int            Regex            (char *, char *);
void           RemoveFile       (char *, int, IOR_param_t *);
int            Regex            (char *, char *);
void           SetupXferBuffers (void **, void **, void **,
                                 IOR_param_t *, int, int);
IOR_queue_t *  SetupTests       (int, char **);
void           ShowFileSystemSize (char *);
void           ShowInfo         (int, char **, IOR_param_t *);
void           ShowSetup        (IOR_param_t *);
void           ShowTest         (IOR_param_t *);
void           SummarizeResults (IOR_param_t *);
void           TestIoSys        (IOR_param_t *);
double         TimeDeviation    (void);
void           ValidTests       (IOR_param_t *);
IOR_offset_t   WriteOrRead      (IOR_param_t *, void *, int);
void           WriteTimes       (IOR_param_t *, double **, int, int);


/* functions used in utilities.c */
char *         CurrentTimeString(void);
void           DumpBuffer       (void *, size_t);
void           ExtractHint      (char *, char *, char *);
void           SetHints         (MPI_Info *, char *);
void           ShowHints        (MPI_Info *);
IOR_offset_t   StringToBytes    (char *);

#if USE_UNDOC_OPT
void           CorruptFile      (char *, IOR_param_t *, int, int);
void           ModifyByteInFile (char *, IOR_offset_t, int);
#endif /* USE_UNDOC_OPTS */
void           SeedRandGen      (MPI_Comm);

#endif /* not _IOR_H */
