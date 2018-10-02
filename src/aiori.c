/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Definitions and prototypes of abstract I/O interface
*
\******************************************************************************/

#include "aiori.h"

#if defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#endif

#if defined(HAVE_SYS_STATFS_H)
#include <sys/statfs.h>
#endif

/*
 * Bind the global "backend" pointer to the requested backend AIORI's
 * function table.
 */

ior_aiori_t *available_aiori[] = {
#ifdef USE_POSIX_AIORI
        &posix_aiori,
#endif
        & dummy_aiori,
#ifdef USE_HDF5_AIORI
        &hdf5_aiori,
#endif
#ifdef USE_HDFS_AIORI
        &hdfs_aiori,
#endif
#ifdef USE_IME_AIORI
        &ime_aiori,
#endif
#ifdef USE_MPIIO_AIORI
        &mpiio_aiori,
#endif
#ifdef USE_NCMPI_AIORI
        &ncmpi_aiori,
#endif
#ifdef USE_MMAP_AIORI
        &mmap_aiori,
#endif
#ifdef USE_S3_AIORI
        &s3_aiori,
        &s3_plus_aiori,
        &s3_emc_aiori,
#endif
#ifdef USE_RADOS_AIORI
        &rados_aiori,
#endif
        NULL
};

void airoi_parse_options(int argc, char ** argv, option_help * global_options){
    int airoi_c = aiori_count();
    options_all opt;
    opt.module_count = airoi_c + 1;
    opt.modules = malloc(sizeof(option_module) * (airoi_c + 1));
    opt.modules[0].prefix = NULL;
    opt.modules[0].options = global_options;
    ior_aiori_t **tmp = available_aiori;
    for (int i=1; *tmp != NULL; ++tmp, i++) {
      opt.modules[i].prefix = (*tmp)->name;
      if((*tmp)->get_options != NULL){
        opt.modules[i].options = (*tmp)->get_options();
      }else{
        opt.modules[i].options = NULL;
      }
    }
    option_parse(argc, argv, &opt);
    free(opt.modules);
}

void aiori_supported_apis(char * APIs){
  ior_aiori_t **tmp = available_aiori;
  if(*tmp != NULL){
    APIs += sprintf(APIs, "%s", (*tmp)->name);
    tmp++;
    for (; *tmp != NULL; ++tmp) {
      APIs += sprintf(APIs, "|%s", (*tmp)->name);
    }
  }
}

/**
 * Default statfs implementation.
 *
 * @param[in]  path       Path to run statfs on
 * @param[out] statfs_buf AIORI statfs buffer
 *
 * This function provides a AIORI statfs for POSIX-compliant filesystems. It
 * uses statvfs is available and falls back on statfs.
 */
int aiori_posix_statfs (const char *path, ior_aiori_statfs_t *stat_buf, IOR_param_t * param)
{
        int ret;
#if defined(HAVE_STATVFS)
        struct statvfs statfs_buf;

        ret = statvfs (path, &statfs_buf);
#else
        struct statfs statfs_buf;

        ret = statfs (path, &statfs_buf);
#endif
        if (-1 == ret) {
                return -1;
        }

        stat_buf->f_bsize = statfs_buf.f_bsize;
        stat_buf->f_blocks = statfs_buf.f_blocks;
        stat_buf->f_bfree = statfs_buf.f_bfree;
        stat_buf->f_files = statfs_buf.f_files;
        stat_buf->f_ffree = statfs_buf.f_ffree;

        return 0;
}

int aiori_posix_mkdir (const char *path, mode_t mode, IOR_param_t * param)
{
        return mkdir (path, mode);
}

int aiori_posix_rmdir (const char *path, IOR_param_t * param)
{
        return rmdir (path);
}

int aiori_posix_access (const char *path, int mode, IOR_param_t * param)
{
        return access (path, mode);
}

int aiori_posix_stat (const char *path, struct stat *buf, IOR_param_t * param)
{
        return stat (path, buf);
}

char* aiori_get_version()
{
  return "";
}

static int is_initialized = FALSE;

void aiori_initialize(){
	if (is_initialized) return;
	is_initialized = TRUE;

  /* Sanity check, we were compiled with SOME backend, right? */
  if (0 == aiori_count ()) {
          ERR("No IO backends compiled into aiori.  "
              "Run 'configure --with-<backend>', and recompile.");
  }

  for (ior_aiori_t **tmp = available_aiori ; *tmp != NULL; ++tmp) {
    if((*tmp)->initialize){
      (*tmp)->initialize();
    }
  }
}

void aiori_finalize(){
  if (! is_initialized) return;
  is_initialized = FALSE;

  for (ior_aiori_t **tmp = available_aiori ; *tmp != NULL; ++tmp) {
    if((*tmp)->finalize){
      (*tmp)->finalize();
    }
  }
}

const ior_aiori_t *aiori_select (const char *api)
{
        char warn_str[256] = {0};
        for (ior_aiori_t **tmp = available_aiori ; *tmp != NULL; ++tmp) {
                if (NULL == api || strcasecmp(api, (*tmp)->name) == 0) {
                        if (NULL == (*tmp)->statfs) {
                                (*tmp)->statfs = aiori_posix_statfs;
                                snprintf(warn_str, 256, "assuming POSIX-based backend for"
                                         " %s statfs call", api);
                                WARN(warn_str);
                        }
                        if (NULL == (*tmp)->mkdir) {
                                (*tmp)->mkdir = aiori_posix_mkdir;
                                snprintf(warn_str, 256, "assuming POSIX-based backend for"
                                         " %s mkdir call", api);
                                WARN(warn_str);
                        }
                        if (NULL == (*tmp)->rmdir) {
                                (*tmp)->rmdir = aiori_posix_rmdir;
                                snprintf(warn_str, 256, "assuming POSIX-based backend for"
                                         " %s rmdir call", api);
                                WARN(warn_str);
                        }
                        if (NULL == (*tmp)->access) {
                                (*tmp)->access = aiori_posix_access;
                                snprintf(warn_str, 256, "assuming POSIX-based backend for"
                                         " %s access call", api);
                                WARN(warn_str);
                        }
                        if (NULL == (*tmp)->stat) {
                                (*tmp)->stat = aiori_posix_stat;
                                snprintf(warn_str, 256, "assuming POSIX-based backend for"
                                         " %s stat call", api);
                                WARN(warn_str);
                        }
                        return *tmp;
                }
        }

        return NULL;
}

int aiori_count (void)
{
        return sizeof (available_aiori)/sizeof(available_aiori[0]) - 1;
}

const char *aiori_default (void)
{
        if (aiori_count () > 0) {
                return available_aiori[0]->name;
        }

        return NULL;
}
