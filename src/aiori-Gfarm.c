#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <gfarm/gfarm.h>
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#include "ior.h"
#include "aiori.h"

struct gfarm_file {
	GFS_File gf;
};

void
Gfarm_initialize()
{
	gfarm_initialize(NULL, NULL);
}

void
Gfarm_finalize()
{
	gfarm_terminate();
}

void *
Gfarm_create(char *fn, IOR_param_t *param)
{
	GFS_File gf;
	struct gfarm_file *fp;
	gfarm_error_t e;

	if (param->dryRun)
		return (NULL);

	e = gfs_pio_create(fn, GFARM_FILE_RDWR, 0664, &gf);
	if (e != GFARM_ERR_NO_ERROR)
		ERR("gfs_pio_create failed");
	GFARM_MALLOC(fp);
	if (fp == NULL)
		ERR("no memory");
	fp->gf = gf;
	return (fp);
}

void *
Gfarm_open(char *fn, IOR_param_t *param)
{
	GFS_File gf;
	struct gfarm_file *fp;
	gfarm_error_t e;

	if (param->dryRun)
		return (NULL);

	e = gfs_pio_open(fn, GFARM_FILE_RDWR, &gf);
	if (e != GFARM_ERR_NO_ERROR)
		ERR("gfs_pio_open failed");
	GFARM_MALLOC(fp);
	if (fp == NULL)
		ERR("no memory");
	fp->gf = gf;
	return (fp);
}

IOR_offset_t
Gfarm_xfer(int access, void *fd, IOR_size_t *buffer, IOR_offset_t len,
	IOR_param_t *param)
{
	struct gfarm_file *fp = fd;
	IOR_offset_t rem = len;
	gfarm_off_t off;
	gfarm_error_t e;
#define MAX_SZ	(1024 * 1024 * 1024)
	int sz, n;
	char *buf = (char *)buffer;

	if (param->dryRun)
		return (len);

	if (len > MAX_SZ)
		sz = MAX_SZ;
	else
		sz = len;

	e = gfs_pio_seek(fp->gf, param->offset, GFARM_SEEK_SET, &off);
	if (e != GFARM_ERR_NO_ERROR)
		ERR("gfs_pio_seek failed");
	while (rem > 0) {
		if (access == WRITE)
			e = gfs_pio_write(fp->gf, buf, sz, &n);
		else
			e = gfs_pio_read(fp->gf, buf, sz, &n);
		if (e != GFARM_ERR_NO_ERROR)
			ERR("xfer failed");
		if (n == 0)
			ERR("EOF encountered");
		rem -= n;
		buf += n;
	}
	return (len);
}

void
Gfarm_close(void *fd, IOR_param_t *param)
{
	struct gfarm_file *fp = fd;

	if (param->dryRun)
		return;

	if (gfs_pio_close(fp->gf) != GFARM_ERR_NO_ERROR)
		ERR("gfs_pio_close failed");
	free(fp);
}

void
Gfarm_delete(char *fn, IOR_param_t *param)
{
	gfarm_error_t e;

	if (param->dryRun)
		return;

	e = gfs_unlink(fn);
	if (e != GFARM_ERR_NO_ERROR)
		errno = gfarm_error_to_errno(e);
}

char *
Gfarm_version()
{
	return ((char *)gfarm_version());
}

void
Gfarm_fsync(void *fd, IOR_param_t *param)
{
	struct gfarm_file *fp = fd;

	if (param->dryRun)
		return;

	if (gfs_pio_sync(fp->gf) != GFARM_ERR_NO_ERROR)
		ERR("gfs_pio_sync failed");
}

IOR_offset_t
Gfarm_get_file_size(IOR_param_t *param, MPI_Comm comm, char *fn)
{
	struct gfs_stat st;
	IOR_offset_t size, sum, min, max;

	if (param->dryRun)
		return (0);

	if (gfs_stat(fn, &st) != GFARM_ERR_NO_ERROR)
		ERR("gfs_stat failed");
	size = st.st_size;
	gfs_stat_free(&st);

	if (param->filePerProc == TRUE) {
		MPI_CHECK(MPI_Allreduce(&size, &sum, 1, MPI_LONG_LONG_INT,
			MPI_SUM, comm), "cannot total data moved");
		size = sum;
	} else {
		MPI_CHECK(MPI_Allreduce(&size, &min, 1, MPI_LONG_LONG_INT,
			MPI_MIN, comm), "cannot total data moved");
		MPI_CHECK(MPI_Allreduce(&size, &max, 1, MPI_LONG_LONG_INT,
			MPI_MAX, comm), "cannot total data moved");
		if (min != max) {
			if (rank == 0)
				WARN("inconsistent file size by different "
				     "tasks");
			/* incorrect, but now consistent across tasks */
			size = min;
		}
	}
	return (size);
}

