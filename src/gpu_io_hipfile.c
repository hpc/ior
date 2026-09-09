/*
 * hipFile return values:
 *   >= 0  : bytes transferred
 *   == -1 : POSIX error (errno)
 *   <  -1 : -(hipFileOpError_t), use HIPFILE_ERRSTR(-rc)
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_HIPFILE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <hip/hip_runtime_api.h>
#include <hipfile.h>

#include "gpu_io.h"
#include "aiori-debug.h"

struct gpu_io_file {
    hipFileHandle_t handle;
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
        snprintf(buf, buflen, "HIP error: %s",
                 hipGetErrorString((hipError_t) status.runtime_error));
    else if (status.backend_error)
        snprintf(buf, buflen, "hipFile error: %s (err=%ld)",
                 HIPFILE_ERRSTR((int) status.backend_error),
                 status.backend_error);
    else if (status.errnum)
        snprintf(buf, buflen, "POSIX error: %s", strerror(status.errnum));
    else
        snprintf(buf, buflen, "unknown GPU I/O error");

    return buf;
}

static gpu_io_status_t
hipfile_to_status(hipFileError_t err)
{
    gpu_io_status_t st;
    memset(&st, 0, sizeof(st));

    if (err.err == hipFileSuccess) {
        st.ok = 1;
        return st;
    }

    st.ok = 0;

    if (IS_HIP_DRV_ERR(err)) {
        st.runtime_error = (long) HIP_DRV_ERR(err);
        st.message = hipGetErrorString(HIP_DRV_ERR(err));
    } else {
        st.backend_error = (long) err.err;
        st.message = HIPFILE_ERRSTR(err.err);
    }

    return st;
}

gpu_io_status_t
gpu_io_driver_open(void)
{
    hipFileError_t err = hipFileDriverOpen();
    return hipfile_to_status(err);
}

gpu_io_status_t
gpu_io_driver_close(void)
{
    hipFileError_t err = hipFileDriverClose();
    return hipfile_to_status(err);
}

/* hipFileHandleRegister() auto-initializes the library on first call. */
gpu_io_status_t
gpu_io_register_fd(gpu_io_file_t **file, int fd)
{
    hipFileDescr_t descr;
    hipFileError_t err;

    *file = (gpu_io_file_t *) malloc(sizeof(gpu_io_file_t));
    if (*file == NULL) {
        gpu_io_status_t st;
        memset(&st, 0, sizeof(st));
        st.ok = 0;
        st.errnum = ENOMEM;
        return st;
    }

    memset(&descr, 0, sizeof(descr));
    descr.type = hipFileHandleTypeOpaqueFD;
    descr.handle.fd = fd;
    descr.fs_ops = NULL;

    err = hipFileHandleRegister(&(*file)->handle, &descr);
    if (err.err != hipFileSuccess) {
        gpu_io_status_t st = hipfile_to_status(err);
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

    hipFileHandleDeregister((*file)->handle);
    free(*file);
    *file = NULL;
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

    res.nbytes = (ssize_t) hipFileRead(file->handle, buffer, size,
                                       (hoff_t) file_offset,
                                       (hoff_t) buffer_offset);

    if (res.nbytes == -1) {
        res.status.ok = 0;
        res.status.errnum = errno;
        res.status.message = strerror(errno);
        res.nbytes = -1;
    } else if (res.nbytes < -1) {
        int raw_err = (int) -res.nbytes;
        res.status.ok = 0;
        res.status.backend_error = (long) raw_err;
        res.status.message = HIPFILE_ERRSTR(raw_err);
        res.nbytes = -1;
    } else {
        res.status.ok = 1;
    }

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

    res.nbytes = (ssize_t) hipFileWrite(file->handle, buffer, size,
                                        (hoff_t) file_offset,
                                        (hoff_t) buffer_offset);

    if (res.nbytes == -1) {
        res.status.ok = 0;
        res.status.errnum = errno;
        res.status.message = strerror(errno);
        res.nbytes = -1;
    } else if (res.nbytes < -1) {
        int raw_err = (int) -res.nbytes;
        res.status.ok = 0;
        res.status.backend_error = (long) raw_err;
        res.status.message = HIPFILE_ERRSTR(raw_err);
        res.nbytes = -1;
    } else {
        res.status.ok = 1;
    }

    return res;
}

#endif
