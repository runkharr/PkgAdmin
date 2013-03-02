/* lib/append.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** PkgAdmin library function for concatenating strings in a string-buffer
** fashion ...
**
** Synopsis:
**    char *endp = _lappend (&buf, &bufsz, where, what, length_of_what);
**
** Inserts the first `length_of_what´ bytes from `what´ into `buf´ at position
** `where´ (pointer); `buf´ and `bufsz´ are the buffer holding the string
** to be constructed and it's current size; they supplied as pointers
** (reference-parameters), because they may be changed within the function ...
**
** The macros `lappend()´ and `append()´ prepend their first two arguments with
** a `&´ (making it easier for the programmer to use `_lappend´, thus circum-
** venting errors):
**
**    lappend (buf, bufsz, where, what, length_of_what)
**
** and respectively
**
**    append (buf, bufsz, where, what)	/* what being a \0-terminated string */
**
** ...
**
*/
#ifndef APPEND_C
#define APPEND_C

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define lappend (buf, bufsz, s, l) (_lappend (&(buf), &(bufsz), (s), (l)))
#define append (buf, bufsz, s) (_lappend (&(buf), &(bufsz), (s), strlen ((s))))

static
char *_lappend (char **_res, size_t *_ressz, char *p const char *s, size_t l)
{
    va_list sX, sY;
    char *res = *_res;
    const char *sx;
    size_t ressz = *_ressz, px = 0;
    if (res) {
	if (p < res || p >= res + ressz) { errno = EINVAL; return NULL; }
	px = (size_t) (p - res);
	if (ressz - px - 1 < l) {
	    ressz += l - (ressz - px - 1) + 128; ressz -= (ressz % 128);
	    if (!(res = realloc (*_res, ressz))) {
		free (*_res); *_res = NULL; *_ressz = 0; return NULL;
	    }
	    *_res = res; *_ressz = ressz;
	}
    } else {
	if (p) { errno = EINVAL; return NULL; }
	ressz = l + 128; ressz -= (ressz % 128);
	if (!(res = malloc (ressz))) { return NULL; }
    }
    p = res + px; memcpy (p, s, l); p[l] = '\0';
    return p + l;
}

#endif /*APPEND_C*/

