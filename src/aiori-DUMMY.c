/*
* Dummy implementation doesn't do anything besides waiting
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "ior.h"
#include "aiori.h"
#include "utilities.h"


/************************** O P T I O N S *****************************/
typedef struct {
  uint64_t delay_creates;
  uint64_t delay_xfer;
  int delay_rank_0_only;
} dummy_options_t;

static char * current = (char*) 1;

static option_help * DUMMY_options(void ** init_backend_options, void * init_values){
  dummy_options_t * o = malloc(sizeof(dummy_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(dummy_options_t));
  }else{
    memset(o, 0, sizeof(dummy_options_t));
  }

  *init_backend_options = o;

  option_help h [] = {
      {0, "dummy.delay-create",        "Delay per create in usec", OPTION_OPTIONAL_ARGUMENT, 'l', & o->delay_creates},
      {0, "dummy.delay-xfer",          "Delay per xfer in usec", OPTION_OPTIONAL_ARGUMENT, 'l', & o->delay_xfer},
      {0, "dummy.delay-only-rank0",    "Delay only Rank0", OPTION_FLAG, 'd', & o->delay_rank_0_only},
      LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

static void *DUMMY_Create(char *testFileName, IOR_param_t * param)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY create: %s = %p\n", testFileName, current);
  }
  dummy_options_t * o = (dummy_options_t*) param->backend_options;
  if (o->delay_creates){
    if (! o->delay_rank_0_only || (o->delay_rank_0_only && rank == 0)){
      struct timespec wait = { o->delay_creates / 1000 / 1000, 1000l * (o->delay_creates % 1000000)};
      nanosleep( & wait, NULL);
    }
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

static char * DUMMY_getVersion()
{
  return "0.5";
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
  dummy_options_t * o = (dummy_options_t*) param->backend_options;
  if (o->delay_xfer){
    if (! o->delay_rank_0_only || (o->delay_rank_0_only && rank == 0)){
      struct timespec wait = {o->delay_xfer / 1000 / 1000, 1000l * (o->delay_xfer % 1000000)};
      nanosleep( & wait, NULL);
    }
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

static int DUMMY_check_params(IOR_param_t * test){  
  return 1;
}

ior_aiori_t dummy_aiori = {
        .name = "DUMMY",
        .name_legacy = NULL,
        .create = DUMMY_Create,
        .open = DUMMY_Open,
        .xfer = DUMMY_Xfer,
        .close = DUMMY_Close,
        .delete = DUMMY_Delete,
        .get_version = DUMMY_getVersion,
        .fsync = DUMMY_Fsync,
        .get_file_size = DUMMY_GetFileSize,
        .statfs = DUMMY_statfs,
        .mkdir = DUMMY_mkdir,
        .rmdir = DUMMY_rmdir,
        .access = DUMMY_access,
        .stat = DUMMY_stat,
        .initialize = NULL,
        .finalize = NULL,
        .get_options = DUMMY_options,
        .enable_mdtest = true,
        .check_params = DUMMY_check_params
};
