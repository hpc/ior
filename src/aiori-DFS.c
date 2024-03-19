/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/*
 * Copyright (C) 2018-2020 Intel Corporation
 * See the file COPYRIGHT for a complete copyright notice and license.
 */

/*
 * This file implements the abstract I/O interface for DAOS FS API.
 */

#define _BSD_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include <mpi.h>
#include <gurt/common.h>
#include <gurt/hash.h>
#include <daos.h>
#include <daos_fs.h>

#include "aiori.h"
#include "utilities.h"
#include "iordef.h"

#if defined(DAOS_API_VERSION_MAJOR) && defined(DAOS_API_VERSION_MINOR)
#define CHECK_DAOS_API_VERSION(major, minor)                            \
        ((DAOS_API_VERSION_MAJOR > (major))                             \
         || (DAOS_API_VERSION_MAJOR == (major) && DAOS_API_VERSION_MINOR >= (minor)))
#else
#define CHECK_DAOS_API_VERSION(major, minor) 0
#endif

static dfs_t *dfs;
static daos_handle_t poh, coh;
static daos_oclass_id_t objectClass;
static daos_oclass_id_t dir_oclass;
static struct d_hash_table *aiori_dfs_hash = NULL;
static int dfs_init_count;

struct aiori_dir_hdl {
        d_list_t	entry;
        dfs_obj_t	*oh;
        char		name[PATH_MAX];
};

enum handleType {
        POOL_HANDLE,
        CONT_HANDLE,
	DFS_HANDLE
};
/**************************** P R O T O T Y P E S *****************************/
static void DFS_Init(aiori_mod_opt_t *);
static void DFS_Finalize(aiori_mod_opt_t *);
static aiori_fd_t *DFS_Create(char *, int, aiori_mod_opt_t *);
static aiori_fd_t *DFS_Open(char *, int, aiori_mod_opt_t *);
static IOR_offset_t DFS_Xfer(int, aiori_fd_t *, IOR_size_t *, IOR_offset_t,
                             IOR_offset_t, aiori_mod_opt_t *);
static void DFS_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void DFS_Delete(char *, aiori_mod_opt_t *);
static char* DFS_GetVersion();
static void DFS_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static void DFS_Sync(aiori_mod_opt_t *);
static IOR_offset_t DFS_GetFileSize(aiori_mod_opt_t *, char *);
static int DFS_Statfs (const char *, ior_aiori_statfs_t *, aiori_mod_opt_t *);
static int DFS_Stat (const char *, struct stat *, aiori_mod_opt_t *);
static int DFS_Mkdir (const char *, mode_t, aiori_mod_opt_t *);
static int DFS_Rename(const char *, const char *, aiori_mod_opt_t *);
static int DFS_Rmdir (const char *, aiori_mod_opt_t *);
static int DFS_Access (const char *, int, aiori_mod_opt_t *);
static option_help * DFS_options(aiori_mod_opt_t **, aiori_mod_opt_t *);
static void DFS_init_xfer_options(aiori_xfer_hint_t *);
static int DFS_check_params(aiori_mod_opt_t *);

/************************** O P T I O N S *****************************/
typedef struct {
        char	*pool;
        char	*group;
        char	*cont;
	int	chunk_size;
	char	*oclass;
	char	*dir_oclass;
	char	*prefix;
        int	destroy;
} DFS_options_t;

