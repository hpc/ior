#include <sys/types.h>
#include <sys/stat.h>
#include <chfs.h>
#include "ior.h"
#include "aiori.h"

static aiori_xfer_hint_t *hints = NULL;

struct CHFS_File {
	int fd;
};

struct chfs_option {
	size_t chunk_size;
};

option_help *
CHFS_options(aiori_mod_opt_t **init_backend_options,
	aiori_mod_opt_t *init_values)
{
	struct chfs_option *o = malloc(sizeof(*o));

	if (init_values != NULL)
		memcpy(o, init_values, sizeof(*o));
	else
		memset(o, 0, sizeof(*o));

	if (o->chunk_size > 0)
		chfs_set_chunk_size(o->chunk_size);

	*init_backend_options = (aiori_mod_opt_t *)o;

	option_help h[] = {
	    {0, "chfs.chunk_size", "chunk size", OPTION_FLAG, 'd',
		    &o->chunk_size},
	    LAST_OPTION
	};
	option_help *help = malloc(sizeof(h));
	memcpy(help, h, sizeof(h));

	return (help);
}

void
CHFS_xfer_hints(aiori_xfer_hint_t *params)
{
	hints = params;
}

void
CHFS_initialize()
{
	chfs_init(NULL);
}

void
CHFS_finalize()
{
	chfs_term();
}

aiori_fd_t *
CHFS_create(char *fn, int flags, aiori_mod_opt_t *param)
{
	struct CHFS_File *bf;
	int fd;

	if (hints->dryRun)
		return (NULL);

	fd = chfs_create(fn, flags, 0664);
	if (fd < 0)
		ERR("chfs_create failed");
	bf = malloc(sizeof(*bf));
	bf->fd = fd;

	return ((aiori_fd_t *)bf);
}

aiori_fd_t *
CHFS_open(char *fn, int flags, aiori_mod_opt_t *param)
{
	struct CHFS_File *bf;
	int fd;

	if (hints->dryRun)
		return (NULL);

	fd = chfs_open(fn, flags);
	if (fd < 0)
		ERR("chfs_open failed");
	bf = malloc(sizeof(*bf));
	bf->fd = fd;

	return ((aiori_fd_t *)bf);
}

IOR_offset_t
CHFS_xfer(int access, aiori_fd_t *fd, IOR_size_t *buffer,
	IOR_offset_t len, IOR_offset_t offset, aiori_mod_opt_t *param)
{
	struct CHFS_File *bf = (struct CHFS_File *)fd;
	ssize_t r;

	if (hints->dryRun)
		return (len);

	switch (access) {
	case WRITE:
		r = chfs_pwrite(bf->fd, buffer, len, offset);
		break;
	default:
		r = chfs_pread(bf->fd, buffer, len, offset);
		break;
	}
	return (r);
}

void
CHFS_close(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
	struct CHFS_File *bf = (struct CHFS_File *)fd;

	if (hints->dryRun)
		return;

	chfs_close(bf->fd);
	free(bf);
}

void
CHFS_delete(char *fn, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return;

	chfs_unlink(fn);
}

char *
CHFS_version()
{
	return ((char *)chfs_version());
}

void
CHFS_fsync(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
	struct CHFS_File *bf = (struct CHFS_File *)fd;

	if (hints->dryRun)
		return;

	chfs_fsync(bf->fd);
}

IOR_offset_t
CHFS_get_file_size(aiori_mod_opt_t *param, char *fn)
{
	struct stat st;
	int r;

	if (hints->dryRun)
		return (0);

	r = chfs_stat(fn, &st);
	if (r < 0)
		return (r);

	return (st.st_size);
}

int
CHFS_statfs(const char *fn, ior_aiori_statfs_t *st, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (0);
}

int
CHFS_mkdir(const char *fn, mode_t mode, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (chfs_mkdir(fn, mode));
}

int
CHFS_rmdir(const char *fn, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (chfs_rmdir(fn));
}

int
CHFS_access(const char *fn, int mode, aiori_mod_opt_t *param)
{
	struct stat sb;

	if (hints->dryRun)
		return (0);

	return (chfs_stat(fn, &sb));
}

int
CHFS_stat(const char *fn, struct stat *buf, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (chfs_stat(fn, buf));
}

void
CHFS_sync(aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return;

	return;
}

ior_aiori_t chfs_aiori = {
	.name = "CHFS",
	.name_legacy = NULL,
	.create = CHFS_create,
	.open = CHFS_open,
	.xfer_hints = CHFS_xfer_hints,
	.xfer = CHFS_xfer,
	.close = CHFS_close,
	.remove = CHFS_delete,
	.get_version = CHFS_version,
	.fsync = CHFS_fsync,
	.get_file_size = CHFS_get_file_size,
	.statfs = CHFS_statfs,
	.mkdir = CHFS_mkdir,
	.rmdir = CHFS_rmdir,
	.access = CHFS_access,
	.stat = CHFS_stat,
	.initialize = CHFS_initialize,
	.finalize = CHFS_finalize,
	.get_options = CHFS_options,
	.sync = CHFS_sync,
	.enable_mdtest = true,
};
