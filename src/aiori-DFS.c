/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
* Copyright (C) 2018 Intel Corporation
*
* GOVERNMENT LICENSE RIGHTS-OPEN SOURCE SOFTWARE
* The Government's rights to use, modify, reproduce, release, perform, display,
* or disclose this software are subject to the terms of the Apache License as
* provided in Contract No. 8F-30005.
* Any reproduction of computer software, computer software documentation, or
* portions thereof marked with this legend must also reproduce the markings.
********************************************************************************
*
* Implement of abstract I/O interface for DFS.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include <daos_types.h>
#include <daos_api.h>
#include <daos_fs.h>

#include "ior.h"
#include "aiori.h"
#include "iordef.h"
#include "utilities.h"

dfs_t *dfs;
daos_handle_t poh, coh;

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

	if (strcmp(path, "/") == 0) {
		*_cont_name = strdup("/");
		if (*_cont_name == NULL)
			return -ENOMEM;
		*_obj_name = NULL;
		return 0;
	}

	f1 = strdup(path);
	if (f1 == NULL)
		D_GOTO(out, rc = -ENOMEM);

	f2 = strdup(path);
	if (f2 == NULL)
		D_GOTO(out, rc = -ENOMEM);

	fname = basename(f1);
	cont_name = dirname(f2);

	if (cont_name[0] == '.' || cont_name[0] != '/') {
		char cwd[1024];

		if (getcwd(cwd, 1024) == NULL)
			D_GOTO(out, rc = -ENOMEM);

		if (strcmp(cont_name, ".") == 0) {
			cont_name = strdup(cwd);
			if (cont_name == NULL)
				D_GOTO(out, rc = -ENOMEM);
		} else {
			char *new_dir = calloc(strlen(cwd) + strlen(cont_name)
					       + 1, sizeof(char));
			if (new_dir == NULL)
				D_GOTO(out, rc = -ENOMEM);

			strcpy(new_dir, cwd);
			if (cont_name[0] == '.') {
				strcat(new_dir, &cont_name[1]);
			} else {
				strcat(new_dir, "/");
				strcat(new_dir, cont_name);
			}
			cont_name = new_dir;
		}
		*_cont_name = cont_name;
	} else {
		*_cont_name = strdup(cont_name);
		if (*_cont_name == NULL)
			D_GOTO(out, rc = -ENOMEM);
	}

	*_obj_name = strdup(fname);
	if (*_obj_name == NULL) {
		free(*_cont_name);
		*_cont_name = NULL;
		D_GOTO(out, rc = -ENOMEM);
	}

out:
	if (f1)
		free(f1);
	if (f2)
		free(f2);
	return rc;
}

/**************************** P R O T O T Y P E S *****************************/
static void *DFS_Create(char *, IOR_param_t *);
static void *DFS_Open(char *, IOR_param_t *);
static IOR_offset_t DFS_Xfer(int, void *, IOR_size_t *,
                             IOR_offset_t, IOR_param_t *);
static void DFS_Close(void *, IOR_param_t *);
static void DFS_Delete(char *, IOR_param_t *);
static void DFS_SetVersion(IOR_param_t *);
static void DFS_Fsync(void *, IOR_param_t *);
static IOR_offset_t DFS_GetFileSize(IOR_param_t *, MPI_Comm, char *);
static int DFS_Statfs (const char *, ior_aiori_statfs_t *, IOR_param_t *);
static int DFS_Stat (const char *, struct stat *, IOR_param_t *);
static int DFS_Mkdir (const char *, mode_t, IOR_param_t *);
static int DFS_Rmdir (const char *, IOR_param_t *);
static int DFS_Access (const char *, int, IOR_param_t *);
static int DFS_Init(IOR_param_t *param);
static int DFS_Finalize(IOR_param_t *param);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t dfs_aiori = {
        .name = "DFS",
        .create = DFS_Create,
        .open = DFS_Open,
        .xfer = DFS_Xfer,
        .close = DFS_Close,
        .delete = DFS_Delete,
        .set_version = DFS_SetVersion,
        .fsync = DFS_Fsync,
        .get_file_size = DFS_GetFileSize,
        .statfs = DFS_Statfs,
        .mkdir = DFS_Mkdir,
        .rmdir = DFS_Rmdir,
        .access = DFS_Access,
        .stat = DFS_Stat,
        .init = DFS_Init,
        .finalize = DFS_Finalize,
};

