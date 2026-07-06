#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_CUDA

#include <cuda_runtime_api.h>
#include "gpu_runtime.h"

int
gpu_runtime_ok(gpu_runtime_status_t status)
{
    return status.code == 0;
}

const char *
gpu_runtime_strerror(gpu_runtime_status_t status)
{
    return cudaGetErrorString((cudaError_t) status.code);
}

gpu_runtime_status_t
gpu_runtime_get_device_count(int *count)
{
    gpu_runtime_status_t st;
    st.code = (int) cudaGetDeviceCount(count);
    return st;
}

gpu_runtime_status_t
gpu_runtime_set_device(int device)
{
    gpu_runtime_status_t st;
    st.code = (int) cudaSetDevice(device);
    return st;
}

gpu_runtime_status_t
gpu_runtime_malloc(void **ptr, size_t size)
{
    gpu_runtime_status_t st;
    st.code = (int) cudaMalloc(ptr, size);
    return st;
}

gpu_runtime_status_t
gpu_runtime_malloc_managed(void **ptr, size_t size)
{
    gpu_runtime_status_t st;
    st.code = (int) cudaMallocManaged(ptr, size, cudaMemAttachGlobal);
    return st;
}

gpu_runtime_status_t
gpu_runtime_free(void *ptr)
{
    gpu_runtime_status_t st;
    st.code = (int) cudaFree(ptr);
    return st;
}

gpu_runtime_status_t
gpu_runtime_memset(void *ptr, int value, size_t count)
{
    gpu_runtime_status_t st;
    st.code = (int) cudaMemset(ptr, value, count);
    return st;
}

gpu_runtime_status_t
gpu_runtime_memcpy(void *dst, const void *src, size_t count,
                   gpu_memcpy_kind_t kind)
{
    gpu_runtime_status_t st;
    enum cudaMemcpyKind cuda_kind;

    switch (kind) {
    case GPU_MEMCPY_HOST_TO_DEVICE:   cuda_kind = cudaMemcpyHostToDevice;   break;
    case GPU_MEMCPY_DEVICE_TO_HOST:   cuda_kind = cudaMemcpyDeviceToHost;   break;
    case GPU_MEMCPY_DEVICE_TO_DEVICE: cuda_kind = cudaMemcpyDeviceToDevice; break;
    case GPU_MEMCPY_HOST_TO_HOST:     cuda_kind = cudaMemcpyHostToHost;     break;
    default:                          cuda_kind = cudaMemcpyDefault;        break;
    }

    st.code = (int) cudaMemcpy(dst, src, count, cuda_kind);
    return st;
}

#endif
