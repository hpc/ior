/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
\******************************************************************************/

#ifndef _PARSE_OPTIONS_H
#define _PARSE_OPTIONS_H

#include "ior.h"

IOR_test_t *ParseCommandLine(int argc, char **argv, MPI_Comm com);

#endif  /* !_PARSE_OPTIONS_H */
