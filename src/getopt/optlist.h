/***************************************************************************
*                       Command Line Option Parser
*
*   File    : optlist.h
*   Purpose : Header for getopt style command line option parsing
*   Author  : Michael Dipperstein
*   Date    : August 1, 2007
*
****************************************************************************
*
* OptList: A command line option parsing library
* Copyright (C) 2007, 20014 by
* Michael Dipperstein (mdipper@alumni.engr.ucsb.edu)
*
* This file is part of the OptList library.
*
* OptList is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or (at
* your option) any later version.
*
* OptList is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************/
#ifndef OPTLIST_H
#define OPTLIST_H

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/

/***************************************************************************
*                                 MACROS
***************************************************************************/

/***************************************************************************
*                                CONSTANTS
***************************************************************************/
#define    OL_NOINDEX    -1     /* this option has no arguement */

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/
typedef struct option_t
{
    char option;                /* the current character option character */
    char *argument;             /* pointer to arguments for this option */
    int argIndex;               /* index into argv[] containing the argument */
    struct option_t *next;      /* the next option in the linked list */
} option_t;

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/

/* returns a linked list of options and arguments similar to getopt() */
option_t *GetOptList(int argc, char *const argv[], char *const options);

/* frees the linked list of option_t returned by GetOptList */
void FreeOptList(option_t *list);

/* return a pointer to file name in a full path.  useful for argv[0] */
char *FindFileName(const char *const fullPath);

#endif  /* ndef OPTLIST_H */
