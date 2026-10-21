/*
  This file contains GPU code for creating and checking memory patterns
  on the device.  Built with nvcc under HAVE_CUDA.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include <cuda_runtime.h>
#include "gpu_runtime.h"
#include "iordef.h"

#define RANDALGO_GOLDEN_RATIO_PRIME        0x9e37fffffffc0001UL

__global__ 
void cu_generate_memory_timestamp(uint64_t * buf, size_t length, int rand_seed, uint64_t pretendRank){
  size_t pos = blockIdx.x * blockDim.x + threadIdx.x;
  if(pos < length){
    buf[pos] = pretendRank | rand_seed + pos;
  }
}

__global__ 
void cu_generate_memory_incompressible(uint64_t * buf, size_t length, uint64_t seed){
  size_t pos = blockIdx.x * blockDim.x + threadIdx.x;
  if(pos < length){
    buf[pos] = seed | pos;
  }
}

__global__ 
void cu_verify_memory_timestamp(uint64_t item, uint64_t * buf, size_t length, int rand_seed, uint64_t pretendRank, int * errors){
  size_t pos = blockIdx.x * blockDim.x + threadIdx.x;
  if(pos < length){
    int correct = buf[pos] == (pretendRank | rand_seed + pos);
    if(! correct){
      atomicExch(errors, 1);
    }
  }
}

extern "C" void generate_memory_pattern_gpu(char * buf, size_t bytes, int rand_seed, int pretendRank, ior_dataPacketType_e dataPacketType){    
  size_t blocks = (bytes+2047)/2048;
  size_t threads = 256;
  switch(dataPacketType){
    case(DATA_RANDOM):
      // Nothing to do, will work on updates
      break;
    case(DATA_INCOMPRESSIBLE):{      
      cu_generate_memory_incompressible<<<blocks, threads>>>((uint64_t*) buf, bytes/sizeof(uint64_t), rand_seed + pretendRank);
      break;
    }case(DATA_OFFSET):{
    }case(DATA_TIMESTAMP):{
      cu_generate_memory_timestamp<<<blocks, threads>>>((uint64_t*) buf, bytes/sizeof(uint64_t), rand_seed, ((uint64_t) pretendRank) << 32);
      break;
    }
  }
}

extern "C" void update_write_memory_pattern_gpu(uint64_t item, char * buf, size_t bytes, int rand_seed, int rank, ior_dataPacketType_e dataPacketType){
  // nothing to do for dataPacketType == DATA_TIMESTAMP, i.e., won't be called for this parameter
}

extern "C" int verify_memory_pattern_gpu(uint64_t item, char * buffer, size_t bytes, int rand_seed, int pretendRank, ior_dataPacketType_e dataPacketType){
  int errors = 0;
  int result = -1;
  int *derror_found = NULL;
  size_t threads = 256;
  size_t words = bytes / sizeof(uint64_t);
  size_t blocks = words / threads + (words % threads != 0);

  if (dataPacketType != DATA_TIMESTAMP || bytes % sizeof(uint64_t) != 0)
    return -1;
  if (bytes == 0)
    return 0;
  if (!gpu_runtime_ok(gpu_runtime_malloc((void **)&derror_found, sizeof(int))))
    return -1;
  if (!gpu_runtime_ok(gpu_runtime_memcpy(derror_found, &errors, sizeof(int),
                                       GPU_MEMCPY_HOST_TO_DEVICE)))
    goto out;

  cu_verify_memory_timestamp<<<blocks, threads>>>(
      item, (uint64_t *)buffer, words, rand_seed,
      ((uint64_t)pretendRank) << 32, derror_found);
  if (cudaGetLastError() != cudaSuccess)
    goto out;
  if (cudaDeviceSynchronize() != cudaSuccess)
    goto out;
  if (!gpu_runtime_ok(gpu_runtime_memcpy(&errors, derror_found, sizeof(int),
                                       GPU_MEMCPY_DEVICE_TO_HOST)))
    goto out;
  result = errors;
out:
  if (!gpu_runtime_ok(gpu_runtime_free(derror_found)))
    result = -1;
  return result;
}
