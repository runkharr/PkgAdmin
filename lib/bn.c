/* bn.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Synopsis:
**    const char *basename = bn (path);
**
** Returns a pointer to the last (UNIX) path-element of 'path' ...
**
*/
#ifndef BN_C
#define BN_C

#include <string.h>

#include "separators.c"

static const char *bn (const char *filename)
{
    const char *res = filename - 1;
    while (*++res);
    while (res > filename && *--res != DIRSEP);
    if (*res == DIRSEP && res[1]) { ++res; }
    return res;
}

#endif /*BN_C*/
