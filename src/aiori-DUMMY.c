/*
* Dummy implementation doesn't do anything besides waiting
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ior.h"
#include "aiori.h"
#include "utilities.h"

static void *DUMMY_Create(char *testFileName, IOR_param_t * param)
{
  return 0;
}

static void *DUMMY_Open(char *testFileName, IOR_param_t * param)
{
  return 0;
}

static void DUMMY_Fsync(void *fd, IOR_param_t * param)
{
}

static void DUMMY_Close(void *fd, IOR_param_t * param)
{
}

static void DUMMY_Delete(char *testFileName, IOR_param_t * param)
{
}

static void DUMMY_SetVersion(IOR_param_t * test)
{
  sprintf(test->apiVersion, "DUMMY");
}

static IOR_offset_t DUMMY_GetFileSize(IOR_param_t * test, MPI_Comm testComm, char *testFileName)
{        return 0;
}

static IOR_offset_t DUMMY_Xfer(int access, void *file, IOR_size_t * buffer, IOR_offset_t length, IOR_param_t * param){
  if (rank == 0){
    usleep(100000);
  }
  return length;
}


ior_aiori_t dummy_aiori = {
  .name = "DUMMY",
  .create = DUMMY_Create,
  .open = DUMMY_Open,
  .xfer = DUMMY_Xfer,
  .close = DUMMY_Close,
  .delete = DUMMY_Delete,
  .set_version = DUMMY_SetVersion,
  .fsync = DUMMY_Fsync,
  .get_file_size = DUMMY_GetFileSize,
};