/***************************** F U N C T I O N S ******************************/

static int
DFS_Init(IOR_param_t *param) {
	uuid_t			pool_uuid, co_uuid;
	daos_pool_info_t	pool_info;
	daos_cont_info_t	co_info;
	d_rank_list_t		*svcl = NULL;
        bool			cont_created = false;
	int			rc;

	if (uuid_parse(param->daosPool, pool_uuid) < 0) {
		fprintf(stderr, "Invalid pool uuid\n");
                return -1;
	}

	if (uuid_parse(param->daosCont, co_uuid) < 0) {
		fprintf(stderr, "Invalid pool uuid\n");
                return -1;
	}

	svcl = daos_rank_list_parse(param->daosPoolSvc, ":");
	if (svcl == NULL) {
		fprintf(stderr, "Invalid pool service rank list\n");
                return -1;
	}

        printf("Pool uuid = %s, SVCL = %s\n", param->daosPool,
               param->daosPoolSvc);

        printf("DFS Container namespace uuid = %s\n", param->daosCont);

	rc = daos_init();
	if (rc) {
		fprintf(stderr, "daos_init() failed with %d\n", rc);
		return rc;
	}

	/** Connect to DAOS pool */
	rc = daos_pool_connect(pool_uuid,
                               strlen(param->daosGroup) ? param->daosGroup : NULL,
                               svcl, DAOS_PC_RW, &poh, &pool_info, NULL);
	if (rc < 0) {
		fprintf(stderr, "Failed to connect to pool (%d)\n", rc);
                goto err_daos;
	}

	rc = daos_cont_open(poh, co_uuid, DAOS_COO_RW, &coh, &co_info, NULL);
	/* If NOEXIST we create it */
	if (rc == -DER_NONEXIST) {
                printf("Creating DFS Container ...\n");
		rc = daos_cont_create(poh, co_uuid, NULL);
		if (rc == 0) {
			cont_created = true;
			rc = daos_cont_open(poh, co_uuid, DAOS_COO_RW, &coh,
					    &co_info, NULL);
		}
	}
	if (rc) {
		fprintf(stderr, "Failed to create container (%d)\n", rc);
                goto err_pool;
	}

	rc = dfs_mount(poh, coh, O_RDWR, &dfs);
	if (rc) {
		fprintf(stderr, "dfs_mount failed (%d)\n", rc);
                goto err_cont;
	}

out:
        daos_rank_list_free(svcl);
	return rc;
err_cont:
	daos_cont_close(coh, NULL);
err_pool:
	if (cont_created)
		daos_cont_destroy(poh, co_uuid, 1, NULL);
	daos_pool_disconnect(poh, NULL);
err_daos:
	daos_fini();
        goto out;
}

int
DFS_Finalize(IOR_param_t *param)
{
        int rc;

	rc = dfs_umount(dfs, true);
	if (rc) {
                fprintf(stderr, "dfs_umount() failed (%d)\n", rc);
                return -1;
        }

	rc = daos_cont_close(coh, NULL);
	if (rc) {
                fprintf(stderr, "daos_cont_close() failed (%d)\n", rc);
                return -1;
        }

        daos_pool_disconnect(poh, NULL);
	if (rc) {
                fprintf(stderr, "daos_pool_disconnect() failed (%d)\n", rc);
                return -1;
        }

	rc = daos_fini();
	if (rc) {
                fprintf(stderr, "daos_fini() failed (%d)\n", rc);
                return -1;
        }

        return 0;
}

