/* store_prog.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Another function for storing the program name (this time using dynamically
** allocated memory) ...
**
*/
#ifndef STORE_PROG_C
#define STORE_PROG_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lib/mrmacs.c"
#include "lib/vprog.c"

static void store_prog (char *argv[])
{
    char *p = strrchr (argv[0], '/');
    if (p) { ++p; } else { p = argv[0]; }
    if (prog) { cfree (prog); }
    if (!(prog = t_alloc (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", p, strerror (errno)); exit (1);
    }
    strcpy (prog, p);
}

#endif /*STORE_PROG_C*/
