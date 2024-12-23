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
  uint64_t delay_close;
  uint64_t delay_sync;
  uint64_t delay_xfer;
  int delay_rank_0_only;
} dummy_options_t;

static char * current = (char*) 1;

static option_help * DUMMY_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  dummy_options_t * o = malloc(sizeof(dummy_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(dummy_options_t));
  }else{
    memset(o, 0, sizeof(dummy_options_t));
  }

  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
      {0, "dummy.delay-create",        "Delay per create in usec", OPTION_OPTIONAL_ARGUMENT, 'l', & o->delay_creates},
      {0, "dummy.delay-close",         "Delay per close in usec", OPTION_OPTIONAL_ARGUMENT, 'l', & o->delay_close},
      {0, "dummy.delay-sync",          "Delay for sync in usec", OPTION_OPTIONAL_ARGUMENT, 'l', & o->delay_sync},
      {0, "dummy.delay-xfer",          "Delay per xfer in usec", OPTION_OPTIONAL_ARGUMENT, 'l', & o->delay_xfer},
      {0, "dummy.delay-only-rank0",    "Delay only Rank0", OPTION_FLAG, 'd', & o->delay_rank_0_only},
      LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

static int count_init = 0;

static aiori_fd_t *DUMMY_Create(char *testFileName, int iorflags, aiori_mod_opt_t * options)
{
  if(count_init <= 0){
    ERR("DUMMY missing initialization in create\n");
  }
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY create: %s = %p\n", testFileName, current);
  }
  dummy_options_t * o = (dummy_options_t*) options;
  if (o->delay_creates){
    if (! o->delay_rank_0_only || (o->delay_rank_0_only && rank == 0)){
      struct timespec wait = { o->delay_creates / 1000 / 1000, 1000l * (o->delay_creates % 1000000)};
      nanosleep( & wait, NULL);
    }
  }
  return (aiori_fd_t*) current++;
}

static aiori_fd_t *DUMMY_Open(char *testFileName, int flags, aiori_mod_opt_t * options)
{
  if(count_init <= 0){
    ERR("DUMMY missing initialization in open\n");
  }
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY open: %s = %p\n", testFileName, current);
  }
  return (aiori_fd_t*) current++;
}

static void DUMMY_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * options)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY fsync %p\n", fd);
  }
}


static void DUMMY_Sync(aiori_mod_opt_t * options)
{
  dummy_options_t * o = (dummy_options_t*) options;
  if (o->delay_sync){
    if (! o->delay_rank_0_only || (o->delay_rank_0_only && rank == 0)){
      struct timespec wait = { o->delay_sync / 1000 / 1000, 1000l * (o->delay_sync % 1000000)};
      nanosleep( & wait, NULL);
    }
  }
}

static void DUMMY_Close(aiori_fd_t *fd, aiori_mod_opt_t * options)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY close %p\n", fd);
  }
  
  dummy_options_t * o = (dummy_options_t*) options;
  if (o->delay_close){
    if (! o->delay_rank_0_only || (o->delay_rank_0_only && rank == 0)){
      struct timespec wait = { o->delay_close / 1000 / 1000, 1000l * (o->delay_close % 1000000)};
      nanosleep( & wait, NULL);
    }
  }
}

static void DUMMY_Delete(char *testFileName, aiori_mod_opt_t * options)
{
    if(verbose > 4){
      fprintf(out_logfile, "DUMMY delete: %s\n", testFileName);
    }
}

static char * DUMMY_getVersion()
{
  return "0.5";
}

static IOR_offset_t DUMMY_GetFileSize(aiori_mod_opt_t * options, char *testFileName)
{
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY getFileSize: %s\n", testFileName);
  }
  return 0;
}

static IOR_offset_t DUMMY_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer, IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * options){
  if(verbose > 4){
    fprintf(out_logfile, "DUMMY %d xfer: %p, %lld, %lld\n", rank, file, offset, length);
  }
  dummy_options_t * o = (dummy_options_t*) options;
  if (o->delay_xfer){
    if (! o->delay_rank_0_only || (o->delay_rank_0_only && rank == 0)){
      struct timespec wait = {o->delay_xfer / 1000 / 1000, 1000l * (o->delay_xfer % 1000000)};
      nanosleep( & wait, NULL);
    }
  }
  return length;
}

static int DUMMY_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options){
  stat->f_bsize = 1;
  stat->f_blocks = 1;
  stat->f_bfree = 1;
  stat->f_bavail = 1;
  stat->f_files = 1;
  stat->f_ffree = 1;
  return 0;
}

static int DUMMY_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options){
  return 0;
}

static int DUMMY_rmdir (const char *path, aiori_mod_opt_t * options){
  return 0;
}

static int DUMMY_access (const char *path, int mode, aiori_mod_opt_t * options){
  return 0;
}

static int DUMMY_stat (const char *path, struct stat *buf, aiori_mod_opt_t * options){
  return 0;
}

static int DUMMY_rename (const char *path, const char *path2, aiori_mod_opt_t * options){
  return 0;
}


static int DUMMY_check_params(aiori_mod_opt_t * options){
  return 0;
}

static void DUMMY_init(aiori_mod_opt_t * options){
  WARN("DUMMY initialized");
  count_init++;
}

static void DUMMY_final(aiori_mod_opt_t * options){
  WARN("DUMMY finalized");
  if(count_init <= 0){
    ERR("DUMMY invalid finalization\n");
  }
  count_init--;
}


ior_aiori_t dummy_aiori = {
        .name = "DUMMY",
        .name_legacy = NULL,
        .create = DUMMY_Create,
        .open = DUMMY_Open,
        .xfer = DUMMY_Xfer,
        .close = DUMMY_Close,
        .remove = DUMMY_Delete,
        .get_version = DUMMY_getVersion,
        .fsync = DUMMY_Fsync,
        .get_file_size = DUMMY_GetFileSize,
        .statfs = DUMMY_statfs,
        .mkdir = DUMMY_mkdir,
        .rmdir = DUMMY_rmdir,
        .rename = DUMMY_rename,
        .access = DUMMY_access,
        .stat = DUMMY_stat,
        .initialize = DUMMY_init,
        .finalize = DUMMY_final,
        .get_options = DUMMY_options,
        .check_params = DUMMY_check_params,
        .sync = DUMMY_Sync,
        .enable_mdtest = true
};
