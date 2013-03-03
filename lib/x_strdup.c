/* lib/x_strdup.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Duplicate a string but terminate with an error message if the memory
** allocation fails ...
**
*/
#ifndef X_STRDUP_C
#define X_STRDUP_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lib/mrmacs.c"
#include "lib/prog.c"
#include "lib/sdup.c"

static char *x_strdup (const char *s)
{
    char *res = sdup (s);
    if (!res) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }
    return res;
}

#endif /*X_STRDUP_C*/
