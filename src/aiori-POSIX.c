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
* Implement of abstract I/O interface for POSIX.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#  include <sys/ioctl.h>          /* necessary for: */
#  define __USE_GNU               /* O_DIRECT and */
#  include <fcntl.h>              /* IO operations */
#  undef __USE_GNU
#endif                          /* __linux__ */

#include <errno.h>
#include <fcntl.h>              /* IO operations */
#include <sys/stat.h>
#include <assert.h>


#ifdef HAVE_LINUX_LUSTRE_LUSTRE_USER_H
#  include <linux/lustre/lustre_user.h>
#elif defined(HAVE_LUSTRE_USER)
#  include <lustre/lustre_user.h>
#endif
#ifdef HAVE_GPFS_H
#  include <gpfs.h>
#endif
#ifdef HAVE_GPFS_FCNTL_H
#  include <gpfs_fcntl.h>
#endif

#ifdef HAVE_BEEGFS_BEEGFS_H
#include <beegfs/beegfs.h>
#include <dirent.h>
#include <libgen.h>
#endif

#include "ior.h"
#include "aiori.h"
#include "iordef.h"
#include "utilities.h"

#include "aiori-POSIX.h"

#ifndef   open64                /* necessary for TRU64 -- */
#  define open64  open            /* unlikely, but may pose */
#endif  /* not open64 */                        /* conflicting prototypes */

#ifndef   lseek64               /* necessary for TRU64 -- */
#  define lseek64 lseek           /* unlikely, but may pose */
#endif  /* not lseek64 */                        /* conflicting prototypes */

#ifndef   O_BINARY              /* Required on Windows    */
#  define O_BINARY 0
#endif

/**************************** P R O T O T Y P E S *****************************/
static IOR_offset_t POSIX_Xfer(int, aiori_fd_t *, IOR_size_t *,
                               IOR_offset_t, IOR_offset_t, aiori_mod_opt_t *);

option_help * POSIX_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  posix_options_t * o = malloc(sizeof(posix_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(posix_options_t));
  }else{
    memset(o, 0, sizeof(posix_options_t));
    o->direct_io = 0;
    o->lustre_stripe_count = -1;
    o->lustre_start_ost = -1;
    o->beegfs_numTargets = -1;
    o->beegfs_chunkSize = -1;
  }

  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
    {0, "posix.odirect", "Direct I/O Mode", OPTION_FLAG, 'd', & o->direct_io},
#ifdef HAVE_BEEGFS_BEEGFS_H
    {0, "posix.beegfs.NumTargets", "", OPTION_OPTIONAL_ARGUMENT, 'd', & o->beegfs_numTargets},
    {0, "posix.beegfs.ChunkSize", "", OPTION_OPTIONAL_ARGUMENT, 'd', & o->beegfs_chunkSize},
#endif
#ifdef HAVE_GPFS_FCNTL_H
    {0, "posix.gpfs.hintaccess", "", OPTION_FLAG, 'd', & o->gpfs_hint_access},
    {0, "posix.gpfs.releasetoken", "", OPTION_OPTIONAL_ARGUMENT, 'd', & o->gpfs_release_token},

#endif
#ifdef HAVE_LUSTRE_USER
    {0, "posix.lustre.stripecount", "", OPTION_OPTIONAL_ARGUMENT, 'd', & o->lustre_stripe_count},
    {0, "posix.lustre.stripesize", "", OPTION_OPTIONAL_ARGUMENT, 'd', & o->lustre_stripe_size},
    {0, "posix.lustre.startost", "", OPTION_OPTIONAL_ARGUMENT, 'd', & o->lustre_start_ost},
    {0, "posix.lustre.ignorelocks", "", OPTION_FLAG, 'd', & o->lustre_ignore_locks},
#endif
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}


/************************** D E C L A R A T I O N S ***************************/


ior_aiori_t posix_aiori = {
        .name = "POSIX",
        .name_legacy = NULL,
        .create = POSIX_Create,
        .mknod = POSIX_Mknod,
        .open = POSIX_Open,
        .xfer = POSIX_Xfer,
        .close = POSIX_Close,
        .delete = POSIX_Delete,
        .xfer_hints = POSIX_xfer_hints,
        .get_version = aiori_get_version,
        .fsync = POSIX_Fsync,
        .get_file_size = POSIX_GetFileSize,
        .statfs = aiori_posix_statfs,
        .mkdir = aiori_posix_mkdir,
        .rmdir = aiori_posix_rmdir,
        .access = aiori_posix_access,
        .stat = aiori_posix_stat,
        .get_options = POSIX_options,
        .enable_mdtest = true,
        .sync = POSIX_Sync,
        .check_params = POSIX_check_params
};

