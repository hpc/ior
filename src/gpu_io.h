#ifndef GPU_IO_H
#define GPU_IO_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gpu_io_file gpu_io_file_t;

typedef struct {
    int         ok;
    int         errnum;
    long        backend_error;
    long        runtime_error;
    const char *message;
} gpu_io_status_t;

typedef struct {
    ssize_t         nbytes;
    gpu_io_status_t status;
} gpu_io_result_t;

int          gpu_io_status_ok(gpu_io_status_t status);
const char  *gpu_io_strerror(gpu_io_status_t status, char *buf, size_t buflen);

gpu_io_status_t gpu_io_driver_open(void);
gpu_io_status_t gpu_io_driver_close(void);

gpu_io_status_t gpu_io_register_fd(gpu_io_file_t **file, int fd);
void gpu_io_deregister_fd(gpu_io_file_t **file);

gpu_io_result_t gpu_io_read(gpu_io_file_t *file, void *buffer,
                             size_t size, int64_t file_offset,
                             int64_t buffer_offset);

gpu_io_result_t gpu_io_write(gpu_io_file_t *file, const void *buffer,
                              size_t size, int64_t file_offset,
                              int64_t buffer_offset);

#ifdef __cplusplus
}
#endif

#endif
