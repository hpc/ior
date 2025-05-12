/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
\******************************************************************************/

#ifndef _UTILITIES_H
#define _UTILITIES_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <mpi.h>
#include "ior.h"

extern int rank;
extern int rankOffset;
extern int verbose;
extern MPI_Comm testComm;
extern FILE * out_resultfile;
extern enum OutputFormat_t outputFormat;  /* format of the output */

/*
 * Try using the system's PATH_MAX, which is what realpath and such use.
 */
#define MAX_PATHLEN PATH_MAX
#define ERROR_LOCATION __func__


void* safeMalloc(uint64_t size);
void set_o_direct_flag(int *fd);

ior_dataPacketType_e parsePacketType(char t);
void update_write_memory_pattern(uint64_t item, char * buf, size_t bytes, int rand_seed, int rank, ior_dataPacketType_e dataPacketType, ior_memory_flags type);
void update_write_memory_pattern_gpu(uint64_t item, char * buf, size_t bytes, int rand_seed, int rank, ior_dataPacketType_e dataPacketType);
void generate_memory_pattern(char * buf, size_t bytes, int rand_seed, int rank, ior_dataPacketType_e dataPacketType, ior_memory_flags type);
void generate_memory_pattern_gpu(char * buf, size_t bytes, int rand_seed, int rank, ior_dataPacketType_e dataPacketType);
/* invalidate memory in the buffer */
void invalidate_buffer_pattern(char * buf, size_t bytes, ior_memory_flags type);

/* check a data buffer, @return 0 if all is correct, otherwise 1 */
int verify_memory_pattern(uint64_t item, char * buffer, size_t bytes, int rand_seed, int pretendRank, ior_dataPacketType_e dataPacketType, ior_memory_flags type);
int verify_memory_pattern_gpu(uint64_t item, char * buffer, size_t bytes, int rand_seed, int pretendRank, ior_dataPacketType_e dataPacketType);

void PrintKeyVal(char * key, char * value);
void initCUDA(int blockMapping, int rank, int numNodes, int tasksPerNode, int useGPUID);
char *CurrentTimeString(void);
int Regex(char *, char *);
void ShowFileSystemSize(char * filename, const struct ior_aiori * backend, void * backend_options);
void DumpBuffer(void *, size_t);
void SetHints (MPI_Info *, char *);
void ShowHints (MPI_Info *);
char *HumanReadable(IOR_offset_t value, int base);
int QueryNodeMapping(MPI_Comm comm, int print_nodemap);
int GetNumNodes(MPI_Comm);
int GetNumTasks(MPI_Comm);
int GetNumTasksOnNode0(MPI_Comm);
void DelaySecs(int delay);
void updateParsedOptions(IOR_param_t * options, options_all_t * global_options);
size_t NodeMemoryStringToBytes(char *size_str);

typedef struct OpTimer OpTimer;
OpTimer* OpTimerInit(char * filename, int size);
void OpTimerValue(OpTimer* otimer_in, double now, double runTime);
void OpTimerFlush(OpTimer* otimer_in);
void OpTimerFree(OpTimer** otimer_in);

/* Returns -1, if cannot be read  */
int64_t ReadStoneWallingIterations(char * const filename, MPI_Comm com);
void StoreStoneWallingIterations(char * const filename, int64_t count);

void init_clock(MPI_Comm com);
double GetTimeStamp(void);
char * PrintTimestamp(void); // TODO remove this function
unsigned long GetProcessorAndCore(int *chip, int *core);
void *aligned_buffer_alloc(size_t size, ior_memory_flags type);
void aligned_buffer_free(void *buf, ior_memory_flags type);

void srand64(uint64_t state);
uint64_t rand64(void);
#endif  /* !_UTILITIES_H */
