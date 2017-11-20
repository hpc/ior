/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2006, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Purpose:
*       Changes a specific Byte offset In File
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR(MSG) do {                                \
    fprintf(stdout, "** error **\n");                \
    fprintf(stdout, "ERROR in %s (line %d): %s.\n",  \
            __FILE__, __LINE__, MSG);                \
    fprintf(stdout, "ERROR: %s\n", strerror(errno)); \
    fprintf(stdout, "** exiting **\n");              \
    fflush(stdout);                                  \
    exit(1);                                         \
} while (0)

#define BYTE_BOUNDARY 8

#ifndef   open64                /* necessary for TRU64 -- */
#  define open64  open            /* unlikely, but may pose */
#endif  /* not open64 */                        /* conflicting prototypes */

#ifndef   lseek64               /* necessary for TRU64 -- */
#  define lseek64 lseek           /* unlikely, but may pose */
#endif  /* not lseek64 */                        /* conflicting prototypes */

int
main(int argc, char **argv)
{
    int i;
    int fd;
    int value;
    unsigned char oldBuffer[BYTE_BOUNDARY];
    unsigned char newBuffer[BYTE_BOUNDARY];
    char *fileName;
    long long int offset;
    long long int alignedOffset;
    long long int bufferIndex;
    long long int indexMask   = (long long int)(BYTE_BOUNDARY - 1);
    long long int alignedMask = (long long int)(BYTE_BOUNDARY - 1)
                                ^ (long long int)(-1);

    /* check usage */
    if (argc != 3 && argc !=4) {
        printf("Usage: %s <filename> <offset> [<newValue>]\n", argv[0]);
        printf("Returns value of byte offset, or modifies to new value.\n");
        exit(1);
    }

    /* gather arguments */
    fileName = argv[1];
    sscanf(argv[2], "%lld", &offset);
    alignedOffset = offset & alignedMask;
    bufferIndex = offset & indexMask;
    if (argc == 4) {
        sscanf(argv[3], "%d", &value);
        if (value < 0 || value > 255) ERR("ERROR: <value> must be 0-255");
    }

    /* open file */
    fd = open64(fileName, O_RDWR);
    if (fd < 0) ERR("ERROR: Unable to open file");

    /* seek to offset */
    if (lseek64(fd, alignedOffset, SEEK_SET) == -1)
        ERR("ERROR: Unable to seek to file offset");

    /* read into buffer */
    if (read(fd, &oldBuffer, BYTE_BOUNDARY) <= 0)
        ERR("ERROR: Unable to read file offset");

    if (argc == 4) {
        /* write from buffer */
        /*   update buffer with new value */
        memcpy(newBuffer, oldBuffer, BYTE_BOUNDARY);
        newBuffer[bufferIndex] = (unsigned char)value;

        /*   seek to offset */
        if (lseek64(fd, alignedOffset, SEEK_SET) == -1)
            ERR("ERROR: Unable to seek to file offset");

        /*   write buffer */
        if (write(fd, &newBuffer, BYTE_BOUNDARY) != BYTE_BOUNDARY)
            ERR("ERROR: Unable to write to file offset");
    }

    /* print data */
    /*   print header  */
    if (argc == 3) {
        fprintf(stdout, "offset:               original value:   \n");
        fprintf(stdout, "------------------    ------------------\n");
    } else {
        fprintf(stdout,
            "offset:               original value:       new value:        \n");
        fprintf(stdout,
            "------------------    ------------------    ------------------\n");
    }

    /*   next hex, multiple bytes */
    fprintf(stdout, "0x%016llx    0x", alignedOffset);
    for (i = 0; i < BYTE_BOUNDARY; i++) {
        fprintf(stdout, "%02x", oldBuffer[i]);
    }
    if (argc == 4) {
        fprintf(stdout, "    0x");
        for (i = 0; i < BYTE_BOUNDARY; i++) {
            fprintf(stdout, "%02x", newBuffer[i]);
        }
    }
    fprintf(stdout, "\n");

    /*   finally decimal, single byte */
    fprintf(stdout, "%-16lld      %-3d (0x%02x)", offset,
            oldBuffer[bufferIndex], oldBuffer[bufferIndex]);
    if (argc == 4) {
        fprintf(stdout, "            %-3d (0x%02x)\n",
                newBuffer[bufferIndex], newBuffer[bufferIndex]);
    }
    fprintf(stdout, "\n");

    /* finished */
    fflush(stdout);
    close(fd);
    return(0);

} /* main() */
