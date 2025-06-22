/******************************************************************************\
*
*  Implement abstract I/O interface for libnfs (a user space nfs library).
*
\******************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>
#include <nfsc/libnfs.h>
#include "aiori-LIBNFS.h"
#include "aiori.h"
#include "aiori-debug.h"

static struct nfs_context *nfs_context;

static struct nfs_url *nfs_url;

static struct aiori_xfer_hint_t *hint_parameter;

/******************************************************************************\
*
*  Helper Functions
*
\******************************************************************************/

int Map_IOR_Open_Flags_To_LIBNFS_Flags(int ior_open_flags) {
    int libnfs_flags = 0;    
    if (ior_open_flags & IOR_RDONLY) {
        libnfs_flags |= O_RDONLY;
    }

    if (ior_open_flags & IOR_WRONLY) {
        libnfs_flags |= O_WRONLY;
    }

    if (ior_open_flags & IOR_RDWR) {
        libnfs_flags |= O_RDWR;
    }

    if (ior_open_flags & IOR_APPEND) {
        libnfs_flags |= O_APPEND;
    }

    if (ior_open_flags & IOR_CREAT) {
        libnfs_flags |= O_CREAT;
    }

    if (ior_open_flags & IOR_TRUNC) {
        libnfs_flags |= O_TRUNC;
    }

    return libnfs_flags;
}

/******************************************************************************\
*
*  Implementation of the Backend-Interface.
*
\******************************************************************************/

char *LIBNFS_GetVersion(){
    return "Version 1.0";
}

aiori_fd_t *LIBNFS_Open(char *file_path, int ior_flags, aiori_mod_opt_t *) {
    struct nfsfh *newFileFh;
    int libnfs_flags = Map_IOR_Open_Flags_To_LIBNFS_Flags(ior_flags);
    int open_result = nfs_open(nfs_context, file_path, libnfs_flags, &newFileFh);
    if (open_result) {
        ERRF("Error while opening the file %s \n nfs error: %s\n", file_path, nfs_get_error(nfs_context));
    }

    return (aiori_fd_t *)newFileFh;
}

void LIBNFS_Close(aiori_fd_t * file_descriptor, aiori_mod_opt_t * module_options) {
    struct nfsfh *file = (struct nfsfh *)file_descriptor;
    int close_result = nfs_close(nfs_context, file);
    if (close_result) {
        ERRF("Error while closing a file \n nfs error: %s\n", nfs_get_error(nfs_context));        
    }
}

aiori_fd_t *LIBNFS_Create(char *file_path, int ior_flags, aiori_mod_opt_t *)
{
    struct nfsfh *newFileFh;
    int libnfs_flags = Map_IOR_Open_Flags_To_LIBNFS_Flags(ior_flags);
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int open_result = nfs_open2(nfs_context, file_path, libnfs_flags, mode, &newFileFh);
    if (open_result) {
        ERRF("Error while creating the file %s \n nfs error: %s\n", file_path, nfs_get_error(nfs_context));
    }

    return (aiori_fd_t *)newFileFh;
}

void LIBNFS_Remove(char* file_path, aiori_mod_opt_t * module_options) {
    int unlink_result = nfs_unlink(nfs_context, file_path);
    if (unlink_result) {
        ERRF("Error while unlinking the file %s \n nfs error: %s\n", file_path, nfs_get_error(nfs_context));
    }
}

IOR_offset_t LIBNFS_Xfer(
    int access,
    aiori_fd_t *file_descriptor,
    IOR_size_t *buffer,
    IOR_offset_t size,
    IOR_offset_t offset,
    aiori_mod_opt_t * module_options) {
    
    struct nfsfh *file = (struct nfsfh *)file_descriptor;
    uint64_t current_offset;    
    nfs_lseek(nfs_context, file, offset, SEEK_SET, &current_offset);
    if (current_offset != offset) {
        ERRF("Error while calling lseek... expected offset: %lld but current offset: %" PRIu64 "\n nfs error: %s\n", offset, current_offset, nfs_get_error(nfs_context));
    }

    if (access == WRITE) {
        int write_result = nfs_write(nfs_context, file, (void *)buffer, (uint64_t)size);
        if (write_result < 0) {
            ERRF("Error while writing to file \n nfs error: %s\n", nfs_get_error(nfs_context));
        }

        return write_result;
    }

    if (access == READ) {
        int read_result = nfs_read(nfs_context, file, (void*)buffer, (size_t)size);
        if (read_result < 0) {
            ERRF("Error while reading to file \n nfs error: %s\n", nfs_get_error(nfs_context));
        }

        return read_result;
    } 

    return (IOR_offset_t)0; 
}

void LIBNFS_XferHints(aiori_xfer_hint_t * params) {
    hint_parameter = params;
}

int LIBNFS_MakeDirectory(const char *path, mode_t mode, aiori_mod_opt_t * module_options) {
    if (nfs_mkdir2(nfs_context, path, mode)) {
        ERRF("Error while creating directory \n nfs error: %s\n", nfs_get_error(nfs_context));
    }

    return 0;
}

int LIBNFS_RemoveDirectory(const char *path, aiori_mod_opt_t * module_options) {
    if (nfs_rmdir(nfs_context, path)) {
        ERRF("Error while removing directory \n nfs error: %s\n", nfs_get_error(nfs_context));
    }

    return 0;
}

