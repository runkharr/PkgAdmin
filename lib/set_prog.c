/* set_prog.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Setting the program-name ('prog') from the program's command line arguments
** ...
**
*/
#ifndef SET_PROG_C
#define SET_PROG_C

#ifndef PROG
#define PROG "c_prog"
#endif

#include "lib/prog.c"

static void set_prog (int argc, char *argv[])
{
    if (argc < 1 || !argv || !argv[0]) {
        prog = PROG;
    } else {
	const char *p = argv[0];
	prog = p--; while (*++p);
	while (p > prog && *--p != '/');
	if (*p == '/') { prog = p + 1; }
    }
}

#endif /*SET_PROG_C*/
