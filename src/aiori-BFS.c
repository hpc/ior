#include <sys/types.h>
#include <sys/stat.h>
#include <dfs.h>
#include "ior.h"
#include "aiori.h"

static aiori_xfer_hint_t *hints = NULL;

struct BFS_File {
	int fd;
};

struct bfs_option {
	size_t chunk_size;
};

option_help *
BFS_options(aiori_mod_opt_t **init_backend_options,
	aiori_mod_opt_t *init_values)
{
	struct bfs_option *o = malloc(sizeof(*o));

	if (init_values != NULL)
		memcpy(o, init_values, sizeof(*o));
	else
		o->chunk_size = 4096;

	*init_backend_options = (aiori_mod_opt_t *)o;

	option_help h[] = {
	    {0, "bfs.chunk_size", "chunk size", OPTION_FLAG, 'd',
		    &o->chunk_size},
	    LAST_OPTION
	};
	option_help *help = malloc(sizeof(h));
	memcpy(help, h, sizeof(h));

	return (help);
}

void
BFS_xfer_hints(aiori_xfer_hint_t *params)
{
	hints = params;
}

void
BFS_initialize()
{
	dfs_init(NULL);
}

void
BFS_finalize()
{
	dfs_term();
}

aiori_fd_t *
BFS_create(char *fn, int flags, aiori_mod_opt_t *param)
{
	struct BFS_File *bf;
	struct bfs_option *o = (struct bfs_option *)param;
	int fd;

	if (hints->dryRun)
		return (NULL);

	fd = dfs_create_chunk_size(fn, 0664, flags, o->chunk_size);
	if (fd < 0)
		ERR("dfs_create failed");
	bf = malloc(sizeof(*bf));
	bf->fd = fd;

	return ((aiori_fd_t *)bf);
}

aiori_fd_t *
BFS_open(char *fn, int flags, aiori_mod_opt_t *param)
{
	struct BFS_File *bf;
	int fd;

	if (hints->dryRun)
		return (NULL);

	fd = dfs_open(fn, flags);
	if (fd < 0)
		ERR("dfs_open failed");
	bf = malloc(sizeof(*bf));
	bf->fd = fd;

	return ((aiori_fd_t *)bf);
}

IOR_offset_t
BFS_xfer(int access, aiori_fd_t *fd, IOR_size_t *buffer,
	IOR_offset_t len, IOR_offset_t offset, aiori_mod_opt_t *param)
{
	struct BFS_File *bf = (struct BFS_File *)fd;
	ssize_t r;

	if (hints->dryRun)
		return (len);

	switch (access) {
	case WRITE:
		r = dfs_pwrite(bf->fd, buffer, len, offset);
		break;
	case READ:
		r = dfs_pread(bf->fd, buffer, len, offset);
		break;
	}
	return (r);
}

void
BFS_close(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
	struct BFS_File *bf = (struct BFS_File *)fd;

	if (hints->dryRun)
		return;

	dfs_close(bf->fd);
	free(bf);
}

void
BFS_delete(char *fn, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return;

	dfs_unlink(fn);
}

char *
BFS_version()
{
	return ("1.0.0");
}

void
BFS_fsync(aiori_fd_t *fd, aiori_mod_opt_t *param)
{
	struct BFS_File *bf = (struct BFS_File *)fd;

	if (hints->dryRun)
		return;

	dfs_fsync(bf->fd);
}

IOR_offset_t
BFS_get_file_size(aiori_mod_opt_t *param, char *fn)
{
	struct stat st;
	int r;

	if (hints->dryRun)
		return (0);

	r = dfs_stat(fn, &st);
	if (r < 0)
		return (r);

	return (st.st_size);
}

int
BFS_statfs(const char *fn, ior_aiori_statfs_t *st, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (0);
}

int
BFS_mkdir(const char *fn, mode_t mode, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (dfs_mkdir(fn, mode));
}

int
BFS_rmdir(const char *fn, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (dfs_rmdir(fn));
}

int
BFS_access(const char *fn, int mode, aiori_mod_opt_t *param)
{
	struct stat sb;

	if (hints->dryRun)
		return (0);

	return (dfs_stat(fn, &sb));
}

int
BFS_stat(const char *fn, struct stat *buf, aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return (0);

	return (dfs_stat(fn, buf));
}

void
BFS_sync(aiori_mod_opt_t *param)
{
	if (hints->dryRun)
		return;

	return;
}

ior_aiori_t bfs_aiori = {
	.name = "BFS",
	.name_legacy = NULL,
	.create = BFS_create,
	.open = BFS_open,
	.xfer_hints = BFS_xfer_hints,
	.xfer = BFS_xfer,
	.close = BFS_close,
	.delete = BFS_delete,
	.get_version = BFS_version,
	.fsync = BFS_fsync,
	.get_file_size = BFS_get_file_size,
	.statfs = BFS_statfs,
	.mkdir = BFS_mkdir,
	.rmdir = BFS_rmdir,
	.access = BFS_access,
	.stat = BFS_stat,
	.initialize = BFS_initialize,
	.finalize = BFS_finalize,
	.get_options = BFS_options,
	.sync = BFS_sync,
	.enable_mdtest = true,
};
