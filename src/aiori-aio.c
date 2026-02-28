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
#include <string.h>

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

  void ** buffer_pool;
  int * free_pool_indices;
  int free_pool_pos;
  int pool_size;
  IOR_offset_t pool_buffer_size;
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

  o->buffer_pool = NULL;
  o->free_pool_indices = NULL;
  o->free_pool_pos = 0;
  o->pool_size = 0;
  o->pool_buffer_size = 0;
}

static void aio_free_pool(aio_options_t * o){
  if (o->buffer_pool){
    for (int i = 0; i < o->pool_size; i++){
      aligned_buffer_free(o->buffer_pool[i], 0);
    }
    free(o->buffer_pool);
    o->buffer_pool = NULL;
  }
  if (o->free_pool_indices){
    free(o->free_pool_indices);
    o->free_pool_indices = NULL;
  }
  o->pool_size = 0;
  o->pool_buffer_size = 0;
}

static void complete_all(aio_options_t * o);

static void aio_setup_pool(aio_options_t * o, IOR_offset_t size){
  if (o->buffer_pool && o->pool_buffer_size >= size) return;
  complete_all(o);
  aio_free_pool(o);
  o->buffer_pool = malloc(sizeof(void *) * o->max_pending);
  o->free_pool_indices = malloc(sizeof(int) * o->max_pending);
  for (int i = 0; i < o->max_pending; i++){
    o->buffer_pool[i] = aligned_buffer_alloc(size, 0);
    o->free_pool_indices[i] = i;
  }
  o->free_pool_pos = o->max_pending;
  o->pool_size = o->max_pending;
  o->pool_buffer_size = size;
}

static void aio_finalize(aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  complete_all(o);
  aio_free_pool(o);
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

static void aio_reap_one(aio_options_t * o, struct io_event * event){
  if((long)event->res < 0){
    ERRF("AIO, error in io_event result: %s", strerror(-(long)event->res));
  }else{
    o->pending_bytes -= event->res;
  }
  struct iocb * iocb = (struct iocb *) event->obj;
  int pool_idx = (int)(uintptr_t) iocb->data;
  if (pool_idx != -1){
    o->free_pool_indices[o->free_pool_pos++] = pool_idx;
  }
  free(iocb);
}

/* complete all pending ops */
static void complete_all(aio_options_t * o){
  submit_pending(o);

  while(o->in_flight > 0){
    int to_reap = o->in_flight > 512 ? 512 : o->in_flight;
    struct io_event events[to_reap];
    int num_events;
    num_events = io_getevents(o->ioctx, 1, to_reap, events, NULL);
    if(num_events < 0){
      if(errno == EINTR) continue;
      ERRF("AIO, error in io_getevents(): %s", strerror(errno));
    }
    for (int i = 0; i < num_events; i++) {
      aio_reap_one(o, & events[i]);
    }
    o->in_flight -= num_events;
  }
  if(o->pending_bytes != 0){
    ERRF("AIO, error in flushing data, pending bytes: %lld", o->pending_bytes);
  }
}

/* called if we must make *some* progress */
static void process_some(aio_options_t * o){
  if(o->in_flight == 0){
    return;
  }
  /* we must submit anything that is pending otherwise we might block forever
     waiting for more events than actually in flight in the kernel */
  submit_pending(o);

  int to_reap = o->in_flight < o->granularity ? o->in_flight : o->granularity;
  if(to_reap > 512) to_reap = 512;
  struct io_event events[to_reap];
  int num_events;
  num_events = io_getevents(o->ioctx, to_reap, to_reap, events, NULL);
  if(num_events < 0){
    if(errno == EINTR) return;
    ERRF("AIO, error in io_getevents(): %s", strerror(errno));
  }
  //printf("Completed: %d\n", num_events);
  for (int i = 0; i < num_events; i++) {
    aio_reap_one(o, & events[i]);
  }
  o->in_flight -= num_events;
}

static IOR_offset_t aio_Xfer(int access, aiori_fd_t *fd, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param){
  aio_options_t * o = (aio_options_t*) param;
  aio_fd_t * afd = (aio_fd_t*) fd;

  /* If we are doing verification, we must be synchronous because IOR expects
     the data to be in the buffer immediately after this call. */
  if (access == READCHECK || access == WRITECHECK){
    complete_all(o); // ensure no races with previous ops
    struct iocb iocb;
    struct iocb * piocb = & iocb;
    io_prep_pread(piocb, *(int*)afd->pfd, buffer, length, offset);
    piocb->data = (void*)(uintptr_t)-1; // not from pool
    int res = io_submit(o->ioctx, 1, & piocb);
    if (res != 1) ERRF("AIO submit failed: %s", strerror(errno));
    struct io_event event;
    res = io_getevents(o->ioctx, 1, 1, & event, NULL);
    if (res != 1) ERRF("AIO getevents failed: %s", strerror(errno));
    if ((long)event.res < 0) ERRF("AIO error: %s", strerror(-(long)event.res));
    return event.res;
  }

  if(o->in_flight >= o->max_pending){
    process_some(o);
  }

  // ensure pool is ready
  aio_setup_pool(o, length);

  // get a buffer from pool. If none available (shouldn't happen because of process_some), wait.
  while (o->free_pool_pos == 0){
    process_some(o);
  }
  int pool_idx = o->free_pool_indices[--o->free_pool_pos];
  void * pbuf = o->buffer_pool[pool_idx];

  if (access == WRITE){
    memcpy(pbuf, buffer, length);
  }

  o->pending_bytes += length;

  struct iocb * iocb = malloc(sizeof(struct iocb));
  if(access == WRITE){
    io_prep_pwrite(iocb, *(int*)afd->pfd, pbuf, length, offset);
  }else{
    io_prep_pread(iocb,  *(int*)afd->pfd, pbuf, length, offset);
  }
  iocb->data = (void*)(uintptr_t)pool_idx;

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
