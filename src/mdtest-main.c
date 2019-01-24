#include "mdtest.h"
#include "aiori.h"

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    mdtest_run(argc, argv, MPI_COMM_WORLD, stdout);
    MPI_Finalize();

    return 0;
}
