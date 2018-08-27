#include "mdtest.h"
#include "aiori.h"

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    aiori_initialize(NULL);

    mdtest_run(argc, argv, MPI_COMM_WORLD, stdout);

    aiori_finalize(NULL);
    MPI_Finalize();
    return 0;
}