/*
 * Creat and open a file through the DFS interface.
 */
static void *
DFS_Create(char *testFileName, IOR_param_t *param)
{
	char *name = NULL, *dir_name = NULL;
	dfs_obj_t *obj = NULL, *parent = NULL;
	mode_t pmode, mode;
        int fd_oflag = 0;
	int rc;

        assert(param);

        fd_oflag |= O_CREAT | O_RDWR;
	mode = S_IFREG | param->mode;

	rc = parse_filename(testFileName, &name, &dir_name);
	if (rc)
                goto out;

	assert(dir_name);
	assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDWR, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

	rc = dfs_open(dfs, parent, name, mode, fd_oflag, DAOS_OC_LARGE_RW,
                      NULL, &obj);
	if (rc)
                goto out;

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);

        return ((void *)obj);
}

/*
 * Open a file through the DFS interface.
 */
static void *
DFS_Open(char *testFileName, IOR_param_t *param)
{
	char *name = NULL, *dir_name = NULL;
	dfs_obj_t *obj = NULL, *parent = NULL;
	mode_t pmode;
	int rc;
        int fd_oflag = 0;

        fd_oflag |= O_RDWR;

	rc = parse_filename(testFileName, &name, &dir_name);
	if (rc)
                goto out;

	assert(dir_name);
	assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDWR, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

	rc = dfs_open(dfs, parent, name, S_IFREG, fd_oflag, 0, NULL, &obj);
	if (rc)
                goto out;

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);

        return ((void *)obj);
}

/*
 * Write or read access to file using the DFS interface.
 */
static IOR_offset_t
DFS_Xfer(int access, void *file, IOR_size_t *buffer, IOR_offset_t length,
         IOR_param_t *param)
{
        int xferRetries = 0;
        long long remaining = (long long)length;
        char *ptr = (char *)buffer;
        daos_size_t ret;
        int rc;
	dfs_obj_t *obj;

        obj = (dfs_obj_t *)file;

        while (remaining > 0) {
                daos_iov_t iov;
                daos_sg_list_t sgl;

                /** set memory location */
                sgl.sg_nr = 1;
                sgl.sg_nr_out = 0;
                daos_iov_set(&iov, (void *)ptr, remaining);
                sgl.sg_iovs = &iov;

                /* write/read file */
                if (access == WRITE) {
                        rc = dfs_write(dfs, obj, sgl, param->offset);
                        if (rc)
                                ERR("write() failed");
                        ret = remaining;
                } else {
                        rc = dfs_read(dfs, obj, sgl, param->offset, &ret);
                        if (rc || ret == 0)
                                ERR("read() failed");
                }

                if (ret < remaining) {
                        if (param->singleXferAttempt == TRUE)
                                MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1),
                                          "barrier error");
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
DFS_Fsync(void *fd, IOR_param_t * param)
{
	dfs_sync(dfs);
        return;
}

/*
 * Close a file through the DFS interface.
 */
static void
DFS_Close(void *fd, IOR_param_t * param)
{
        dfs_release((dfs_obj_t *)fd);
}

/*
 * Delete a file through the DFS interface.
 */
static void
DFS_Delete(char *testFileName, IOR_param_t * param)
{
	char *name = NULL, *dir_name = NULL;
	dfs_obj_t *parent = NULL;
	mode_t pmode;
	int rc;

	rc = parse_filename(testFileName, &name, &dir_name);
	if (rc)
                goto out;

	assert(dir_name);
	assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDWR, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

	rc = dfs_remove(dfs, parent, name, false);
	if (rc)
                goto out;

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);
}

/*
 * Determine api version.
 */
static void
DFS_SetVersion(IOR_param_t * test)
{
        strcpy(test->apiVersion, test->api);
}