int
Gfarm_statfs(const char *fn, ior_aiori_statfs_t *st, IOR_param_t *param)
{
	gfarm_off_t used, avail, files;
	gfarm_error_t e;
	int bsize = 4096;

	if (param->dryRun)
		return (0);

	e = gfs_statfs_by_path(fn, &used, &avail, &files);
	if (e != GFARM_ERR_NO_ERROR) {
		errno = gfarm_error_to_errno(e);
		return (-1);
	}
	st->f_bsize = bsize;
	st->f_blocks = (used + avail) / bsize;
	st->f_bfree = avail / bsize;
	st->f_files = 2 * files; /* XXX */
	st->f_ffree = files; /* XXX */
	return (0);
}

int
Gfarm_mkdir(const char *fn, mode_t mode, IOR_param_t *param)
{
	gfarm_error_t e;

	if (param->dryRun)
		return (0);

	e = gfs_mkdir(fn, mode);
	if (e == GFARM_ERR_NO_ERROR)
		return (0);
	errno = gfarm_error_to_errno(e);
	return (-1);
}

int
Gfarm_rmdir(const char *fn, IOR_param_t *param)
{
	gfarm_error_t e;

	if (param->dryRun)
		return (0);

	e = gfs_rmdir(fn);
	if (e == GFARM_ERR_NO_ERROR)
		return (0);
	errno = gfarm_error_to_errno(e);
	return (-1);
}

int
Gfarm_access(const char *fn, int mode, IOR_param_t *param)
{
	struct gfs_stat st;
	gfarm_error_t e;

	if (param->dryRun)
		return (0);

	e = gfs_stat(fn, &st);
	if (e != GFARM_ERR_NO_ERROR) {
		errno = gfarm_error_to_errno(e);
		return (-1);
	}
	gfs_stat_free(&st);
	return (0);
}

/* XXX FIXME */
#define GFS_DEV		((dev_t)-1)
#define GFS_BLKSIZE	8192
#define STAT_BLKSIZ	512	/* for st_blocks */

int
Gfarm_stat(const char *fn, struct stat *buf, IOR_param_t *param)
{
	struct gfs_stat st;
	gfarm_error_t e;

	if (param->dryRun)
		return (0);

	e = gfs_stat(fn, &st);
	if (e != GFARM_ERR_NO_ERROR) {
		errno = gfarm_error_to_errno(e);
		return (-1);
	}
	buf->st_dev = GFS_DEV;
	buf->st_ino = st.st_ino;
	buf->st_mode = st.st_mode;
	buf->st_nlink = st.st_nlink;
	buf->st_uid = getuid();	/* XXX */
	buf->st_gid = getgid();	/* XXX */
	buf->st_size = st.st_size;
	buf->st_blksize = GFS_BLKSIZE;
	buf->st_blocks = (st.st_size + STAT_BLKSIZ - 1) / STAT_BLKSIZ;
	buf->st_atime = st.st_atimespec.tv_sec;
	buf->st_mtime = st.st_mtimespec.tv_sec;
	buf->st_ctime = st.st_ctimespec.tv_sec;
#if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	buf->st_atim.tv_nsec = st.st_atimespec.tv_nsec;
	buf->st_mtim.tv_nsec = st.st_mtimespec.tv_nsec;
	buf->st_ctim.tv_nsec = st.st_ctimespec.tv_nsec;
#endif
	gfs_stat_free(&st);
	return (0);
}

ior_aiori_t gfarm_aiori = {
	.name = "Gfarm",
	.name_legacy = NULL,
	.create = Gfarm_create,
	.open = Gfarm_open,
	.xfer = Gfarm_xfer,
	.close = Gfarm_close,
	.delete = Gfarm_delete,
	.get_version = Gfarm_version,
	.fsync = Gfarm_fsync,
	.get_file_size = Gfarm_get_file_size,
	.statfs = Gfarm_statfs,
	.mkdir = Gfarm_mkdir,
	.rmdir = Gfarm_rmdir,
	.access = Gfarm_access,
	.stat = Gfarm_stat,
	.initialize = Gfarm_initialize,
	.finalize = Gfarm_finalize,
	.get_options = NULL,
	.enable_mdtest = true,
};
