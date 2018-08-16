#include "../ior.h"
#include "../mdtest.h"

int main(int argc, char ** argv){
  int rank;
  int ret = 0;

  MPI_Init(& argc, & argv);
  MPI_Comm_rank(MPI_COMM_WORLD, & rank);

  if (rank == 0){
    char * param[] = {"./ior", "-a", "DUMMY"};
    IOR_test_t * res = ior_run(3, param, MPI_COMM_SELF, stdout);
    if (res == NULL)
    {
        fprintf(stderr, "Could not run ior\n");
        ret = 1;
    }
  }
  if (rank == 0){
    char * param[] = {"./mdtest", "-a", "DUMMY"};
    mdtest_results_t * res = mdtest_run(3, param, MPI_COMM_SELF, stdout);
    if (res == NULL)
    {
        fprintf(stderr, "Could not run mdtest\n");
        ret = 1;
    }
  }
  MPI_Finalize();

  return ret;
}
