#include <assert.h>

#include "../ior.h"
#include "../ior-internal.h"

// Run all tests via:
// make distcheck
// build a single test via, e.g., mpicc example.c -I ../src/ ../src/libaiori.a -lm

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  IOR_param_t test;
  init_IOR_Param_t(& test, MPI_COMM_WORLD);
  test.blockSize = 10;
  test.transferSize = 10;
  test.segmentCount = 5;
  test.numTasks = 2;

  // having an individual file
  test.filePerProc = 1;

  printf("OK\n");
  MPI_Finalize();
  return 0;
}
