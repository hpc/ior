#include <cuda_runtime.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iordef.h"

#define RANDALGO_GOLDEN_RATIO_PRIME        0x9e37fffffffc0001UL

__global__ 
void cu_generate_memory_timestamp(float * buf, size_t bytes, int rand_seed, int pretendRank){
  
}

extern "C" void generate_memory_pattern_gpu(char * buf, size_t bytes, int rand_seed, int pretendRank, ior_dataPacketType_e dataPacketType){    
  if(dataPacketType == DATA_TIMESTAMP){
    cu_generate_memory_timestamp<<<bytes, 1>>>((float*)bytes, bytes, rand_seed, pretendRank);
  }
}
