#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ior.h"
#include "../utilities.h"
#include "../option.h"

void ParseLine(char *line, IOR_param_t *params, options_all_t *options);

int main(void)
{
    IOR_param_t params = {0};
    char expected[MAX_STR];
    char line[3 * MAX_STR];
    char original[3 * MAX_STR];
    char *backend_value = NULL;
    option_help help[] = {
        {0, "probe.string", "String directive boundary", OPTION_OPTIONAL_ARGUMENT,
         's', &backend_value},
        LAST_OPTION
    };
    option_module module = {NULL, help, NULL};
    options_all_t options = {1, &module};
    const size_t lengths[] = {1, 7, MAX_STR - 1};

    out_logfile = stdout;
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        size_t length = lengths[i];
        memset(expected, 'x', length);
        if (length >= 7)
            memcpy(expected + 2, " \t=", 3);
        expected[length] = '\0';
        snprintf(line, sizeof(line), " platform = %s \t# ignored", expected);
        strcpy(original, line);
        ParseLine(line, &params, &options);
        if (strcmp(params.platform, expected) || strcmp(line, original))
            return EXIT_FAILURE;
        free(params.platform);
        params.platform = NULL;

        snprintf(line, sizeof(line), " --probe.string = %s \t", expected);
        strcpy(original, line);
        ParseLine(line, &params, &options);
        if (strcmp(backend_value, expected) || strcmp(line, original))
            return EXIT_FAILURE;
        free(backend_value);
        backend_value = NULL;
        printf("native and backend directive values: %zu bytes preserved\n", length);
    }
    return EXIT_SUCCESS;
}
