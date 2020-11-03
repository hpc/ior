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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdbool.h>

#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif

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
#ifdef USE_AIO_AIORI
        &aio_aiori,
#endif
#ifdef USE_PMDK_AIORI
        &pmdk_aiori,
#endif
#ifdef USE_DAOS_AIORI
        &dfs_aiori,
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
#ifdef USE_S3_LIBS3_AIORI
        &S3_libS3_aiori,
#endif
#ifdef USE_S3_4C_AIORI
        &s3_4c_aiori,
        &s3_plus_aiori,
        &s3_emc_aiori,
#endif
#ifdef USE_RADOS_AIORI
        &rados_aiori,
#endif
#ifdef USE_CEPHFS_AIORI
        &cephfs_aiori,
#endif
#ifdef USE_GFARM_AIORI
        &gfarm_aiori,
#endif
        NULL
};

void * airoi_update_module_options(const ior_aiori_t * backend, options_all_t * opt){
  if (backend->get_options == NULL)
    return NULL;
  char * name = backend->name;
  ior_aiori_t **tmp = available_aiori;
  for (int i=1; *tmp != NULL; ++tmp, i++) {
    if (strcmp(opt->modules[i].prefix, name) == 0){
      opt->modules[i].options = (*tmp)->get_options(& opt->modules[i].defaults,  opt->modules[i].defaults);
      return opt->modules[i].defaults;
    }
  }
  return NULL;
}

options_all_t * airoi_create_all_module_options(option_help * global_options){
  if(! out_logfile) out_logfile = stdout;
  int airoi_c = aiori_count();
  options_all_t * opt = malloc(sizeof(options_all_t));
  opt->module_count = airoi_c + 1;
  opt->modules = malloc(sizeof(option_module) * (airoi_c + 1));
  opt->modules[0].prefix = NULL;
  opt->modules[0].options = global_options;
  ior_aiori_t **tmp = available_aiori;
  for (int i=1; *tmp != NULL; ++tmp, i++) {
    opt->modules[i].prefix = (*tmp)->name;
    if((*tmp)->get_options != NULL){
      opt->modules[i].options = (*tmp)->get_options(& opt->modules[i].defaults, NULL);
    }else{
      opt->modules[i].options = NULL;
    }
  }
  return opt;
}

void aiori_supported_apis(char * APIs, char * APIs_legacy, enum bench_type type)
{
        ior_aiori_t **tmp = available_aiori;
        char delimiter = ' ';

        while (*tmp != NULL)
        {
                if ((type == MDTEST) && !(*tmp)->enable_mdtest)
                {
                    tmp++;
                    continue;
                }

                if (delimiter == ' ')
                {
                        APIs += sprintf(APIs, "%s", (*tmp)->name);
                        delimiter = '|';
                }
                else
                        APIs += sprintf(APIs, "%c%s", delimiter, (*tmp)->name);

                if ((*tmp)->name_legacy != NULL)
                        APIs_legacy += sprintf(APIs_legacy, "%c%s",
                                               delimiter, (*tmp)->name_legacy);
                tmp++;
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
int aiori_posix_statfs (const char *path, ior_aiori_statfs_t *stat_buf, aiori_mod_opt_t * module_options)
{
  // find the parent directory
  char * fileName = strdup(path);
  int i;
  int directoryFound = FALSE;

  /* get directory for outfile */
  i = strlen(fileName);
  while (i-- > 0) {
          if (fileName[i] == '/') {
                  fileName[i] = '\0';
                  directoryFound = TRUE;
                  break;
          }
  }
  /* if no directory/, use '.' */
  if (directoryFound == FALSE) {
    strcpy(fileName, ".");
  }

  int ret;
#if defined(HAVE_STATVFS)
  struct statvfs statfs_buf;

  ret = statvfs (fileName, &statfs_buf);
#else
  struct statfs statfs_buf;

  ret = statfs (fileName, &statfs_buf);
#endif
  if (-1 == ret) {
    perror("POSIX couldn't call statvfs");
    return -1;
  }

  stat_buf->f_bsize = statfs_buf.f_bsize;
  stat_buf->f_blocks = statfs_buf.f_blocks;
  stat_buf->f_bfree = statfs_buf.f_bfree;
  stat_buf->f_files = statfs_buf.f_files;
  stat_buf->f_ffree = statfs_buf.f_ffree;

  free(fileName);
  return 0;
}

int aiori_posix_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * module_options)
{
        return mkdir (path, mode);
}

int aiori_posix_rmdir (const char *path, aiori_mod_opt_t * module_options)
{
        return rmdir (path);
}

int aiori_posix_access (const char *path, int mode, aiori_mod_opt_t * module_options)
{
        return access (path, mode);
}

int aiori_posix_stat (const char *path, struct stat *buf, aiori_mod_opt_t * module_options)
{
        return stat (path, buf);
}

char* aiori_get_version()
{
  return "";
}

const ior_aiori_t *aiori_select (const char *api)
{
        char warn_str[256] = {0};
        for (ior_aiori_t **tmp = available_aiori ; *tmp != NULL; ++tmp) {
                char *name_leg = (*tmp)->name_legacy;
                if (NULL != api &&
                    (strcasecmp(api, (*tmp)->name) != 0) &&
                    (name_leg == NULL || strcasecmp(api, name_leg) != 0))
                    continue;

                if (name_leg != NULL && strcasecmp(api, name_leg) == 0)
                {
                        snprintf(warn_str, 256, "%s backend is deprecated use %s"
                                 " instead", api, (*tmp)->name);
                        WARN(warn_str);
                }

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
