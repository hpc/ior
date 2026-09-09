#ifndef _UTILITIES_RAND_H
#define _UTILITIES_RAND_H

#include <stdint.h>

typedef void LFSR;
typedef void LFSRConfig;
typedef void LFSRRange;

/* 
Create a single LFSR for an arbitrary number of blocks covering the full range, e.g. 2^3 + 2^4 + 2^7 file size
*/
LFSRRange * u_lfsr_range_init (uint64_t blocks, unsigned seed);

uint64_t u_lfsr_range_step (LFSRRange * lfsr);


#endif