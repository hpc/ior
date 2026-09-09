#ifndef GPU_RUNTIME_H
#define GPU_RUNTIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPU_MEMCPY_HOST_TO_DEVICE,
    GPU_MEMCPY_DEVICE_TO_HOST,
    GPU_MEMCPY_DEVICE_TO_DEVICE,
    GPU_MEMCPY_HOST_TO_HOST
} gpu_memcpy_kind_t;

typedef struct {
    int code;
} gpu_runtime_status_t;

int              gpu_runtime_ok(gpu_runtime_status_t status);
const char      *gpu_runtime_strerror(gpu_runtime_status_t status);

gpu_runtime_status_t gpu_runtime_get_device_count(int *count);
gpu_runtime_status_t gpu_runtime_set_device(int device);

gpu_runtime_status_t gpu_runtime_malloc(void **ptr, size_t size);
gpu_runtime_status_t gpu_runtime_malloc_managed(void **ptr, size_t size);
gpu_runtime_status_t gpu_runtime_free(void *ptr);
gpu_runtime_status_t gpu_runtime_memset(void *ptr, int value, size_t count);
gpu_runtime_status_t gpu_runtime_memcpy(void *dst, const void *src,
                                        size_t count, gpu_memcpy_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif
