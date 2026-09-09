#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_CUFILE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <cuda_runtime_api.h>
#include <cufile.h>

#include "gpu_io.h"
#include "aiori-debug.h"

struct gpu_io_file {
    CUfileHandle_t handle;
};

int
gpu_io_status_ok(gpu_io_status_t status)
{
    return status.ok;
}

const char *
gpu_io_strerror(gpu_io_status_t status, char *buf, size_t buflen)
{
    if (status.ok)
        return "success";

    if (status.message)
        snprintf(buf, buflen, "%s", status.message);
    else if (status.runtime_error)
        snprintf(buf, buflen, "CUDA error: %s",
                 cudaGetErrorString((cudaError_t) status.runtime_error));
    else if (status.backend_error)
        snprintf(buf, buflen, "cuFile error: %s (err=%ld)",
                 strerror((int) status.backend_error), status.backend_error);
    else if (status.errnum)
        snprintf(buf, buflen, "POSIX error: %s", strerror(status.errnum));
    else
        snprintf(buf, buflen, "unknown GPU I/O error");

    return buf;
}

static gpu_io_status_t
cufile_to_status(CUfileError_t err)
{
    gpu_io_status_t st;
    memset(&st, 0, sizeof(st));

    if (err.err == CU_FILE_SUCCESS) {
        st.ok = 1;
        return st;
    }

    st.ok = 0;

    if (IS_CUDA_ERR(err)) {
        st.runtime_error = (long) err.err;
        st.message = cudaGetErrorString(err.err);
    } else {
        st.backend_error = (long) err.err;
        st.message = strerror(err.err);
    }

    return st;
}

gpu_io_status_t
gpu_io_driver_open(void)
{
    CUfileError_t err = cuFileDriverOpen();
    return cufile_to_status(err);
}

gpu_io_status_t
gpu_io_driver_close(void)
{
    CUfileError_t err = cuFileDriverClose();
    return cufile_to_status(err);
}

static int gpu_io_driver_opened = 0;

gpu_io_status_t
gpu_io_register_fd(gpu_io_file_t **file, int fd)
{
    CUfileDescr_t descr;
    CUfileError_t err;

    if (!gpu_io_driver_opened) {
        gpu_io_status_t open_st = gpu_io_driver_open();
        if (!gpu_io_status_ok(open_st))
            return open_st;
        gpu_io_driver_opened = 1;
    }

    *file = (gpu_io_file_t *) malloc(sizeof(gpu_io_file_t));
    if (*file == NULL) {
        gpu_io_status_t st;
        memset(&st, 0, sizeof(st));
        st.ok = 0;
        st.errnum = ENOMEM;
        return st;
    }

    memset(&descr, 0, sizeof(descr));
    descr.handle.fd = fd;
    descr.type = CU_FILE_HANDLE_TYPE_OPAQUE_FD;

    err = cuFileHandleRegister(&(*file)->handle, &descr);
    if (err.err != CU_FILE_SUCCESS) {
        gpu_io_status_t st = cufile_to_status(err);
        free(*file);
        *file = NULL;
        return st;
    }

    {
        gpu_io_status_t ok;
        memset(&ok, 0, sizeof(ok));
        ok.ok = 1;
        return ok;
    }
}

void
gpu_io_deregister_fd(gpu_io_file_t **file)
{
    if (file == NULL || *file == NULL)
        return;

    cuFileHandleDeregister((*file)->handle);
    free(*file);
    *file = NULL;
}

/*
 * cuFileRead/Write return:
 *   >= 0 : bytes transferred
 *   < 0  : negative CUfileOpError_t.  Use CUFILE_ERRSTR(-rc).
 */
static void
cufile_xfer_decode(gpu_io_result_t *res)
{
    if (res->nbytes < 0) {
        int raw_err = (int) -res->nbytes;

        res->status.ok = 0;
        res->status.backend_error = (long) raw_err;
#ifdef CUFILE_ERRSTR
        res->status.message = CUFILE_ERRSTR(raw_err);
#else
        res->status.message = strerror(raw_err);
#endif
        res->nbytes = -1;
    } else {
        res->status.ok = 1;
    }
}

gpu_io_result_t
gpu_io_read(gpu_io_file_t *file, void *buffer,
            size_t size, int64_t file_offset, int64_t buffer_offset)
{
    gpu_io_result_t res;
    memset(&res, 0, sizeof(res));

    if (file == NULL) {
        res.nbytes = -1;
        res.status.ok = 0;
        res.status.errnum = EBADF;
        return res;
    }

    res.nbytes = (ssize_t) cuFileRead(file->handle, buffer, size,
                                      file_offset, buffer_offset);
    cufile_xfer_decode(&res);

    return res;
}

gpu_io_result_t
gpu_io_write(gpu_io_file_t *file, const void *buffer,
             size_t size, int64_t file_offset, int64_t buffer_offset)
{
    gpu_io_result_t res;
    memset(&res, 0, sizeof(res));

    if (file == NULL) {
        res.nbytes = -1;
        res.status.ok = 0;
        res.status.errnum = EBADF;
        return res;
    }

    res.nbytes = (ssize_t) cuFileWrite(file->handle, buffer, size,
                                       file_offset, buffer_offset);
    cufile_xfer_decode(&res);

    return res;
}

#endif
