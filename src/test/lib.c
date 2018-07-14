#include "../ior.h"
#include "../mdtest.h"

int main(int argc, char ** argv){
  int rank;
  MPI_Init(& argc, & argv);
  MPI_Comm_rank(MPI_COMM_WORLD, & rank);

  if (rank == 0){
    char * param[] = {"./ior", "-a", "DUMMY"};
    IOR_test_t * res = ior_run(3, param, MPI_COMM_SELF, stdout);
  }
  if (rank == 0){
    char * param[] = {"./mdtest", "-a", "DUMMY"};
    mdtest_results_t * res = mdtest_run(3, param, MPI_COMM_SELF, stdout);
  }
  MPI_Finalize();

  return 0;
}