static option_help * DFS_options(aiori_mod_opt_t ** init_backend_options,
                                 aiori_mod_opt_t * init_values){
        DFS_options_t * o = malloc(sizeof(DFS_options_t));

        if (init_values != NULL) {
                memcpy(o, init_values, sizeof(DFS_options_t));
        } else {
                memset(o, 0, sizeof(DFS_options_t));
        }

        *init_backend_options = (aiori_mod_opt_t *) o;

        option_help h [] = {
                {0, "dfs.pool", "Pool label", OPTION_OPTIONAL_ARGUMENT, 's', &o->pool},
                {0, "dfs.group", "DAOS system name", OPTION_OPTIONAL_ARGUMENT, 's', &o->group},
                {0, "dfs.cont", "Container label", OPTION_OPTIONAL_ARGUMENT, 's', &o->cont},
                {0, "dfs.chunk_size", "File chunk size in bytes (e.g.: 8, 4k, 2m, 1g)", OPTION_OPTIONAL_ARGUMENT, 'd', &o->chunk_size},
                {0, "dfs.oclass", "File object class", OPTION_OPTIONAL_ARGUMENT, 's', &o->oclass},
                {0, "dfs.dir_oclass", "Directory object class", OPTION_OPTIONAL_ARGUMENT, 's',
                 &o->dir_oclass},
                {0, "dfs.prefix", "Mount prefix", OPTION_OPTIONAL_ARGUMENT, 's', &o->prefix},
                {0, "dfs.destroy", "Destroy DFS container on finalize", OPTION_FLAG, 'd', &o->destroy},
                LAST_OPTION
        };

        option_help * help = malloc(sizeof(h));
        memcpy(help, h, sizeof(h));
        return help;
}


/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t dfs_aiori = {
        .name		= "DFS",
        .initialize	= DFS_Init,
        .finalize	= DFS_Finalize,
        .create		= DFS_Create,
        .open		= DFS_Open,
        .xfer		= DFS_Xfer,
        .close		= DFS_Close,
        .remove		= DFS_Delete,
        .get_version	= DFS_GetVersion,
        .fsync		= DFS_Fsync,
        .sync		= DFS_Sync,
        .get_file_size	= DFS_GetFileSize,
        .xfer_hints	= DFS_init_xfer_options,
        .statfs		= DFS_Statfs,
        .mkdir		= DFS_Mkdir,
        .rename		= DFS_Rename,
        .rmdir		= DFS_Rmdir,
        .access		= DFS_Access,
        .stat		= DFS_Stat,
        .get_options	= DFS_options,
        .check_params	= DFS_check_params,
        .enable_mdtest	= true,
};

/***************************** F U N C T I O N S ******************************/

