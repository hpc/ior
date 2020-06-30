/*
* S3 implementation using the newer libs3
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

static option_help * S3_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  dummy_options_t * o = malloc(sizeof(dummy_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(dummy_options_t));
  }else{
    memset(o, 0, sizeof(dummy_options_t));
  }

  *init_backend_options = (aiori_mod_opt_t*) o;

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

static int count_init = 0;

static aiori_fd_t *S3_Create(char *testFileName, int iorflags, aiori_mod_opt_t * options)
{
  if(count_init <= 0){
    ERR("S3 missing initialization in create\n");
  }
  if(verbose > 4){
    fprintf(out_logfile, "S3 create: %s = %p\n", testFileName, current);
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

static aiori_fd_t *S3_Open(char *testFileName, int flags, aiori_mod_opt_t * options)
{
  if(count_init <= 0){
    ERR("S3 missing initialization in open\n");
  }
  if(verbose > 4){
    fprintf(out_logfile, "S3 open: %s = %p\n", testFileName, current);
  }
  return (aiori_fd_t*) current++;
}

static void S3_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * options)
{
  if(verbose > 4){
    fprintf(out_logfile, "S3 fsync %p\n", fd);
  }
}


static void S3_Sync(aiori_mod_opt_t * options)
{
}

static void S3_Close(aiori_fd_t *fd, aiori_mod_opt_t * options)
{
  if(verbose > 4){
    fprintf(out_logfile, "S3 close %p\n", fd);
  }
}

static void S3_Delete(char *testFileName, aiori_mod_opt_t * options)
{
    if(verbose > 4){
      fprintf(out_logfile, "S3 delete: %s\n", testFileName);
    }
}

static char * S3_getVersion()
{
  return "0.5";
}

static IOR_offset_t S3_GetFileSize(aiori_mod_opt_t * options, char *testFileName)
{
  if(verbose > 4){
    fprintf(out_logfile, "S3 getFileSize: %s\n", testFileName);
  }
  return 0;
}

static IOR_offset_t S3_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer, IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * options){
  if(verbose > 4){
    fprintf(out_logfile, "S3 xfer: %p\n", file);
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

static int S3_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options){
  stat->f_bsize = 1;
  stat->f_blocks = 1;
  stat->f_bfree = 1;
  stat->f_bavail = 1;
  stat->f_files = 1;
  stat->f_ffree = 1;
  return 0;
}

static int S3_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options){
  return 0;
}

static int S3_rmdir (const char *path, aiori_mod_opt_t * options){
  return 0;
}

static int S3_access (const char *path, int mode, aiori_mod_opt_t * options){
  return 0;
}

static int S3_stat (const char *path, struct stat *buf, aiori_mod_opt_t * options){
  return 0;
}

static int S3_check_params(aiori_mod_opt_t * options){
  return 0;
}

static void S3_init(aiori_mod_opt_t * options){
  WARN("S3 initialized");
  count_init++;
}

static void S3_final(aiori_mod_opt_t * options){
  WARN("S3 finalized");
  if(count_init <= 0){
    ERR("S3 invalid finalization\n");
  }
  count_init--;
}


ior_aiori_t S3_libS3_aiori = {
        .name = "S3-libs3",
        .name_legacy = NULL,
        .create = S3_Create,
        .open = S3_Open,
        .xfer = S3_Xfer,
        .close = S3_Close,
        .delete = S3_Delete,
        .get_version = S3_getVersion,
        .fsync = S3_Fsync,
        .get_file_size = S3_GetFileSize,
        .statfs = S3_statfs,
        .mkdir = S3_mkdir,
        .rmdir = S3_rmdir,
        .access = S3_access,
        .stat = S3_stat,
        .initialize = S3_init,
        .finalize = S3_final,
        .get_options = S3_options,
        .check_params = S3_check_params,
        .sync = S3_Sync,
        .enable_mdtest = true
};
