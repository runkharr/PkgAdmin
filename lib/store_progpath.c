#ifndef STORE_PROGPATH_C
#define STORE_PROGPATH_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/mrmacs.c"
#include "lib/vprogpath.c"
#include "lib/x_strdup.c"

static void store_progpath (char *argv[])
{
    char *p = strrchr (argv[0], '/');
    if (p) { ++p; } else { p = argv[0]; }
    cfree (prog); /*if (prog) { cfree (prog); }*/
    if (!(prog = t_alloc (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", p, strerror (errno)); exit (1);
    }
    strcpy (prog, p);
    progpath = x_strdup (argv[0]);
}

#endif /*STORE_PROGPATH_C*/
