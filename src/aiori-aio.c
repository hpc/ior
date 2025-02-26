/*
 This backend uses linux-aio
 Requires: libaio-dev
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <libaio.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>

#include "ior.h"
#include "aiori.h"
#include "iordef.h"
#include "utilities.h"

#include "aiori-POSIX.h"

/************************** O P T I O N S *****************************/
typedef struct{
  aiori_mod_opt_t * p; // posix options
  int max_pending;
  int granularity; // how frequent to submit, submit ever granularity elements

  // runtime data
  io_context_t ioctx; // one context per fs
  struct iocb ** iocbs;
  int iocbs_pos; // how many are pending in iocbs

  int in_flight; // total pending ops
  IOR_offset_t pending_bytes; // track pending IO volume for error checking
} aio_options_t;

option_help * aio_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  aio_options_t * o = malloc(sizeof(aio_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(aio_options_t));
  }else{
    memset(o, 0, sizeof(aio_options_t));
    o->max_pending = 128;
    o->granularity = 16;
  }
  option_help * p_help = POSIX_options((aiori_mod_opt_t**)& o->p, init_values == NULL ? NULL : (aiori_mod_opt_t*) ((aio_options_t*)init_values)->p);
  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
    {0, "aio.max-pending", "Max number of pending ops", OPTION_OPTIONAL_ARGUMENT, 'd', & o->max_pending},
    {0, "aio.granularity", "How frequent to submit pending IOs, submit every *granularity* elements", OPTION_OPTIONAL_ARGUMENT, 'd', & o->granularity},
    LAST_OPTION
  };
  option_help * help = option_merge(h, p_help);
  free(p_help);
  return help;
}


/************************** D E C L A R A T I O N S ***************************/

typedef struct{
  aiori_fd_t * pfd; // the underlying POSIX fd
} aio_fd_t;

/***************************** F U N C T I O N S ******************************/

static aiori_xfer_hint_t * hints = NULL;

static void aio_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
  POSIX_xfer_hints(params);
}

static void aio_initialize(aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  if(io_setup(o->max_pending, & o->ioctx) != 0){
    ERRF("Couldn't initialize io context %s", strerror(errno));
  }

  o->iocbs = malloc(sizeof(struct iocb *) * o->granularity);
  o->iocbs_pos = 0;
  o->in_flight = 0;
}

static void aio_finalize(aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  io_destroy(o->ioctx);
}

static int aio_check_params(aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  POSIX_check_params((aiori_mod_opt_t*) o->p);
  if(o->max_pending < 8){
    ERRF("AIO max-pending = %d < 8", o->max_pending);
  }
  if(o->granularity > o->max_pending){
    ERRF("AIO granularity must be < max-pending, is %d > %d", o->granularity, o->max_pending);
  }
  return 0;
}

static aiori_fd_t *aio_Open(char *testFileName, int flags, aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  aio_fd_t * fd = malloc(sizeof(aio_fd_t));
  fd->pfd = POSIX_Open(testFileName, flags, o->p);
  return (aiori_fd_t*) fd;
}

static aiori_fd_t *aio_create(char *testFileName, int flags, aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  aio_fd_t * fd = malloc(sizeof(aio_fd_t));
  fd->pfd = POSIX_Create(testFileName, flags, o->p);
  return (aiori_fd_t*) fd;
}

/* called whenever the granularity is met */
static void submit_pending(aio_options_t * o){
  if(o->iocbs_pos == 0){
    return;
  }
  int res;
  res = io_submit(o->ioctx, o->iocbs_pos, o->iocbs);
  //printf("AIO submit %d jobs\n", o->iocbs_pos);
  if(res != o->iocbs_pos){
    if(errno == EAGAIN){
      ERR("AIO: errno == EAGAIN; this should't happen");
    }
    ERRF("AIO: submitted %d, error: \"%s\" ; this should't happen", res, strerror(errno));
  }
  o->iocbs_pos = 0;
}

/* complete all pending ops */
static void complete_all(aio_options_t * o){
  submit_pending(o);

  struct io_event events[o->in_flight];
  int num_events;
  num_events = io_getevents(o->ioctx, o->in_flight, o->in_flight, events, NULL);
  for (int i = 0; i < num_events; i++) {
    struct io_event event = events[i];
    if(event.res == -1){
      ERR("AIO, error in io_getevents(), IO incomplete!");
    }else{
      o->pending_bytes -= event.res;
    }
    free(event.obj);
  }
  if(o->pending_bytes != 0){
    ERRF("AIO, error in flushing data, pending bytes: %lld", o->pending_bytes);
  }
  o->in_flight = 0;
}

/* called if we must make *some* progress */
static void process_some(aio_options_t * o){
  if(o->in_flight == 0){
    return;
  }
  struct io_event events[o->in_flight];
  int num_events;
  int mn = o->in_flight < o->granularity ? o->in_flight : o->granularity;
  num_events = io_getevents(o->ioctx, mn, o->in_flight, events, NULL);
  //printf("Completed: %d\n", num_events);
  for (int i = 0; i < num_events; i++) {
    struct io_event event = events[i];
    if(event.res == -1){
      ERR("AIO, error in io_getevents(), IO incomplete!");
    }else{
      o->pending_bytes -= event.res;
    }
    free(event.obj);
  }
  o->in_flight -= num_events;
}

static IOR_offset_t aio_Xfer(int access, aiori_fd_t *fd, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  aio_fd_t * afd = (aio_fd_t*) fd;

  if(o->in_flight >= o->max_pending){
    process_some(o);
  }
  o->pending_bytes += length;

  struct iocb * iocb = malloc(sizeof(struct iocb));
  if(access == WRITE){
    io_prep_pwrite(iocb, *(int*)afd->pfd, buffer, length, offset);
  }else{
    io_prep_pread(iocb,  *(int*)afd->pfd, buffer, length, offset);
  }
  o->iocbs[o->iocbs_pos] = iocb;
  o->iocbs_pos++;
  o->in_flight++;

  if(o->iocbs_pos == o->granularity){
    submit_pending(o);
  }
  return length;
}

static void aio_Close(aiori_fd_t *fd, aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  aio_fd_t * afd = (aio_fd_t*) fd;
  complete_all(o);
  POSIX_Close(afd->pfd, o->p);
}

static void aio_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  complete_all(o);
  aio_fd_t * afd = (aio_fd_t*) fd;
  POSIX_Fsync(afd->pfd, o->p);
}

static void aio_Sync(aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  complete_all(o);
  POSIX_Sync((aiori_mod_opt_t*) o->p);
}



ior_aiori_t aio_aiori = {
        .name = "AIO",
        .name_legacy = NULL,
        .create = aio_create,
        .get_options = aio_options,
        .initialize = aio_initialize,
        .finalize = aio_finalize,
        .xfer_hints = aio_xfer_hints,
        .fsync = aio_Fsync,
        .open = aio_Open,
        .xfer = aio_Xfer,
        .close = aio_Close,
        .sync = aio_Sync,
        .check_params = aio_check_params,
        .remove = POSIX_Delete,
        .get_version = aiori_get_version,
        .get_file_size = POSIX_GetFileSize,
        .statfs = aiori_posix_statfs,
        .mkdir = aiori_posix_mkdir,
        .rmdir = aiori_posix_rmdir,
        .access = aiori_posix_access,
        .stat = aiori_posix_stat,
        .enable_mdtest = true
};
