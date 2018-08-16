#include "mdtest.h"
#include "aiori.h"

int main(int argc, char **argv) {
    aiori_initialize();
    MPI_Init(&argc, &argv);

    mdtest_run(argc, argv, MPI_COMM_WORLD, stdout);

    MPI_Finalize();
    aiori_finalize();
    return 0;
}