/*
 * Use DFS stat() to return aggregate file size.
 */
static IOR_offset_t
DFS_GetFileSize(IOR_param_t * test, MPI_Comm testComm, char *testFileName)
{
        dfs_obj_t *obj;
        daos_size_t fsize, tmpMin, tmpMax, tmpSum;
        int rc;

	rc = dfs_lookup(dfs, testFileName, O_RDONLY, &obj, NULL);
	if (rc)
                return -1;

        rc = dfs_get_size(dfs, obj, &fsize);
        if (rc)
                return -1;

        dfs_release(obj);

        if (test->filePerProc == TRUE) {
                MPI_CHECK(MPI_Allreduce(&fsize, &tmpSum, 1,
                                        MPI_LONG_LONG_INT, MPI_SUM, testComm),
                          "cannot total data moved");
                fsize = tmpSum;
        } else {
                MPI_CHECK(MPI_Allreduce(&fsize, &tmpMin, 1,
                                        MPI_LONG_LONG_INT, MPI_MIN, testComm),
                          "cannot total data moved");
                MPI_CHECK(MPI_Allreduce(&fsize, &tmpMax, 1,
                                        MPI_LONG_LONG_INT, MPI_MAX, testComm),
                          "cannot total data moved");
                if (tmpMin != tmpMax) {
                        if (rank == 0) {
                                WARN("inconsistent file size by different tasks");
                        }
                        /* incorrect, but now consistent across tasks */
                        fsize = tmpMin;
                }
        }

        return (fsize);
}

static int
DFS_Statfs(const char *path, ior_aiori_statfs_t *sfs, IOR_param_t * param)
{
        return 0;
}

static int
DFS_Mkdir(const char *path, mode_t mode, IOR_param_t * param)
{
        dfs_obj_t *parent = NULL;
	mode_t pmode;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
	if (rc)
		return rc;

	assert(dir_name);
        assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDWR, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

	rc = dfs_mkdir(dfs, parent, name, mode);
	if (rc)
                goto out;

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);
        if (rc)
                return -1;
	return rc;
}

static int
DFS_Rmdir(const char *path, IOR_param_t * param)
{
        dfs_obj_t *parent = NULL;
	mode_t pmode;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
	if (rc)
		return rc;

	assert(dir_name);
        assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDWR, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

	rc = dfs_remove(dfs, parent, name, false);
	if (rc)
                goto out;

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);
        if (rc)
                return -1;
	return rc;
}

static int
DFS_Access(const char *path, int mode, IOR_param_t * param)
{
        dfs_obj_t *parent = NULL;
	mode_t pmode;
	char *name = NULL, *dir_name = NULL;
        struct stat stbuf;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
	if (rc)
		return rc;

	assert(dir_name);
        assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDWR, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

        if (strcmp(name, ".") == 0) {
                free(name);
                name = NULL;
        }
	rc = dfs_stat(dfs, parent, name, &stbuf);
	if (rc) {
                rc = -1;
                errno = -ENOENT;
                goto out;
        }

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);
        if (rc)
                return -1;
	return rc;
}

static int
DFS_Stat(const char *path, struct stat *buf, IOR_param_t * param)
{
        dfs_obj_t *parent = NULL;
	mode_t pmode;
	char *name = NULL, *dir_name = NULL;
	int rc;

	rc = parse_filename(path, &name, &dir_name);
	if (rc)
		return rc;

	assert(dir_name);
        assert(name);

	rc = dfs_lookup(dfs, dir_name, O_RDONLY, &parent, &pmode);
	if (rc || !S_ISDIR(pmode))
                goto out;

	rc = dfs_stat(dfs, parent, name, buf);
	if (rc)
                goto out;

out:
	if (name)
		free(name);
	if (dir_name)
		free(dir_name);
	if (parent)
		dfs_release(parent);
        if (rc)
                return -1;
	return rc;
}
