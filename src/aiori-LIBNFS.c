/******************************************************************************\
*
*  Implement abstract I/O interface for libnfs (a user space nfs library).
*
\******************************************************************************/

#include "aiori.h"

char *LIBNFS_GetVersion(){
    return "Version 1.0";
}

aiori_fd_t *LIBNFS_Open(char *filePath, int iorflags, aiori_mod_opt_t *) {
    return NULL;
}

void LIBNFS_Close(aiori_fd_t * fileDescriptor, aiori_mod_opt_t * module_options) {

}

aiori_fd_t *LIBNFS_Create(char *filePath, int iorflags, aiori_mod_opt_t *)
{
    return NULL;
}

void LIBNFS_Remove(char* filePath, aiori_mod_opt_t * module_options) {
    
}

int LIBNFS_MakeDirectory(const char *path, mode_t mode, aiori_mod_opt_t * module_options) {
    return 0;
}

int LIBNFS_RemoveDirectory(const char *path, aiori_mod_opt_t * module_options) {
    return 0;
}

int LIBNFS_Stat(const char *path, struct stat *buf, aiori_mod_opt_t * module_options) {
    return 0;
}

int LIBNFS_StatFS(const char *filePath, ior_aiori_statfs_t *, aiori_mod_opt_t * module_options) {
    return 0;
}

void LIBNFS_Sync(aiori_mod_opt_t *moduleOptions) {

}

void LIBNFS_FSync(aiori_fd_t *fileDescriptor, aiori_mod_opt_t * module_options) {
  //TODO can we support fsync?
}

int LIBNFS_Access(const char *path, int mode, aiori_mod_opt_t *module_options) {
    return 0;
}

IOR_offset_t LIBNFS_GetFileSize(aiori_mod_opt_t *module_options, char *filename) {
    return (IOR_offset_t)0;
}

IOR_offset_t LIBNFS_Xfer(int access, aiori_fd_t *, IOR_size_t *, IOR_offset_t size, IOR_offset_t offset, aiori_mod_opt_t * module_options) {
    return (IOR_offset_t)0;
}

void LIBNFS_XferHints(aiori_xfer_hint_t * params) {

}

option_help *LIBNFS_GetOptions(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t* init_values) {

}

int LIBNFS_CheckParams(aiori_mod_opt_t *module_options) {
    return 0;
}

void LIBNFS_Initialize(aiori_mod_opt_t * options) {

}

void LIBNFS_Finalize(aiori_mod_opt_t * options) {

}

ior_aiori_t libnfs_aiori = {
                               .name = "LIBNFS",
                               .name_legacy = NULL,
                               .get_version = LIBNFS_GetVersion,
                               .open = LIBNFS_Open,
                               .close = LIBNFS_Close,
                               .create = LIBNFS_Create,
                               .remove = LIBNFS_Remove,
                               .mkdir = LIBNFS_MakeDirectory,
                               .rmdir = LIBNFS_RemoveDirectory,
                               .stat = LIBNFS_Stat,
                               .statfs = LIBNFS_StatFS,
                               .sync = LIBNFS_Sync,
                               .fsync = LIBNFS_FSync,
                               .access = LIBNFS_Access,
                               .get_file_size = LIBNFS_GetFileSize,
                               .xfer = LIBNFS_Xfer,
                               .xfer_hints = LIBNFS_XferHints,
                               .get_options = LIBNFS_GetOptions,
                               .check_params = LIBNFS_CheckParams,
                               .initialize = LIBNFS_Initialize,
                               .finalize = LIBNFS_Finalize
};
