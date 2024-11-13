/******************************************************************************\
*
*  Implement abstract I/O interface for libnfs (a user space nfs library).
*
\******************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <nfsc/libnfs.h>
#include "aiori.h"

typedef struct {
    char *url;
} libnfs_options_t;

static struct nfs_context *nfs_context;

static struct nfs_url *nfs_url;

/******************************************************************************\
*
*  Helper Functions
*
\******************************************************************************/

int Map_IOR_Open_Flags_To_LIBNFS_Flags(int ior_open_flags) {
    int libnfs_flags = 0;
    int test = ior_open_flags & IOR_RDONLY;
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
    //TODO IOR_EXCL and IOR_DIRECT are not supported?
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
        fprintf(stderr, "Error while opening the file %s\n", file_path);
        return NULL;
    }

    return (aiori_fd_t *)newFileFh;
}

void LIBNFS_Close(aiori_fd_t * file_descriptor, aiori_mod_opt_t * module_options) {
    struct nfsfh *file = (struct nfsfh *)file_descriptor;
    int close_result = nfs_close(nfs_context, file);
    if (close_result) {
        fprintf(stderr, "Error while closing the file\n");
    }
}

aiori_fd_t *LIBNFS_Create(char *file_path, int ior_flags, aiori_mod_opt_t *)
{
    struct nfsfh *newFileFh;
    int libnfs_flags = Map_IOR_Open_Flags_To_LIBNFS_Flags(ior_flags);
    int open_result = nfs_open2(nfs_context, file_path, libnfs_flags, 0777, &newFileFh);
    nfs_chmod(nfs_context, file_path, 0777); // mode parameter in nfs_open2 does not work, but with extra call nfs_chmod it works?
    if (open_result) {
        fprintf(stderr, "Error while creating the file %s\n", file_path);
        return NULL;
    }

    return (aiori_fd_t *)newFileFh;
}

void LIBNFS_Remove(char* file_path, aiori_mod_opt_t * module_options) {
    int unlink_result = nfs_unlink(nfs_context, file_path);
    if (unlink_result) {
        fprintf(stderr, "Error while unlinking the file %s\n", file_path);
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
    //set offset
    uint64_t current_offset = -1;    
    nfs_lseek(nfs_context, file, offset, SEEK_SET, &current_offset);
    if (current_offset != offset) {
        fprintf(stderr, "Error while lseek... expected offset: %lld but current offset: %" PRIu64 "\n", offset, current_offset);
        return 0;
    }

    if (access == WRITE) {
        //TODO multiple attempts?

        int write_result = nfs_write(nfs_context, file, (uint64_t)size, (void *)buffer);
        if (write_result < 0) {
            fprintf(stderr, "Error while writing to file\n");
            return 0;
        }

        return write_result;
    }

    if (access == READ) {
        int read_result = nfs_read(nfs_context, file, (uint64_t)size, (void*) buffer);
        if (read_result < 0) {
            fprintf(stderr, "Error while reading file\n");
            return 0;
        }

        return read_result;
    } 

    return (IOR_offset_t)0; 
}

void LIBNFS_XferHints(aiori_xfer_hint_t * params) {
    int i = 0;
    i++;
}

int LIBNFS_MakeDirectory(const char *path, mode_t mode, aiori_mod_opt_t * module_options) {
    return 0;
}

int LIBNFS_RemoveDirectory(const char *path, aiori_mod_opt_t * module_options) {
    return 0;
}

int LIBNFS_Stat(const char *path, struct stat *stat, aiori_mod_opt_t * module_options) {
    struct nfs_stat_64 nfs_stat;
    int stat_result = nfs_stat64(nfs_context, path, &nfs_stat);
    if (stat_result) {
        return stat_result;
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
    // stat->st_atimensec = nfs_stat.nfs_atime_nsec;
    // stat->st_mtimensec = nfs_stat.nfs_mtime_nsec;
    // stat->st_ctimensec = nfs_stat.nfs_ctime_nsec;
    return 0;
}

int LIBNFS_StatFS(const char *file_path, ior_aiori_statfs_t *stat, aiori_mod_opt_t * module_options) {
    struct nfs_statvfs_64 stat_fs;
    int stat_result = nfs_statvfs64(nfs_context, file_path, &stat_fs);
    if (stat_result) {
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

void LIBNFS_Sync(aiori_mod_opt_t *module_options) {
    int i = 0;
    i++;
}

void LIBNFS_FSync(aiori_fd_t *file_descriptor, aiori_mod_opt_t * module_options) {
  //TODO can we support fsync?
    int i = 0;
    i++;
}

int LIBNFS_Access(const char *path, int mode, aiori_mod_opt_t *module_options) {
    return 0;
}

IOR_offset_t LIBNFS_GetFileSize(aiori_mod_opt_t *module_options, char *filename) {
    return (IOR_offset_t)0;
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

int LIBNFS_CheckParams(aiori_mod_opt_t *module_options) {
    //TODO Validate the Serveradress??
    return 0;
}

void LIBNFS_Initialize(aiori_mod_opt_t * options) {
    if (nfs_context || nfs_url)
    {
        return;
    }

    libnfs_options_t *libnfs_options = (libnfs_options_t *)options;
    nfs_context = nfs_init_context();
    if (!nfs_context) {
        fprintf(stderr, "Error while creating nfs context\n");
        return;
    }

    nfs_url = nfs_parse_url_full(nfs_context, libnfs_options->url);
    if (!nfs_url) {
        fprintf(stderr, "Error while parsing libnfs.url\n");
        return;
    }

    int mount_result = nfs_mount(nfs_context, nfs_url->server, nfs_url->path);
    if (mount_result) {
        fprintf(stderr, "Error while mounting on nfs server: %s, path: %s\n", nfs_url->server, nfs_url->path);
        return;
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
                               .sync = LIBNFS_Sync,
                               .fsync = LIBNFS_FSync,
                               .access = LIBNFS_Access,
                               .get_file_size = LIBNFS_GetFileSize,
                               .get_options = LIBNFS_GetOptions,
                               .check_params = LIBNFS_CheckParams,
                               .initialize = LIBNFS_Initialize,
                               .finalize = LIBNFS_Finalize
};
