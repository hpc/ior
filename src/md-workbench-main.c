#include <mpi.h>

#include "md-workbench.h"

int main(int argc, char ** argv){
  MPI_Init(& argc, & argv);
  int ret = md_workbench(argc, argv);
  MPI_Finalize();
  return ret;
}
