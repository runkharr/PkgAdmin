#ifndef STORE_PROGPATH_C
#define STORE_PROGPATH_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/mrmacs.c"
#include "lib/prog.c"
#include "lib/progpath.c"
#include "lib/x_strdup.c"

static void store_progpath (char *argv[])
{
    char *p = strrchr (argv[0], '/'), *p1;
    if (p) { ++p; } else { p = argv[0]; }
    cfree (prog);
    if (!(p1 = t_allocv (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", p, strerror (errno)); exit (1);
    }
    strcpy (p1, p); prog = p1;
    progpath = x_strdup (argv[0]);
}

#endif /*STORE_PROGPATH_C*/
