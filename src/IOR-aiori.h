/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Abstract prototypes
*
\******************************************************************************/

#ifndef _IOR_AIORI_H
#define _IOR_AIORI_H

#include "IOR.h"


/**************************** P R O T O T Y P E S *****************************/

/* abstract IOR interfaces used in aiori-*.c */
void *       (*IOR_Create)      (char *, IOR_param_t *);
void *       (*IOR_Open)        (char *, IOR_param_t *);
IOR_offset_t (*IOR_Xfer)        (int, void *, IOR_size_t *,
                                 IOR_offset_t, IOR_param_t *);
void         (*IOR_Close)       (void *, IOR_param_t *);
void         (*IOR_Delete)      (char *, IOR_param_t *);
void         (*IOR_SetVersion)  (IOR_param_t *);
void         (*IOR_Fsync)       (void *, IOR_param_t *);
IOR_offset_t (*IOR_GetFileSize) (IOR_param_t *, MPI_Comm, char *);

#endif /* not _IOR_AIORI_H */
