#ifndef AIORI_POSIX_H
#define AIORI_POSIX_H

#include "aiori.h"
#ifdef HAVE_LUSTRE_USER
#  ifdef HAVE_LINUX_LUSTRE_LUSTRE_USER_H
#    include <linux/lustre/lustre_user.h>
#  elif defined(HAVE_LUSTRE_LUSTRE_USER_H)
#    include <lustre/lustre_user.h>
#  endif
#define LUSTRE_POOL_NAME_MAX LOV_MAXPOOLNAME
#else
#define LUSTRE_POOL_NAME_MAX 15
#endif /* HAVE_LUSTRE_USER */
#ifdef HAVE_LUSTRE_LUSTREAPI
#include <lustre/lustreapi.h>
#endif /* HAVE_LUSTRE_LUSTREAPI */

/************************** O P T I O N S *****************************/
typedef struct{
  /* in case of a change, please update depending MMAP module too */
  int direct_io;

  /* Lustre variables */
  int lustre_set_striping;         /* flag that we need to set lustre striping */
  int lustre_stripe_count;
  int lustre_set_pool;            /* flag that we need to set a lustre pool */
  char * lustre_pool;
  int lustre_stripe_size;
  int lustre_start_ost;
  int lustre_ignore_locks;

  /* gpfs variables */
  int gpfs_hint_access;          /* use gpfs "access range" hint */
  int gpfs_release_token;        /* immediately release GPFS tokens after
                                    creating or opening a file */
  int gpfs_finegrain_writesharing;  /* Enable fine grain write sharing */
  int gpfs_finegrain_readsharing;   /* Enable fine grain read sharing */
  int gpfs_createsharing;        /* Enable efficient file creation in
                                    a shared directory */

  /* beegfs variables */
  int beegfs_numTargets;           /* number storage targets to use */
  int beegfs_chunkSize;            /* srtipe pattern for new files */
  int gpuDirect;
  int range_locks;                 /* use POSIX range locks for writes */
} posix_options_t;

void POSIX_Sync(aiori_mod_opt_t * param);
int POSIX_check_params(aiori_mod_opt_t * param);
void POSIX_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
int POSIX_check_params(aiori_mod_opt_t * options);
aiori_fd_t *POSIX_Create(char *testFileName, int flags, aiori_mod_opt_t * module_options);
int POSIX_Mknod(char *testFileName);
aiori_fd_t *POSIX_Open(char *testFileName, int flags, aiori_mod_opt_t * module_options);
IOR_offset_t POSIX_GetFileSize(aiori_mod_opt_t * test, char *testFileName);
void POSIX_Delete(char *testFileName, aiori_mod_opt_t * module_options);
int POSIX_Rename(const char *oldfile, const char *newfile, aiori_mod_opt_t * module_options);
void POSIX_Close(aiori_fd_t *fd, aiori_mod_opt_t * module_options);
option_help * POSIX_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values);
void POSIX_xfer_hints(aiori_xfer_hint_t * params);


#endif
