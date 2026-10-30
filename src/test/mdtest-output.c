#include "../mdtest.h"

#include <stdlib.h>
#include <string.h>

/* Exercise the FILE* API independently of the command-line stdout path. */
int main(int argc, char **argv)
{
    int rank, failed = 0, all_failed;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    char *backend = argc > 1 ? argv[1] : "DUMMY";

    for (int call = 0; call < 3; call++) {
        FILE *output = tmpfile();
        if (output == NULL) MPI_Abort(MPI_COMM_WORLD, 1);
        char summary_format[] = "--summaryFormat=JSON";
        char *params[] = {"mdtest", "-a", backend, "-F", "-n", "1", "-i", "2",
                          summary_format};
        const int json = call == 1;
        mdtest_results_t *results = mdtest_run(json ? 9 : 8, params,
                                              MPI_COMM_WORLD, output);
        if (results == NULL || out_logfile != output || out_resultfile != output) {
            fprintf(stderr, "mdtest did not preserve caller output streams: call %d, rank %d\n",
                    call, rank);
            failed = 1;
        }
        free(results);
        if (fflush(output) != 0 || ferror(output)) failed = 1;
        rewind(output);
        if (rank == 0) {
            char content[32768];
            size_t length = fread(content, 1, sizeof(content) - 1, output);
            content[length] = '\0';
            if (json ? (content[0] != '{' || strstr(content, "\"summary\"") == NULL)
                     : strstr(content, "SUMMARY") == NULL) {
                fprintf(stderr, "mdtest output missing from caller stream: call %d\n", call);
                failed = 1;
            }
        }
        /* The library must leave ownership and write access with its caller. */
        if (fseek(output, 0, SEEK_END) != 0 || fputs("caller retains ownership\n", output) < 0 ||
            fclose(output) != 0) failed = 1;
    }

    MPI_Allreduce(&failed, &all_failed, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Finalize();
    return all_failed;
}
