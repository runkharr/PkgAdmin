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

#include "lib/mrmacs.c"
#include "lib/pbCopy.c"

#define bconc(res, ressz, ...) (_bconc (&(res), &(ressz), __VA_ARGS__, NULL))

static char *_bconc (char **_res, size_t *_ressz, ...)
{
    int sc = 0;
    va_list sX, sY;
    char *res = *_res, *p;
    const char *sx;
    size_t ressz = *_ressz, reslen = 1;
    va_start (sX, _ressz);
    va_copy (sY, sX);
    while ((sx = va_arg (sX, const char *))) {
        reslen += strlen (sx); ++sc;
    }
    va_end (sX);
    if (!res) { ressz = 0; }
    if (reslen > *_ressz) {
	ressz = reslen + 128; ressz -= (ressz% 128);
        if (!(res = t_realloc (char, *_res, ressz))) {
	    free (*_res); *_res = NULL; *_ressz = 0; return res;
	}
        *_res = res; *_ressz = ressz;
    }
    p = res;
    while ((sx = va_arg (sY, const char *))) {
        p = pbCopy (p, sx);
    }
    *p = '\0';
    return res;
}

#endif /*BCONC_C*/