/* For DAOS methods. */
#define DCHECK(rc, format, ...)                                         \
do {                                                                    \
        int _rc = (rc);                                                 \
                                                                        \
        if (_rc != 0) {                                                 \
                fprintf(stderr, "ERROR (%s:%d): %d: %d: "               \
                        format"\n", __FILE__, __LINE__, rank, _rc,      \
                        ##__VA_ARGS__);                                 \
                fflush(stderr);                                         \
                goto out;                                               \
        }                                                               \
} while (0)

#define DINFO(level, format, ...)					\
do {                                                                    \
        if (verbose >= level)						\
                printf("[%d] "format"\n", rank, ##__VA_ARGS__);         \
} while (0)

#define DERR(format, ...)                                               \
do {                                                                    \
        fprintf(stderr, format"\n", ##__VA_ARGS__);                     \
        fflush(stderr);                                                 \
        rc = -1;                                                        \
        goto out;                                                       \
} while (0)

static aiori_xfer_hint_t * hints = NULL;

void DFS_init_xfer_options(aiori_xfer_hint_t * params)
{
        hints = params;
}

static int DFS_check_params(aiori_mod_opt_t * options){
        DFS_options_t *o = (DFS_options_t *) options;

        if (o->pool == NULL || o->cont == NULL)
                ERR("Invalid pool or container options\n");

        if (testComm == MPI_COMM_NULL)
                testComm = MPI_COMM_WORLD;

        return 0;
}

static inline struct aiori_dir_hdl *
hdl_obj(d_list_t *rlink)
{
        return container_of(rlink, struct aiori_dir_hdl, entry);
}

static bool
key_cmp(struct d_hash_table *htable, d_list_t *rlink,
	const void *key, unsigned int ksize)
{
        struct aiori_dir_hdl *hdl = hdl_obj(rlink);

        return (strcmp(hdl->name, (const char *)key) == 0);
}

static void
rec_free(struct d_hash_table *htable, d_list_t *rlink)
{
        struct aiori_dir_hdl *hdl = hdl_obj(rlink);

        dfs_release(hdl->oh);
        free(hdl);
}

static bool
rec_decref(struct d_hash_table *htable, d_list_t *rlink)
{
        return true;
}

static uint32_t
rec_hash(struct d_hash_table *htable, d_list_t *rlink)
{
	struct aiori_dir_hdl *hdl = hdl_obj(rlink);

        return d_hash_string_u32(hdl->name, strlen(hdl->name));
}

static d_hash_table_ops_t hdl_hash_ops = {
        .hop_key_cmp	= key_cmp,
	.hop_rec_decref	= rec_decref,
	.hop_rec_free	= rec_free,
	.hop_rec_hash	= rec_hash
};

/* Distribute process 0's pool or container handle to others. */
static int
HandleDistribute(enum handleType type)
{
        d_iov_t global;
        int        rc;

        global.iov_buf = NULL;
        global.iov_buf_len = 0;
        global.iov_len = 0;

        assert(type == POOL_HANDLE || type == CONT_HANDLE || type == DFS_HANDLE);
        if (rank == 0) {
                /* Get the global handle size. */
                if (type == POOL_HANDLE)
                        rc = daos_pool_local2global(poh, &global);
                else if (type == CONT_HANDLE)
                        rc = daos_cont_local2global(coh, &global);
                else
                        rc = dfs_local2global(dfs, &global);
                DCHECK(rc, "Failed to get global handle size");
        }

        MPI_CHECK(MPI_Bcast(&global.iov_buf_len, 1, MPI_UINT64_T, 0, testComm),
                  "Failed to bcast global handle buffer size");

	global.iov_len = global.iov_buf_len;
        global.iov_buf = malloc(global.iov_buf_len);
        if (global.iov_buf == NULL)
                ERR("Failed to allocate global handle buffer");

        if (rank == 0) {
                if (type == POOL_HANDLE)
                        rc = daos_pool_local2global(poh, &global);
                else if (type == CONT_HANDLE)
                        rc = daos_cont_local2global(coh, &global);
                else
                        rc = dfs_local2global(dfs, &global);
                DCHECK(rc, "Failed to create global handle");
        }

        MPI_CHECK(MPI_Bcast(global.iov_buf, global.iov_buf_len, MPI_BYTE, 0, testComm),
                  "Failed to bcast global pool handle");

        if (rank != 0) {
                if (type == POOL_HANDLE)
                        rc = daos_pool_global2local(global, &poh);
                else if (type == CONT_HANDLE)
                        rc = daos_cont_global2local(poh, global, &coh);
                else
                        rc = dfs_global2local(poh, coh, 0, global, &dfs);
                DCHECK(rc, "Failed to get local handle");
        }

out:
        if (global.iov_buf)
                free(global.iov_buf);
        return rc;
}

static int
parse_filename(const char *path, char **_obj_name, char **_cont_name)
{
	char *f1 = NULL;
	char *f2 = NULL;
	char *fname = NULL;
	char *cont_name = NULL;
	int rc = 0;

	if (path == NULL || _obj_name == NULL || _cont_name == NULL)
		return -EINVAL;

	f1 = strdup(path);
	if (f1 == NULL) {
                rc = -ENOMEM;
                goto out;
        }

	f2 = strdup(path);
	if (f2 == NULL) {
                rc = -ENOMEM;
                goto out;
        }

	fname = basename(f1);
	cont_name = dirname(f2);

        if (cont_name[0] != '/') {
                char *ptr;
                char buf[PATH_MAX];

                ptr = realpath(cont_name, buf);
                if (ptr == NULL) {
                        rc = errno;
                        goto out;
                }

                cont_name = strdup(ptr);
                if (cont_name == NULL) {
                        rc = ENOMEM;
                        goto out;
                }
                *_cont_name = cont_name;
        } else {
                *_cont_name = strdup(cont_name);
                if (*_cont_name == NULL) {
                        rc = ENOMEM;
                        goto out;
                }
        }

        *_obj_name = strdup(fname);
        if (*_obj_name == NULL) {
                rc = ENOMEM;
                goto out;
        }

out:
	if (f1)
		free(f1);
	if (f2)
		free(f2);
	return rc;
}

static int
share_file_handle(dfs_obj_t **file, MPI_Comm comm)
{
        d_iov_t global;
        int        rc;

        global.iov_buf = NULL;
        global.iov_buf_len = 0;
        global.iov_len = 0;

        if (rank == 0) {
                rc = dfs_obj_local2global(dfs, *file, &global);
                DCHECK(rc, "Failed to get global handle size");
        }

        MPI_CHECK(MPI_Bcast(&global.iov_buf_len, 1, MPI_UINT64_T, 0, testComm),
                  "Failed to bcast global handle buffer size");

	global.iov_len = global.iov_buf_len;
        global.iov_buf = malloc(global.iov_buf_len);
        if (global.iov_buf == NULL)
                ERR("Failed to allocate global handle buffer");

        if (rank == 0) {
                rc = dfs_obj_local2global(dfs, *file, &global);
                DCHECK(rc, "Failed to create global handle");
        }

        MPI_CHECK(MPI_Bcast(global.iov_buf, global.iov_buf_len, MPI_BYTE, 0, testComm),
                  "Failed to bcast global pool handle");

        if (rank != 0) {
                rc = dfs_obj_global2local(dfs, 0, global, file);
                DCHECK(rc, "Failed to get local handle");
        }

out:
        if (global.iov_buf)
                free(global.iov_buf);
        return rc;
}

static dfs_obj_t *
lookup_insert_dir(const char *name, mode_t *mode)
{
        struct aiori_dir_hdl *hdl;
        dfs_obj_t *oh;
        d_list_t *rlink;
        size_t len = strlen(name);
        int rc;

        rlink = d_hash_rec_find(aiori_dfs_hash, name, len);
        if (rlink != NULL) {
                hdl = hdl_obj(rlink);
                return hdl->oh;
        }

        rc = dfs_lookup(dfs, name, O_RDWR, &oh, mode, NULL);
        if (rc)
                return NULL;

        if (mode && !S_ISDIR(*mode))
                return oh;

        hdl = calloc(1, sizeof(struct aiori_dir_hdl));
        if (hdl == NULL)
                return NULL;

        strncpy(hdl->name, name, len);
        hdl->oh = oh;

        rc = d_hash_rec_insert(aiori_dfs_hash, hdl->name, len, &hdl->entry, false);
        if (rc) {
                fprintf(stderr, "Failed to insert dir handle in hashtable\n");
                dfs_release(hdl->oh);
                free(hdl);
                return NULL;
        }

        return hdl->oh;
}

static void
DFS_Init(aiori_mod_opt_t * options)
{
        DFS_options_t *o = (DFS_options_t *)options;
        bool pool_connect, cont_create, cont_open, dfs_mounted;
	int rc;

        dfs_init_count++;
        if (dfs_init_count > 1) {
                pool_connect = cont_create = cont_open = dfs_mounted = true;
                /** chunk size and oclass can change between different runs */
                if (o->oclass) {
                        objectClass = daos_oclass_name2id(o->oclass);
                        if (objectClass == OC_UNKNOWN)
                                DERR("Invalid DAOS object class: %s\n", o->oclass);
                }
                if (o->dir_oclass) {
                        dir_oclass = daos_oclass_name2id(o->dir_oclass);
                        if (dir_oclass == OC_UNKNOWN)
                                DERR("Invalid DAOS directory object class: %s\n", o->dir_oclass);
                }
                return;
        }

        /** shouldn't be fatal since it can be called with POSIX backend selection */
        if (o->pool == NULL || o->cont == NULL) {
                dfs_init_count--;
                return;
        }

        pool_connect = cont_create = cont_open = dfs_mounted = false;

	rc = daos_init();
        DCHECK(rc, "Failed to initialize daos");

        if (o->oclass) {
                objectClass = daos_oclass_name2id(o->oclass);
                if (objectClass == OC_UNKNOWN)
                        DERR("Invalid DAOS object class: %s\n", o->oclass);
        }

        if (o->dir_oclass) {
                dir_oclass = daos_oclass_name2id(o->dir_oclass);
                if (dir_oclass == OC_UNKNOWN)
                        DERR("Invalid DAOS directory object class: %s\n", o->dir_oclass);
        }

        rc = d_hash_table_create(D_HASH_FT_EPHEMERAL | D_HASH_FT_NOLOCK | D_HASH_FT_LRU,
                                 4, NULL, &hdl_hash_ops, &aiori_dfs_hash);
        DCHECK(rc, "Failed to initialize dir hashtable");

        if (rank == 0) {
                daos_pool_info_t pool_info;
                daos_cont_info_t co_info;

                DINFO(VERBOSE_1, "DFS Pool = %s", o->pool);
                DINFO(VERBOSE_1, "DFS Container = %s", o->cont);

                rc = daos_pool_connect(o->pool, o->group, DAOS_PC_RW, &poh, &pool_info, NULL);
                DCHECK(rc, "Failed to connect to pool %s", o->pool);
                pool_connect = true;

                rc = daos_cont_open(poh, o->cont, DAOS_COO_RW, &coh, &co_info, NULL);
                /* If NOEXIST we create it */
                if (rc == -DER_NONEXIST) {
                        DINFO(VERBOSE_1, "Creating DFS Container ...\n");
                        rc = dfs_cont_create_with_label(poh, o->cont, NULL, NULL, &coh, NULL);
                        if (rc)
                                DCHECK(rc, "Failed to create container");
                        cont_create = true;
                } else if (rc) {
                        DCHECK(rc, "Failed to open container %s", o->cont);
                }
                cont_open = true;

                rc = dfs_mount(poh, coh, O_RDWR, &dfs);
                DCHECK(rc, "Failed to mount DFS namespace");
                dfs_mounted = true;
        }

        HandleDistribute(POOL_HANDLE);
        pool_connect = true;
        HandleDistribute(CONT_HANDLE);
        cont_open = true;
        HandleDistribute(DFS_HANDLE);
        dfs_mounted = true;

        if (o->prefix) {
                rc = dfs_set_prefix(dfs, o->prefix);
                DCHECK(rc, "Failed to set DFS Prefix");
        }

out:
        if (rc) {
                if (dfs_mounted)
                        dfs_umount(dfs);
                if (cont_open)
                        daos_cont_close(coh, NULL);
                if (cont_create && rank == 0) {
                        daos_cont_destroy(poh, o->cont, 1, NULL);
                }
                if (pool_connect)
                        daos_pool_disconnect(poh, NULL);
                if (aiori_dfs_hash)
                        d_hash_table_destroy(aiori_dfs_hash, false);
                daos_fini();
                dfs_init_count--;
                ERR("Failed to initialize DAOS DFS driver");
        }
}

static void
DFS_Finalize(aiori_mod_opt_t *options)
{
        DFS_options_t *o = (DFS_options_t *)options;
        int rc;

	objectClass	= 0;
	dir_oclass	= 0;

        dfs_init_count --;
        if (dfs_init_count != 0)
                return;

	MPI_Barrier(testComm);

        while (1) {
                d_list_t *rlink = NULL;

                rlink = d_hash_rec_first(aiori_dfs_hash);
                if (rlink == NULL)
                        break;
                d_hash_rec_decref(aiori_dfs_hash, rlink);
        }

        rc = d_hash_table_destroy(aiori_dfs_hash, false);
        DCHECK(rc, "Failed to destroy DFS hash");
        MPI_Barrier(testComm);

	rc = dfs_umount(dfs);
        DCHECK(rc, "Failed to umount DFS namespace");
	MPI_Barrier(testComm);

	rc = daos_cont_close(coh, NULL);
        DCHECK(rc, "Failed to close container %s (%d)", o->cont, rc);
	MPI_Barrier(testComm);

	if (o->destroy) {
                if (rank == 0) {
                        DINFO(VERBOSE_1, "Destroying DFS Container: %s", o->cont);
                        daos_cont_destroy(poh, o->cont, 1, NULL);
                        DCHECK(rc, "Failed to destroy container %s", o->cont);
                }

                MPI_Bcast(&rc, 1, MPI_INT, 0, testComm);
                if (rc) {
			if (rank == 0)
				DCHECK(rc, "Failed to destroy container %s (%d)", o->cont, rc);
                }
        }

        if (rank == 0)
                DINFO(VERBOSE_1, "Disconnecting from DAOS POOL");

        rc = daos_pool_disconnect(poh, NULL);
        DCHECK(rc, "Failed to disconnect from pool");

	MPI_CHECK(MPI_Barrier(testComm), "barrier error");

        if (rank == 0)
                DINFO(VERBOSE_1, "Finalizing DAOS..");

	rc = daos_fini();
        DCHECK(rc, "Failed to finalize DAOS");

out:
        /** reset tunables */
	o->pool		= NULL;
	o->group	= NULL;
	o->cont		= NULL;
	o->chunk_size	= 0;
	o->oclass	= NULL;
	o->dir_oclass	= NULL;
	o->prefix	= NULL;
	o->destroy	= 0;
}

/*
 * Create and open a file through the DFS interface.
 */
static aiori_fd_t *
DFS_Create(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        DFS_options_t *o = (DFS_options_t*) param;
	char *name = NULL, *dir_name = NULL;
	dfs_obj_t *obj = NULL, *parent = NULL;
	mode_t mode = 0664;
        int fd_oflag = 0;
	int rc;

	rc = parse_filename(testFileName, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", testFileName);
	assert(dir_name);
	assert(name);

        mode = S_IFREG | mode;
	if (hints->filePerProc || rank == 0) {
                fd_oflag |= O_CREAT | O_RDWR;

                parent = lookup_insert_dir(dir_name, NULL);
                if (parent == NULL)
                        DERR("Failed to lookup parent: %s", dir_name);

                if (flags & IOR_EXCL)
                        fd_oflag |= O_EXCL;

                rc = dfs_open(dfs, parent, name, mode, fd_oflag,
                              objectClass, o->chunk_size, NULL, &obj);
                DCHECK(rc, "dfs_open() of %s Failed", name);
        }

        if (!hints->filePerProc) {
                rc = share_file_handle(&obj, testComm);
                DCHECK(rc, "global open of %s Failed", name);
        }

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
        return (aiori_fd_t *)(obj);
}

/*
 * Open a file through the DFS interface.
 */
static aiori_fd_t *
DFS_Open(char *testFileName, int flags, aiori_mod_opt_t *param)
{
        DFS_options_t *o = (DFS_options_t*) param;
	char *name = NULL, *dir_name = NULL;
	dfs_obj_t *obj = NULL, *parent = NULL;
	mode_t mode = 0664;
        int fd_oflag = 0;
	int rc;

        fd_oflag |= O_RDWR;
	mode = S_IFREG | flags;

	rc = parse_filename(testFileName, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", testFileName);
	assert(dir_name);
	assert(name);

	if (hints->filePerProc || rank == 0) {
                parent = lookup_insert_dir(dir_name, NULL);
                if (parent == NULL)
                        DERR("Failed to lookup parent: %s", dir_name);

                rc = dfs_open(dfs, parent, name, mode, fd_oflag, objectClass,
                              o->chunk_size, NULL, &obj);
                DCHECK(rc, "dfs_open() of %s Failed", name);
        }

        if (!hints->filePerProc) {
                rc = share_file_handle(&obj, testComm);
                DCHECK(rc, "global open of %s Failed", name);
        }

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);

        return (aiori_fd_t *)(obj);
}

/*
 * Write or read access to file using the DFS interface.
 */
static IOR_offset_t
DFS_Xfer(int access, aiori_fd_t *file, IOR_size_t *buffer, IOR_offset_t length,
         IOR_offset_t off, aiori_mod_opt_t *param)
{
        int xferRetries = 0;
        long long remaining = (long long)length;
        char *ptr = (char *)buffer;
        daos_size_t ret;
        int rc;
	dfs_obj_t *obj;

        obj = (dfs_obj_t *)file;

        while (remaining > 0) {
                d_iov_t iov;
                d_sg_list_t sgl;

                /** set memory location */
                sgl.sg_nr = 1;
                sgl.sg_nr_out = 0;
                d_iov_set(&iov, (void *)ptr, remaining);
                sgl.sg_iovs = &iov;

                /* write/read file */
                if (access == WRITE) {
                        rc = dfs_write(dfs, obj, &sgl, off, NULL);
                        if (rc)
                                ERRF("dfs_write(%p, %lld) failed (%d): %s\n",
                                     (void*)ptr, remaining, rc, strerror(rc));
                        ret = remaining;
                } else {
                        rc = dfs_read(dfs, obj, &sgl, off, &ret, NULL);
                        if (rc)
                                ERRF("dfs_read(%p, %lld) failed (%d): %s\n",
                                     (void*)ptr, remaining, rc, strerror(rc));
                        if (ret == 0)
                                ERRF("dfs_read(%p, %lld) returned EOF prematurely",
                                     (void*)ptr, remaining);
                }

                if (ret < remaining) {
                        if (hints->singleXferAttempt == TRUE)
                                exit(EXIT_FAILURE);
                        if (xferRetries > MAX_RETRY)
                                ERR("too many retries -- aborting");
                }

                assert(ret >= 0);
                assert(ret <= remaining);
                remaining -= ret;
                ptr += ret;
                xferRetries++;
        }

        return (length);
}

/*
 * Perform fsync().
 */
static void
DFS_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        /* no cache in DFS, so this is a no-op currently */
	dfs_sync(dfs);
        return;
}

/*
 * Perform sync() on the dfs mount.
 */
static void
DFS_Sync(aiori_mod_opt_t * param)
{
        /* no cache in DFS, so this is a no-op currently */
	dfs_sync(dfs);
        return;
}

/*
 * Close a file through the DFS interface.
 */
static void
DFS_Close(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        dfs_release((dfs_obj_t *)fd);
}

/*
 * Delete a file through the DFS interface.
 */
static void
DFS_Delete(char *testFileName, aiori_mod_opt_t * param)
{
	char *name = NULL, *dir_name = NULL;
	dfs_obj_t *parent = NULL;
	int rc;

	rc = parse_filename(testFileName, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", testFileName);

	assert(dir_name);
	assert(name);

        parent = lookup_insert_dir(dir_name, NULL);
        if (parent == NULL)
                DERR("Failed to lookup parent: %s", dir_name);

	rc = dfs_remove(dfs, parent, name, false, NULL);
        DCHECK(rc, "Failed to remove path %s", testFileName);
out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
}

static char* DFS_GetVersion()
{
	static char ver[1024] = {};

	sprintf(ver, "%s", "DAOS");
	return ver;
}

/*
 * Use DFS stat() to return aggregate file size.
 */
static IOR_offset_t
DFS_GetFileSize(aiori_mod_opt_t * test, char *testFileName)
{
        dfs_obj_t *obj;
        MPI_Comm comm;
        daos_size_t fsize;
        int rc;

        if (hints->filePerProc == TRUE) {
                comm = MPI_COMM_SELF;
        } else {
                comm = testComm;
        }

	if (hints->filePerProc || rank == 0) {
                rc = dfs_lookup(dfs, testFileName, O_RDONLY, &obj, NULL, NULL);
                if (rc) {
                        fprintf(stderr, "dfs_lookup() of %s Failed (%d)", testFileName, rc);
                        return -1;
                }

                rc = dfs_get_size(dfs, obj, &fsize);
                dfs_release(obj);
                if (rc)
                        return -1;
        }

        if (!hints->filePerProc) {
                rc = MPI_Bcast(&fsize, 1, MPI_UINT64_T, 0, comm);
                if (rc)
                        return rc;
        }

        return (fsize);
}

static int
DFS_Statfs(const char *path, ior_aiori_statfs_t *sfs, aiori_mod_opt_t * param)
{
        daos_pool_info_t info = {.pi_bits = DPI_SPACE};
        int rc;

        rc = daos_pool_query(poh, NULL, &info, NULL, NULL);
        DCHECK(rc, "Failed to query pool");

        sfs->f_blocks = info.pi_space.ps_space.s_total[DAOS_MEDIA_SCM]
                + info.pi_space.ps_space.s_total[DAOS_MEDIA_NVME];
        sfs->f_bfree = info.pi_space.ps_space.s_free[DAOS_MEDIA_SCM]
                + info.pi_space.ps_space.s_free[DAOS_MEDIA_NVME];
        sfs->f_bsize = 1;
        sfs->f_files = -1;
        sfs->f_ffree = -1;
        sfs->f_bavail = sfs->f_bfree;

out:
        if (rc)
                rc = -1;
        return rc;
}

static int
DFS_Mkdir(const char *path, mode_t mode, aiori_mod_opt_t * param)
{
        dfs_obj_t *parent = NULL;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", path);

	assert(dir_name);
        if (!name)
                return 0;

        parent = lookup_insert_dir(dir_name, NULL);
        if (parent == NULL)
                DERR("Failed to lookup parent: %s", dir_name);

        rc = dfs_mkdir(dfs, parent, name, mode, dir_oclass);

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
        if (rc)
                rc = -1;
	return rc;
}

static int
DFS_Rename(const char *oldfile, const char *newfile, aiori_mod_opt_t * param)
{
        dfs_obj_t *old_parent = NULL, *new_parent = NULL;
	char *old_name = NULL, *old_dir_name = NULL;
	char *new_name = NULL, *new_dir_name = NULL;
	int rc;

	rc = parse_filename(oldfile, &old_name, &old_dir_name);
        DCHECK(rc, "Failed to parse path %s", oldfile);
	assert(old_dir_name);
        assert(old_name);

	rc = parse_filename(newfile, &new_name, &new_dir_name);
        DCHECK(rc, "Failed to parse path %s", newfile);
	assert(new_dir_name);
        assert(new_name);

        old_parent = lookup_insert_dir(old_dir_name, NULL);
        if (old_parent == NULL)
                DERR("Failed to lookup parent: %s", old_dir_name);

        new_parent = lookup_insert_dir(new_dir_name, NULL);
        if (new_parent == NULL)
                DERR("Failed to lookup parent: %s", new_dir_name);

        rc = dfs_move(dfs, old_parent, old_name, new_parent, new_name, NULL);

out:
	if (old_name)
		free(old_name);
	if (old_dir_name)
		free(old_dir_name);
	if (new_name)
		free(new_name);
	if (new_dir_name)
		free(new_dir_name);
        if (rc)
                return -1;
	return rc;
}

static int
DFS_Rmdir(const char *path, aiori_mod_opt_t * param)
{
        dfs_obj_t *parent = NULL;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", path);

	assert(dir_name);
        assert(name);

        parent = lookup_insert_dir(dir_name, NULL);
        if (parent == NULL)
                DERR("Failed to lookup parent: %s", dir_name);

	rc = dfs_remove(dfs, parent, name, false, NULL);

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
        if (rc)
                return -1;
	return rc;
}

static int
DFS_Access(const char *path, int mode, aiori_mod_opt_t * param)
{
        dfs_obj_t *parent = NULL;
        dfs_obj_t *obj = NULL;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", path);

	assert(dir_name);
        assert(name);

        parent = lookup_insert_dir(dir_name, NULL);
        if (parent == NULL)
                DERR("Failed to lookup parent: %s", dir_name);

        if (strcmp(name, "/") == 0) {
                free(name);
                name = NULL;
        }

	rc = dfs_access(dfs, parent, name, mode);

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
        if (rc)
                return -1;
	return rc;
}

static int
DFS_Stat(const char *path, struct stat *buf, aiori_mod_opt_t * param)
{
        dfs_obj_t *parent = NULL;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
        DCHECK(rc, "Failed to parse path %s", path);

	assert(dir_name);
        assert(name);

        parent = lookup_insert_dir(dir_name, NULL);
        if (parent == NULL)
                DERR("Failed to lookup parent: %s", dir_name);

	rc = dfs_stat(dfs, parent, name, buf);

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
        if (rc)
                return -1;
	return rc;
}