int LIBNFS_Stat(const char *path, struct stat *stat, aiori_mod_opt_t * module_options) {
    struct nfs_stat_64 nfs_stat;
    int stat_result = nfs_stat64(nfs_context, path, &nfs_stat);
    if (stat_result) {
        if (-stat_result == ENOENT) {
            return ENOENT;
        }

        ERRF("Error while calling stat on path %s \n nfs error: %s\n", path, nfs_get_error(nfs_context));
    }

    stat->st_blksize = nfs_stat.nfs_blksize;
    stat->st_blocks = nfs_stat.nfs_blocks;
    stat->st_size = nfs_stat.nfs_size;
    stat->st_gid = nfs_stat.nfs_gid;
    stat->st_uid = nfs_stat.nfs_uid;
    stat->st_dev = nfs_stat.nfs_dev;
    stat->st_rdev = nfs_stat.nfs_rdev;
    stat->st_ino = nfs_stat.nfs_ino;
    stat->st_mode = nfs_stat.nfs_mode;
    stat->st_nlink = nfs_stat.nfs_nlink;
    stat->st_atime = nfs_stat.nfs_atime;
    stat->st_mtime = nfs_stat.nfs_mtime;
    stat->st_ctime = nfs_stat.nfs_ctime;
    return 0;
}

int LIBNFS_StatFS(const char *path, ior_aiori_statfs_t *stat, aiori_mod_opt_t * module_options) {
    struct nfs_statvfs_64 stat_fs;
    int stat_result = nfs_statvfs64(nfs_context, path, &stat_fs);
    if (stat_result) {
        ERRF("Error while calling statfs on path %s \n nfs error: %s\n", path, nfs_get_error(nfs_context));
        return stat_result;
    }

    stat->f_bavail = stat_fs.f_bavail;
    stat->f_bfree = stat_fs.f_bfree;
    stat->f_blocks = stat_fs.f_blocks;
    stat->f_bsize = stat_fs.f_bsize;
    stat->f_ffree = stat_fs.f_ffree;
    stat->f_files = stat_fs.f_files;

    return 0;
}

void LIBNFS_FSync(aiori_fd_t *file_descriptor, aiori_mod_opt_t * module_options) {
    struct nfsfh *file = (struct nfsfh *)file_descriptor; 
    if (nfs_fsync(nfs_context, file)) {
        ERRF("Error while calling fsync \n nfs error: %s\n", nfs_get_error(nfs_context));
    }
}

int LIBNFS_Access(const char *path, int mode, aiori_mod_opt_t *module_options) {
    return nfs_access(nfs_context, path, mode);    
}

IOR_offset_t LIBNFS_GetFileSize(aiori_mod_opt_t *module_options, char *path) {
    struct nfs_stat_64 nfs_stat;    
    int stat_result = nfs_stat64(nfs_context, path, &nfs_stat);
    if (stat_result) {
        ERRF("Error while calling stat on path %s (to evaluate the file size) \n nfs error: %s\n", path, nfs_get_error(nfs_context));
    }

    return (IOR_offset_t)nfs_stat.nfs_size;
}

option_help *LIBNFS_GetOptions(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t* init_values) {
    libnfs_options_t *libnfs_options = malloc(sizeof(libnfs_options_t));
    if (init_values != NULL) {
        memcpy(libnfs_options, init_values, sizeof(libnfs_options_t));
    } else {
        memset(libnfs_options, 0, sizeof(libnfs_options_t));
    }

    *init_backend_options = (aiori_mod_opt_t *) libnfs_options;

    option_help h [] = {
        {0, "libnfs.url", "The URL (RFC2224) specifing the server, path and options", OPTION_REQUIRED_ARGUMENT, 's', &libnfs_options->url},
        LAST_OPTION
    };

    option_help * help = malloc(sizeof(h));
    memcpy(help, h, sizeof(h));
    return help;
}

void LIBNFS_Initialize(aiori_mod_opt_t * options) {
    if (nfs_context || nfs_url)
    {
        return;
    }

    libnfs_options_t *libnfs_options = (libnfs_options_t *)options;
    nfs_context = nfs_init_context();
    if (!nfs_context) {
        ERRF("Error while creating the nfs context \n nfs error: %s\n", nfs_get_error(nfs_context));
    }

    nfs_url = nfs_parse_url_full(nfs_context, libnfs_options->url);
    if (!nfs_url) {
        ERRF("Error while parsing the argument libnfs.url \n nfs error: %s\n", nfs_get_error(nfs_context));
    }

    int mount_result = nfs_mount(nfs_context, nfs_url->server, nfs_url->path);
    if (mount_result) {
        ERRF("Error while mounting nfs server: %s, path: %s \n nfs error: %s\n", nfs_url->server, nfs_url->path, nfs_get_error(nfs_context));
    }
}

void LIBNFS_Finalize(aiori_mod_opt_t * options) {
    if (nfs_context) {
        nfs_destroy_context(nfs_context);
        nfs_context = NULL;
    }

    if (nfs_url) {
        nfs_destroy_url(nfs_url);
        nfs_url = NULL;
    }
}

ior_aiori_t libnfs_aiori = {
                               .name = "LIBNFS",
                               .name_legacy = NULL,
                               .get_version = LIBNFS_GetVersion,
                               .open = LIBNFS_Open,
                               .close = LIBNFS_Close,
                               .create = LIBNFS_Create,
                               .remove = LIBNFS_Remove,
                               .xfer = LIBNFS_Xfer,
                               .xfer_hints = LIBNFS_XferHints,
                               .mkdir = LIBNFS_MakeDirectory,
                               .rmdir = LIBNFS_RemoveDirectory,
                               .stat = LIBNFS_Stat,
                               .statfs = LIBNFS_StatFS,
                               .sync = NULL,
                               .fsync = LIBNFS_FSync,
                               .access = LIBNFS_Access,
                               .get_file_size = LIBNFS_GetFileSize,
                               .get_options = LIBNFS_GetOptions,
                               .check_params = NULL,
                               .initialize = LIBNFS_Initialize,
                               .finalize = LIBNFS_Finalize,
                               .enable_mdtest = true,
};
