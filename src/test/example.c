#include <assert.h>

#include <ior.h>
#include <ior-internal.h>

// build a single test via, e.g., mpicc example.c -I ../src/ ../src/libaiori.a -lm

int main(){
  IOR_param_t test;
  init_IOR_Param_t(& test);
  test.blockSize = 10;
  test.transferSize = 10;
  test.segmentCount = 5;
  test.numTasks = 2;

  // having an individual file
  test.filePerProc = 1;

  IOR_offset_t * offsets;
  offsets = GetOffsetArraySequential(& test, 0);
  assert(offsets[0] == 0);
  assert(offsets[1] == 10);
  assert(offsets[2] == 20);
  assert(offsets[3] == 30);
  assert(offsets[4] == 40);
  // for(int i = 0; i < test.segmentCount; i++){
  //   printf("%lld\n", (long long int) offsets[i]);
  // }
  printf("OK\n");
  return 0;
}
