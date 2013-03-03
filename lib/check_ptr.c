/* lib/check_ptr.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Checks a pointer for being NULL (undefined) and terminates in this case;
** the first argument is displayed as the function where the problem occurred
** ...
**
*/
#ifndef CHECK_PTR_C
#define CHECK_PTR_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lib/prog.c"
#include "lib/mrmacs.c"

static void check_ptr (const char *where, void *ptr)
{
    ifnull (ptr) {
	fprintf (stderr, "%s: (in %s) %s\n", prog, where, strerror (errno));
	exit (71);
    }
}

#endif /*CHECK_PTR_C*/
