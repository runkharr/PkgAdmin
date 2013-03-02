/* set_prog.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Setting the program-name (`prog´) from the program's command line arguments
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
    if (argc < 1) {
        prog = PROG;
    } else if (!argv || !argv[0]) {
        prog = PROG;
    } else if ((prog = strrchr (argv[0], '/'))) {
        ++prog;
    } else {
        prog = argv[0];
    }
}

#endif /*SET_PROG_C*/