/***************************** F U N C T I O N S ******************************/

static aiori_xfer_hint_t * hints = NULL;

void POSIX_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
}

int POSIX_check_params(aiori_mod_opt_t * param){
  posix_options_t * o = (posix_options_t*) param;
  if (o->beegfs_chunkSize != -1 && (!ISPOWEROFTWO(o->beegfs_chunkSize) || o->beegfs_chunkSize < (1<<16)))
        ERR("beegfsChunkSize must be a power of two and >64k");
  if(o->lustre_stripe_count != -1 || o->lustre_stripe_size != 0)
    o->lustre_set_striping = 1;
  return 0;
}

#ifdef HAVE_GPFS_FCNTL_H
void gpfs_free_all_locks(int fd)
{
        int rc;
        struct {
                gpfsFcntlHeader_t header;
                gpfsFreeRange_t release;
        } release_all;
        release_all.header.totalLength = sizeof(release_all);
        release_all.header.fcntlVersion = GPFS_FCNTL_CURRENT_VERSION;
        release_all.header.fcntlReserved = 0;

        release_all.release.structLen = sizeof(release_all.release);
        release_all.release.structType = GPFS_FREE_RANGE;
        release_all.release.start = 0;
        release_all.release.length = 0;

        rc = gpfs_fcntl(fd, &release_all);
        if (verbose >= VERBOSE_0 && rc != 0) {
                EWARNF("gpfs_fcntl(%d, ...) release all locks hint failed.", fd);
        }
}
void gpfs_access_start(int fd, IOR_offset_t length, IOR_offset_t offset, int access)
{
        int rc;
        struct {
                gpfsFcntlHeader_t header;
                gpfsAccessRange_t access;
        } take_locks;

        take_locks.header.totalLength = sizeof(take_locks);
        take_locks.header.fcntlVersion = GPFS_FCNTL_CURRENT_VERSION;
        take_locks.header.fcntlReserved = 0;

        take_locks.access.structLen = sizeof(take_locks.access);
        take_locks.access.structType = GPFS_ACCESS_RANGE;
        take_locks.access.start = offset;
        take_locks.access.length = length;
        take_locks.access.isWrite = (access == WRITE);

        rc = gpfs_fcntl(fd, &take_locks);
        if (verbose >= VERBOSE_2 && rc != 0) {
                EWARNF("gpfs_fcntl(%d, ...) access range hint failed.", fd);
        }
}

void gpfs_access_end(int fd, IOR_offset_t length, IOR_offset_t offset,  int access)
{
        int rc;
        struct {
                gpfsFcntlHeader_t header;
                gpfsFreeRange_t free;
        } free_locks;


        free_locks.header.totalLength = sizeof(free_locks);
        free_locks.header.fcntlVersion = GPFS_FCNTL_CURRENT_VERSION;
        free_locks.header.fcntlReserved = 0;

        free_locks.free.structLen = sizeof(free_locks.free);
        free_locks.free.structType = GPFS_FREE_RANGE;
        free_locks.free.start = offset;
        free_locks.free.length = length;

        rc = gpfs_fcntl(fd, &free_locks);
        if (verbose >= VERBOSE_2 && rc != 0) {
                EWARNF("gpfs_fcntl(%d, ...) free range hint failed.", fd);
        }
}

#endif

#ifdef HAVE_BEEGFS_BEEGFS_H

int mkTempInDir(char* dirPath)
{
   unsigned long len = strlen(dirPath) + 8;
   char* tmpfilename = (char*)malloc(sizeof (char)*len+1);
   snprintf(tmpfilename, len, "%s/XXXXXX", dirPath);

   int fd = mkstemp(tmpfilename);
   unlink(tmpfilename);
   free(tmpfilename);

   return fd;
}

bool beegfs_getStriping(char* dirPath, u_int16_t* numTargetsOut, unsigned* chunkSizeOut)
{
   bool retVal = false;

   int fd = mkTempInDir(dirPath);
   if (fd) {
      unsigned stripePattern = 0;
      retVal = beegfs_getStripeInfo(fd, &stripePattern, chunkSizeOut, numTargetsOut);
      close(fd);
   }

   return retVal;
}

