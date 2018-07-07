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

static char * current = (char*) 1;

static void *DUMMY_Create(char *testFileName, IOR_param_t * param)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY create: %s = %p\n", testFileName, current);
  }
  if (rank == 0){
    usleep(100000);
  }  
  return current++;
}

static void *DUMMY_Open(char *testFileName, IOR_param_t * param)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY open: %s = %p\n", testFileName, current);
  }
  return current++;
}

static void DUMMY_Fsync(void *fd, IOR_param_t * param)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY fsync %p\n", fd);
  }
}

static void DUMMY_Close(void *fd, IOR_param_t * param)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY close %p\n", fd);
  }
}

static void DUMMY_Delete(char *testFileName, IOR_param_t * param)
{
    if(verbose > 4){
      fprintf(out_logfile, "DUMMY delete: %s\n", testFileName);
    }
}

static void DUMMY_SetVersion(IOR_param_t * test)
{
  sprintf(test->apiVersion, "DUMMY");
}

static IOR_offset_t DUMMY_GetFileSize(IOR_param_t * test, MPI_Comm testComm, char *testFileName)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY getFileSize: %s\n", testFileName);
  }
  return 0;
}

static IOR_offset_t DUMMY_Xfer(int access, void *file, IOR_size_t * buffer, IOR_offset_t length, IOR_param_t * param){
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY xfer: %p\n", file);
  }
  if (rank == 0){
    usleep(100000);
  }
  return length;
}

static int DUMMY_statfs (const char * path, ior_aiori_statfs_t * stat, IOR_param_t * param){
  stat->f_bsize = 1;
  stat->f_blocks = 1;
  stat->f_bfree = 1;
  stat->f_bavail = 1;
  stat->f_files = 1;
  stat->f_ffree = 1;
  return 0;
}

static int DUMMY_mkdir (const char *path, mode_t mode, IOR_param_t * param){
  return 0;
}

static int DUMMY_rmdir (const char *path, IOR_param_t * param){
  return 0;
}

static int DUMMY_access (const char *path, int mode, IOR_param_t * param){
  return 0;
}

static int DUMMY_stat (const char *path, struct stat *buf, IOR_param_t * param){
  return 0;
}

ior_aiori_t dummy_aiori = {
  "DUMMY",
  DUMMY_Create,
  DUMMY_Open,
  DUMMY_Xfer,
  DUMMY_Close,
  DUMMY_Delete,
  DUMMY_SetVersion,
  DUMMY_Fsync,
  DUMMY_GetFileSize,
  DUMMY_statfs,
  DUMMY_mkdir,
  DUMMY_rmdir,
  DUMMY_access,
  DUMMY_stat
};
