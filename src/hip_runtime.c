#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_HIP

#include <hip/hip_runtime_api.h>
#include "gpu_runtime.h"

int
gpu_runtime_ok(gpu_runtime_status_t status)
{
    return status.code == 0;
}

const char *
gpu_runtime_strerror(gpu_runtime_status_t status)
{
    return hipGetErrorString((hipError_t) status.code);
}

gpu_runtime_status_t
gpu_runtime_get_device_count(int *count)
{
    gpu_runtime_status_t st;
    st.code = (int) hipGetDeviceCount(count);
    return st;
}

gpu_runtime_status_t
gpu_runtime_set_device(int device)
{
    gpu_runtime_status_t st;
    st.code = (int) hipSetDevice(device);
    return st;
}

gpu_runtime_status_t
gpu_runtime_malloc(void **ptr, size_t size)
{
    gpu_runtime_status_t st;
    st.code = (int) hipMalloc(ptr, size);
    return st;
}

gpu_runtime_status_t
gpu_runtime_malloc_managed(void **ptr, size_t size)
{
    gpu_runtime_status_t st;
    st.code = (int) hipMallocManaged(ptr, size, hipMemAttachGlobal);
    return st;
}

gpu_runtime_status_t
gpu_runtime_free(void *ptr)
{
    gpu_runtime_status_t st;
    st.code = (int) hipFree(ptr);
    return st;
}

gpu_runtime_status_t
gpu_runtime_memset(void *ptr, int value, size_t count)
{
    gpu_runtime_status_t st;
    st.code = (int) hipMemset(ptr, value, count);
    return st;
}

gpu_runtime_status_t
gpu_runtime_memcpy(void *dst, const void *src, size_t count,
                   gpu_memcpy_kind_t kind)
{
    gpu_runtime_status_t st;
    enum hipMemcpyKind hip_kind;

    switch (kind) {
    case GPU_MEMCPY_HOST_TO_DEVICE:   hip_kind = hipMemcpyHostToDevice;   break;
    case GPU_MEMCPY_DEVICE_TO_HOST:   hip_kind = hipMemcpyDeviceToHost;   break;
    case GPU_MEMCPY_DEVICE_TO_DEVICE: hip_kind = hipMemcpyDeviceToDevice; break;
    case GPU_MEMCPY_HOST_TO_HOST:     hip_kind = hipMemcpyHostToHost;     break;
    default:                          hip_kind = hipMemcpyDefault;        break;
    }

    st.code = (int) hipMemcpy(dst, src, count, hip_kind);
    return st;
}

#endif
