/* lib/cwd.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Returns the current working directory. Because the working directory is
** never changed in any of the programs in `PkgAdmin´, this function allocates
** the memory required for the result only once (storing it in a static local
** variable) and returns the value as `const char *´ ...
**
*/
#ifndef CWD_C
#define CWD_C

#include <stdlib.h>

#include "lib/mrmacs.c"
#include "lib/check_ptr.c"

static const char *cwd (void)
{
    static size_t wdsz = 0;
    static char *wd = NULL;
    /*if (wd) { return wd; }*/
    if (wdsz == 0) {
	wdsz = 1024;
	check_ptr ("cwd", wd = t_allocv (char, wdsz));
    }
    for (;;) {
	unlessnull (getcwd (wd, wdsz)) { return wd; }
	wdsz += 1024;
	check_ptr ("cwd", wd = realloc (wd, wdsz));
    }
}

#endif /*CWD_C*/