bool beegfs_isOptionSet(int opt) {
   return opt != -1;
}

bool beegfs_compatibleFileExists(char* filepath, int numTargets, int chunkSize)
{
      int fd = open(filepath, O_RDWR);

      if (fd == -1)
         return false;

      unsigned read_stripePattern = 0;
      u_int16_t read_numTargets = 0;
      int read_chunkSize = 0;

      bool retVal = beegfs_getStripeInfo(fd, &read_stripePattern, &read_chunkSize, &read_numTargets);

      close(fd);

      return retVal && read_numTargets == numTargets && read_chunkSize == chunkSize;
}

/*
 * Create a file on a BeeGFS file system with striping parameters
 */
bool beegfs_createFilePath(char* filepath, mode_t mode, int numTargets, int chunkSize)
{
        bool retVal = false;
        char* dirTmp = strdup(filepath);
        char* dir = dirname(dirTmp);
        DIR* parentDirS = opendir(dir);
        if (!parentDirS) {
                ERRF("Failed to get directory: %s", dir);
        }
        else
        {
                int parentDirFd = dirfd(parentDirS);
                if (parentDirFd < 0)
                {
                        ERRF("Failed to get directory descriptor: %s", dir);
                }
                else
                {
                        bool isBeegfs = beegfs_testIsBeeGFS(parentDirFd);
                        if (!isBeegfs)
                        {
                                WARN("Not a BeeGFS file system");
                        }
                        else
                        {
                                if (   !beegfs_isOptionSet(numTargets)
                                    || !beegfs_isOptionSet(chunkSize)) {
                                        u_int16_t defaultNumTargets = 0;
                                        unsigned  defaultChunkSize  = 0;
                                        bool haveDefaults = beegfs_getStriping(dir,
                                                                               &defaultNumTargets,
                                                                               &defaultChunkSize);
                                        if (!haveDefaults)
                                                ERR("Failed to get default BeeGFS striping values");

                                        numTargets = beegfs_isOptionSet(numTargets) ?
                                                        numTargets : defaultNumTargets;
                                        chunkSize  = beegfs_isOptionSet(chunkSize) ?
                                                        chunkSize  : defaultChunkSize;
                                }

                                char* filenameTmp = strdup(filepath);
                                char* filename = basename(filepath);
                                bool isFileCreated =    beegfs_compatibleFileExists(filepath, numTargets, chunkSize)
                                                     || beegfs_createFile(parentDirFd, filename,
                                                                          mode, numTargets, chunkSize);
                                if (!isFileCreated)
                                        ERR("Could not create file");
                                retVal = true;
                                free(filenameTmp);
                        }
                }
                closedir(parentDirS);
        }
        free(dirTmp);
        return retVal;
}
#endif /* HAVE_BEEGFS_BEEGFS_H */


/*
 * Create and open a file through the POSIX interface.
 */
