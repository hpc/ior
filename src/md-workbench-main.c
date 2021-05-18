#include <mpi.h>

#include "md-workbench.h"

int main(int argc, char ** argv){
  MPI_Init(& argc, & argv);
  //phase_stat_t* results =
  md_workbench_run(argc, argv, MPI_COMM_WORLD, stdout);
  // API check, access the results of the first phase which is precrate.
  //printf("Max op runtime: %f\n", results->max_op_time);
  MPI_Finalize();
  return 0;
}
