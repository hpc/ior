/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: aiori-noMPIIO.c,v $
*   $Revision: 1.1.1.1 $
*   $Date: 2007/10/15 23:36:54 $
*   $Author: rklundt $
*
* Purpose:
*       Empty MPIIO functions for when compiling without MPIIO support.
*
\******************************************************************************/

#include "aiori.h"

void *
IOR_Create_MPIIO(char        * testFileName,
                 IOR_param_t * param)
{ 
    ERR("This copy of IOR was not compiled with MPIIO support");
    return 0;
}

void *
IOR_Open_MPIIO(char        * testFileName,
               IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
    return 0;
}


IOR_offset_t
IOR_Xfer_MPIIO(int            access,
               void         * fd,
               IOR_size_t   * buffer,
               IOR_offset_t   length,
               IOR_param_t  * param)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
    return 0;
}

void
IOR_Fsync_MPIIO(void * fd, IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
}

void
IOR_Close_MPIIO(void        * fd,
                IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
}

void
IOR_Delete_MPIIO(char * testFileName, IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
}

void
IOR_SetVersion_MPIIO(IOR_param_t *test)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
}

IOR_offset_t
IOR_GetFileSize_MPIIO(IOR_param_t * test,
                      MPI_Comm   testComm,
                      char     * testFileName)
{
    ERR("This copy of IOR was not compiled with MPIIO support");
    return 0;
}
