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
#define bconcv(res, ressz, args) (_bconc (&(res), &(ressz), (args)))

static char *_bconcv (char **_res, size_t *_ressz, va_list sX)
{
    int sc = 0;
    va_list sY;
    char *res = *_res, *p;
    const char *sx;
    size_t ressz = *_ressz, reslen = 1;
    va_copy (sY, sX);
    while ((sx = va_arg (sY, const char *))) {
	reslen += strlen (sx); ++sc;
    }
    va_end (sY);
    if (!res) { ressz = 0; }
    if (reslen > *_ressz) {
	ressz = reslen + 127; ressz += ressz % 128;
	if (!(res = t_realloc (char, *_res, ressz))) {
	    /* Leave *_res UNCHANGED!
	    ** free (*_res); *_res = NULL; *_ressz = 0; return res;
	    */
	    return res;
	}
    }
    p = res;
    while ((sx = va_arg (sX, const char *))) {
	p = pbCopy (p, sx);
    }
    *p = '\0';
    /* Ensure that the in/out-arguments remain unchanged on errors by assigning
    ** new values as last action before a return!
    */
    *_res = res; *_ressz = ressz;
    return res;
}

static char *_bconc (char **_res, size_t *_ressz, ...)
{
    va_list sX;
    char *res;
    va_start (sX, _ressz);
    res = _bconcv (_res, _ressz, sX);
    va_end (sX);
    return res;
}

#ifdef __GNUC__
# define conc(arg1, ...) \
    ({ char *res = NULL, *buf; size_t bufsz = 0; \
       res = _bconc (&buf, &bufsz, (arg1), ##__VA_ARGS__, NULL); \
       res; })
#else
# warning 'conc()' is not available for non-GNU compilers
#endif

#endif /*BCONC_C*/
