#include <mpi.h>

#include "md-workbench.h"

int main(int argc, char ** argv){
  MPI_Init(& argc, & argv);
  int ret = md_workbench_run(argc, argv, MPI_COMM_WORLD, stdout);
  MPI_Finalize();
  return ret;
}
