#include "ior.h"

int main(int argc, char **argv)
{
    int errors = ior_main(argc, argv);

    /* Process exit statuses have a limited range; keep failures nonzero. */
    if (errors < 0 || errors > 255) {
        return 255;
    }
    return errors;
}
