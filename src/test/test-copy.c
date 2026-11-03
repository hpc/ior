#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ior.h"

int main(int argc, char **argv)
{
    IOR_param_t params;
    IOR_test_t *first = NULL, *second = NULL, *fallback = NULL;
    char *automatic;
    int status = EXIT_FAILURE;

    MPI_Init(&argc, &argv);
    init_IOR_Param_t(&params, MPI_COMM_WORLD);
    automatic = GetPlatformName();
    if (params.platform == NULL || params.platform[0] == '\0') {
        fprintf(stderr, "default platform is empty\n");
        goto done;
    }
    free(params.platform);
    params.platform = strdup("retained-platform");
    first = CreateTest(&params, 0);
    second = CreateTest(&first->params, 1);
    if (strcmp(first->params.platform, params.platform)
        || strcmp(second->params.platform, params.platform)
        || first->params.platform == params.platform
        || second->params.platform == first->params.platform) {
        fprintf(stderr, "test copy lost the platform value or its ownership\n");
        goto done;
    }
    first->params.platform[0] = 'R';
    if (strcmp(second->params.platform, "retained-platform")) {
        fprintf(stderr, "modifying one test changed its copy\n");
        goto done;
    }
    free(params.platform);
    params.platform = NULL;
    fallback = CreateTest(&params, 2);
    if (strcmp(fallback->params.platform, automatic)) {
        fprintf(stderr, "missing platform did not retain automatic detection\n");
        goto done;
    }
    status = EXIT_SUCCESS;
    puts("automatic platform, explicit values, and independent test copies verified");
done:
    free(automatic);
    free(params.platform);
    free(params.api);
    free(params.testFileName);
    if (first) { free(first->params.platform); free(first); }
    if (second) { free(second->params.platform); free(second); }
    if (fallback) { free(fallback->params.platform); free(fallback); }
    MPI_Finalize();
    return status;
}
