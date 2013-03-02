/* bconc.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Concatenates a number of string into a dynamically allocated buffer ...
**
*/
#ifndef BCONC_C
#define BCONC_C

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "lib/pbCopy.c"

static char *bconc (char **_res, size_t *_ressz, const char *s0, ...)
{
    va_list sX, sY;
    char *res = *_res, *p;
    const char *sx;
    size_t ressz = *_ressz;
    if (!s0) { return res; }
    ressz = 1 + strlen (s0);
    va_start (sX, s0);
    va_copy (sY, sX);
    while ((sx = va_arg (sX, const char *))) {
        ressz += strlen (sx);
    }
    va_end (sX);
    if (ressz > *_ressz) {
        if (!(res = realloc (*_res, ressz))) { return res; }
        *_res = res; *_ressz = ressz;
    }
    p = res;
    p = pbCopy (p, s0);
    while ((sx = va_arg (sY, const char *))) {
        p = pbCopy (p, sx);
    }
    return res;
}

#endif /*BCONC_C*/
