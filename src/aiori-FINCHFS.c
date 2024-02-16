#include <sys/types.h>
#include <sys/stat.h>
#include <finchfs.h>
#include "ior.h"
#include "aiori.h"

static aiori_xfer_hint_t *hints = NULL;

struct FINCHFS_File {
	int fd;
};

struct finchfs_option {
	size_t chunk_size;
};

option_help *
FINCHFS_options(aiori_mod_opt_t **init_backend_options,
	aiori_mod_opt_t *init_values)
{
	struct finchfs_option *o = malloc(sizeof(*o));

	if (init_values != NULL)
		memcpy(o, init_values, sizeof(*o));
	else
		memset(o, 0, sizeof(*o));

	if (o->chunk_size > 0)
		finchfs_set_chunk_size(o->chunk_size);

	*init_backend_options = (aiori_mod_opt_t *)o;

	option_help h[] = {
	    {0, "finchfs.chunk_size", "chunk size", OPTION_FLAG, 'd',
		    &o->chunk_size},
	    LAST_OPTION
	};
	option_help *help = malloc(sizeof(h));
	memcpy(help, h, sizeof(h));

	return (help);
}

void
FINCHFS_xfer_hints(aiori_xfer_hint_t *params)
{
	hints = params;
}

void
FINCHFS_initialize()
{
	finchfs_init(NULL);
}

void
FINCHFS_finalize()
{
	finchfs_term();
}

aiori_fd_t *
FINCHFS_create(char *fn, int flags, aiori_mod_opt_t *param)
{
	struct FINCHFS_File *bf;
	int fd;

	if (hints->dryRun)
		return (NULL);

	fd = finchfs_create(fn, flags, 0664);
	if (fd < 0)
		ERR("finchfs_create failed");
	bf = malloc(sizeof(*bf));
	bf->fd = fd;

	return ((aiori_fd_t *)bf);
}

aiori_fd_t *
FINCHFS_open(char *fn, int flags, aiori_mod_opt_t *param)
{
	struct FINCHFS_File *bf;
	int fd;

	if (hints->dryRun)
		return (NULL);

	fd = finchfs_open(fn, flags);
	if (fd < 0)
		ERR("finchfs_open failed");
	bf = malloc(sizeof(*bf));
	bf->fd = fd;

	return ((aiori_fd_t *)bf);
}

IOR_offset_t
FINCHFS_xfer(int access, aiori_fd_t *fd, IOR_size_t *buffer,
	IOR_offset_t len, IOR_offset_t offset, aiori_mod_opt_t *param)
{
	struct FINCHFS_File *bf = (struct FINCHFS_File *)fd;
	ssize_t r;

	if (hints->dryRun)
		return (len);

	switch (access) {
	case WRITE:
		r = finchfs_pwrite(bf->fd, buffer, len, offset);
		break;
	default:
		r = finchfs_pread(bf->fd, buffer, len, offset);
		break;
	}
	return (r);
}

void
FINCHFS_close(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
	struct FINCHFS_File *bf = (struct FINCHFS_File *)fd;

	if (hints->dryRun)
		return;

	finchfs_close(bf->fd);
	free(bf);
}

void
FINCHFS_delete(char *fn, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return;

	finchfs_unlink(fn);
}

char *
FINCHFS_version()
{
	return ((char *)finchfs_version());
}

void
FINCHFS_fsync(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
	struct FINCHFS_File *bf = (struct FINCHFS_File *)fd;

	if (hints->dryRun)
		return;

	finchfs_fsync(bf->fd);
}

IOR_offset_t
FINCHFS_get_file_size(aiori_mod_opt_t *param, char *fn)
{
	struct stat st;
	int r;

	if (hints->dryRun)
		return (0);

	r = finchfs_stat(fn, &st);
	if (r < 0)
		return (r);

	return (st.st_size);
}

int
FINCHFS_statfs(const char *fn, ior_aiori_statfs_t *st, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (0);
}

int
FINCHFS_mkdir(const char *fn, mode_t mode, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (finchfs_mkdir(fn, mode));
}

int
FINCHFS_rename(const char *oldfile, const char *newfile, aiori_mod_opt_t * param)
{
	if (hints->dryRun)
		return (0);
	return (finchfs_rename(oldfile, newfile));
}

int
FINCHFS_rmdir(const char *fn, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (finchfs_rmdir(fn));
}

int
FINCHFS_access(const char *fn, int mode, aiori_mod_opt_t *param)
{
	struct stat sb;

	if (hints->dryRun)
		return (0);

	return (finchfs_stat(fn, &sb));
}

int
FINCHFS_stat(const char *fn, struct stat *buf, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (finchfs_stat(fn, buf));
}

void
FINCHFS_sync(aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return;

	return;
}

ior_aiori_t finchfs_aiori = {
	.name = "FINCHFS",
	.name_legacy = NULL,
	.create = FINCHFS_create,
	.open = FINCHFS_open,
	.xfer_hints = FINCHFS_xfer_hints,
	.xfer = FINCHFS_xfer,
	.close = FINCHFS_close,
	.remove = FINCHFS_delete,
	.get_version = FINCHFS_version,
	.fsync = FINCHFS_fsync,
	.get_file_size = FINCHFS_get_file_size,
	.statfs = FINCHFS_statfs,
	.mkdir = FINCHFS_mkdir,
	.rename = FINCHFS_rename,
	.rmdir = FINCHFS_rmdir,
	.access = FINCHFS_access,
	.stat = FINCHFS_stat,
	.initialize = FINCHFS_initialize,
	.finalize = FINCHFS_finalize,
	.get_options = FINCHFS_options,
	.sync = FINCHFS_sync,
	.enable_mdtest = true,
};