aiori_fd_t *POSIX_Create(char *testFileName, int flags, aiori_mod_opt_t * param)
{
        int fd_oflag = O_BINARY;
        int mode = 0664;
        int *fd;

        fd = (int *)malloc(sizeof(int));
        if (fd == NULL)
                ERR("Unable to malloc file descriptor");
        posix_options_t * o = (posix_options_t*) param;
        if (o->direct_io == TRUE){
          set_o_direct_flag(&fd_oflag);
        }

        if(hints->dryRun)
          return (aiori_fd_t*) 0;

#ifdef HAVE_LUSTRE_USER
/* Add a #define for FASYNC if not available, as it forms part of
 * the Lustre O_LOV_DELAY_CREATE definition. */
#ifndef FASYNC
#define FASYNC          00020000   /* fcntl, for BSD compatibility */
#endif
        if (o->lustre_set_striping) {
                /* In the single-shared-file case, task 0 has to create the
                   file with the Lustre striping options before any other
                   processes open the file */
                if (!hints->filePerProc && rank != 0) {
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                        fd_oflag |= O_RDWR;
                        *fd = open64(testFileName, fd_oflag, mode);
                        if (*fd < 0)
                                ERRF("open64(\"%s\", %d, %#o) failed",
                                        testFileName, fd_oflag, mode);
                } else {
                        struct lov_user_md opts = { 0 };

                        /* Setup Lustre IOCTL striping pattern structure */
                        opts.lmm_magic = LOV_USER_MAGIC;
                        opts.lmm_stripe_size = o->lustre_stripe_size;
                        opts.lmm_stripe_offset = o->lustre_start_ost;
                        opts.lmm_stripe_count = o->lustre_stripe_count;

                        /* File needs to be opened O_EXCL because we cannot set
                         * Lustre striping information on a pre-existing file.*/

                        fd_oflag |= O_CREAT | O_EXCL | O_RDWR | O_LOV_DELAY_CREATE;
                        *fd = open64(testFileName, fd_oflag, mode);
                        if (*fd < 0) {
                                fprintf(stdout, "\nUnable to open '%s': %s\n",
                                        testFileName, strerror(errno));
                                MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1),
                                          "MPI_Abort() error");
                        } else if (ioctl(*fd, LL_IOC_LOV_SETSTRIPE, &opts)) {
                                char *errmsg = "stripe already set";
                                if (errno != EEXIST && errno != EALREADY)
                                        errmsg = strerror(errno);
                                fprintf(stdout,
                                        "\nError on ioctl for '%s' (%d): %s\n",
                                        testFileName, *fd, errmsg);
                                MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1),
                                          "MPI_Abort() error");
                        }
                        if (!hints->filePerProc)
                                MPI_CHECK(MPI_Barrier(testComm),
                                          "barrier error");
                }
        } else {
#endif                          /* HAVE_LUSTRE_USER */

                fd_oflag |= O_CREAT | O_RDWR;

#ifdef HAVE_BEEGFS_BEEGFS_H
                if (beegfs_isOptionSet(o->beegfs_chunkSize)
                    || beegfs_isOptionSet(o->beegfs_numTargets)) {
                    bool result = beegfs_createFilePath(testFileName,
                                                        mode,
                                                        o->beegfs_numTargets,
                                                        o->beegfs_chunkSize);
                    if (result) {
                       fd_oflag &= ~O_CREAT;
                    } else {
                       EWARN("BeeGFS tuning failed");
                    }
                 }
#endif /* HAVE_BEEGFS_BEEGFS_H */

                *fd = open64(testFileName, fd_oflag, mode);
                if (*fd < 0)
                        ERRF("open64(\"%s\", %d, %#o) failed",
                                testFileName, fd_oflag, mode);

#ifdef HAVE_LUSTRE_USER
        }

        if (o->lustre_ignore_locks) {
                int lustre_ioctl_flags = LL_FILE_IGNORE_LOCK;
                if (ioctl(*fd, LL_IOC_SETFLAGS, &lustre_ioctl_flags) == -1)
                        ERRF("ioctl(%d, LL_IOC_SETFLAGS, ...) failed", *fd);
        }
#endif                          /* HAVE_LUSTRE_USER */

#ifdef HAVE_GPFS_FCNTL_H
        /* in the single shared file case, immediately release all locks, with
         * the intent that we can avoid some byte range lock revocation:
         * everyone will be writing/reading from individual regions */
        if (o->gpfs_release_token ) {
                gpfs_free_all_locks(*fd);
        }
#endif
        return (aiori_fd_t*) fd;
}

/*
 * Create a file through mknod interface.
 */
int POSIX_Mknod(char *testFileName)
{
    int ret;

    ret = mknod(testFileName, S_IFREG | S_IRUSR, 0);
    if (ret < 0)
        ERR("mknod failed");

    return ret;
}

/*
 * Open a file through the POSIX interface.
 */
aiori_fd_t *POSIX_Open(char *testFileName, int flags, aiori_mod_opt_t * param)
{
        int fd_oflag = O_BINARY;
        int *fd;

        fd = (int *)malloc(sizeof(int));
        if (fd == NULL)
                ERR("Unable to malloc file descriptor");

        posix_options_t * o = (posix_options_t*) param;
        if (o->direct_io == TRUE)
                set_o_direct_flag(&fd_oflag);

        fd_oflag |= O_RDWR;

        if(hints->dryRun)
          return (aiori_fd_t*) 0;

        *fd = open64(testFileName, fd_oflag);
        if (*fd < 0)
                ERRF("open64(\"%s\", %d) failed: %s", testFileName, fd_oflag, strerror(errno));

#ifdef HAVE_LUSTRE_USER
        if (o->lustre_ignore_locks) {
                int lustre_ioctl_flags = LL_FILE_IGNORE_LOCK;
                if (verbose >= VERBOSE_1) {
                        fprintf(stdout,
                                "** Disabling lustre range locking **\n");
                }
                if (ioctl(*fd, LL_IOC_SETFLAGS, &lustre_ioctl_flags) == -1)
                        ERRF("ioctl(%d, LL_IOC_SETFLAGS, ...) failed", *fd);
        }
#endif                          /* HAVE_LUSTRE_USER */

#ifdef HAVE_GPFS_FCNTL_H
        if(o->gpfs_release_token) {
                gpfs_free_all_locks(*fd);
        }
#endif
        return (aiori_fd_t*) fd;
}

