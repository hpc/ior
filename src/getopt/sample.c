/***************************************************************************
*                           OptList Usage Sample
*
*   File    : sample.c
*   Purpose : Demonstrates usage of optlist library.
*   Author  : Michael Dipperstein
*   Date    : July 23, 2004
*
****************************************************************************
*
* Sample: A optlist library sample usage program
* Copyright (C) 2007, 2014 by
* Michael Dipperstein (mdipper@alumni.engr.ucsb.edu)
*
* This file is part of the optlist library.
*
* The optlist library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* The optlist library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "optlist.h"

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : main
*   Description: This is the main function for this program, it calls
*                optlist to parse the command line input displays the
*                results of the parsing.
*   Parameters : argc - number of parameters
*                argv - parameter list
*   Effects    : parses command line parameters
*   Returned   : EXIT_SUCCESS for success, otherwise EXIT_FAILURE.
****************************************************************************/
int main(int argc, char *argv[])
{
    option_t *optList, *thisOpt;

    /* get list of command line options and their arguments */
    optList = NULL;
    optList = GetOptList(argc, argv, "a:bcd:ef?");

    /* display results of parsing */
    while (optList != NULL)
    {
        thisOpt = optList;
        optList = optList->next;

        if ('?' == thisOpt->option)
        {
            printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
            printf("options:\n");
            printf("  -a : option excepting argument.\n");
            printf("  -b : option without arguments.\n");
            printf("  -c : option without arguments.\n");
            printf("  -d : option excepting argument.\n");
            printf("  -e : option without arguments.\n");
            printf("  -f : option without arguments.\n");
            printf("  -? : print out command line options.\n\n");

            FreeOptList(thisOpt);   /* free the rest of the list */
            return EXIT_SUCCESS;
        }

        printf("found option %c\n", thisOpt->option);

        if (thisOpt->argument != NULL)
        {
            printf("\tfound argument %s", thisOpt->argument);
            printf(" at index %d\n", thisOpt->argIndex);
        }
        else
        {
            printf("\tno argument for this option\n");
        }

        free(thisOpt);    /* done with this item, free it */
    }

    return EXIT_SUCCESS;
}
