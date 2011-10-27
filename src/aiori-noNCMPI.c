/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: aiori-noNCMPI.c,v $
*   $Revision: 1.2 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       Empty NCMPI functions for when compiling without NCMPI support.
*
\******************************************************************************/

#include "aiori.h"

void *
IOR_Create_NCMPI(char        * testFileName,
                 IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
    return 0;
}

void *
IOR_Open_NCMPI(char        * testFileName,
               IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
    return 0;
}


IOR_offset_t
IOR_Xfer_NCMPI(int            access,
               void         * fd,
               IOR_size_t   * buffer,
               IOR_offset_t   length,
               IOR_param_t  * param)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
    return 0;
}

void
IOR_Fsync_NCMPI(void * fd, IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
}

void
IOR_Close_NCMPI(void        * fd,
                IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
}

void
IOR_Delete_NCMPI(char * testFileName, IOR_param_t * param)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
}

void
IOR_SetVersion_NCMPI(IOR_param_t *test)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
}

IOR_offset_t
IOR_GetFileSize_NCMPI(IOR_param_t * test,
                      MPI_Comm   testComm,
                      char     * testFileName)
{
    ERR("This copy of IOR was not compiled with NCMPI support");
    return 0;
}