/*
 * Write or read access to file using the POSIX interface.
 */
static IOR_offset_t POSIX_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param)
{
        int xferRetries = 0;
        long long remaining = (long long)length;
        char *ptr = (char *)buffer;
        long long rc;
        int fd;
        posix_options_t * o = (posix_options_t*) param;

        if(hints->dryRun)
          return length;

        fd = *(int *)file;

#ifdef HAVE_GPFS_FCNTL_H
        if (o->gpfs_hint_access) {
          gpfs_access_start(fd, length, offset, access);
        }
#endif


        /* seek to offset */
        if (lseek64(fd, offset, SEEK_SET) == -1)
                ERRF("lseek64(%d, %lld, SEEK_SET) failed", fd, offset);

        while (remaining > 0) {
                /* write/read file */
                if (access == WRITE) {  /* WRITE */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout,
                                        "task %d writing to offset %lld\n",
                                        rank,
                                        offset + length - remaining);
                        }
                        rc = write(fd, ptr, remaining);
                        if (rc == -1)
                                ERRF("write(%d, %p, %lld) failed",
                                        fd, (void*)ptr, remaining);
                        if (hints->fsyncPerWrite == TRUE){
                          POSIX_Fsync((aiori_fd_t*) &fd, param);
                        }
                } else {        /* READ or CHECK */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout,
                                        "task %d reading from offset %lld\n",
                                        rank,
                                        offset + length - remaining);
                        }
                        rc = read(fd, ptr, remaining);
                        if (rc == 0)
                                ERRF("read(%d, %p, %lld) returned EOF prematurely",
                                        fd, (void*)ptr, remaining);
                        if (rc == -1)
                                ERRF("read(%d, %p, %lld) failed",
                                        fd, (void*)ptr, remaining);
                }
                if (rc < remaining) {
                        fprintf(stdout,
                                "WARNING: Task %d, partial %s, %lld of %lld bytes at offset %lld\n",
                                rank,
                                access == WRITE ? "write()" : "read()",
                                rc, remaining,
                                offset + length - remaining);
                        if (hints->singleXferAttempt == TRUE)
                                MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1),
                                          "barrier error");
                        if (xferRetries > MAX_RETRY)
                                ERR("too many retries -- aborting");
                }
                assert(rc >= 0);
                assert(rc <= remaining);
                remaining -= rc;
                ptr += rc;
                xferRetries++;
        }
#ifdef HAVE_GPFS_FCNTL_H
        if (o->gpfs_hint_access) {
            gpfs_access_end(fd, length, offset, access);
        }
#endif
        return (length);
}

void POSIX_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        if (fsync(*(int *)fd) != 0)
                EWARNF("fsync(%d) failed", *(int *)fd);
}


void POSIX_Sync(aiori_mod_opt_t * param)
{
  int ret = system("sync");
  if (ret != 0){
    FAIL("Error executing the sync command, ensure it exists.");
  }
}


/*
 * Close a file through the POSIX interface.
 */
void POSIX_Close(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        if(hints->dryRun)
          return;
        if (close(*(int *)fd) != 0)
                ERRF("close(%d) failed", *(int *)fd);
        free(fd);
}

/*
 * Delete a file through the POSIX interface.
 */
void POSIX_Delete(char *testFileName, aiori_mod_opt_t * param)
{
        if(hints->dryRun)
          return;
        if (unlink(testFileName) != 0){
                EWARNF("[RANK %03d]: unlink() of file \"%s\" failed", rank, testFileName);
        }
}

/*
 * Use POSIX stat() to return aggregate file size.
 */
IOR_offset_t POSIX_GetFileSize(aiori_mod_opt_t * test, char *testFileName)
{
        if(hints->dryRun)
          return 0;
        struct stat stat_buf;
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;

        if (stat(testFileName, &stat_buf) != 0) {
                ERRF("stat(\"%s\", ...) failed", testFileName);
        }
        aggFileSizeFromStat = stat_buf.st_size;

        return (aggFileSizeFromStat);
}
