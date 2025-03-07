/* sfmt.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Write a string with `$<number>` and `$#` placeholders to the given buffer
** `buf` of size `bufsz`, while replacing the placeholders either with the
** <number>th argument from the remaining argument list (for `$#`, an internal
** index is managed, starting from `0` to the number of arguments counted), or
** nothing if <number> reaches outside of the argument list or the internally
** managed index reeaches the end of the argument list. A `$$` or a single `$`
** at the end of the format string leads to `$` being printed itself.
**
*/
#ifndef SFMT_C
#define SFMT_C

#include <stddef.h>
#include <stdarg.h>
#include <errno.h>

#include "parseint.c"

# ifdef __cplusplus
extern "C" {
# endif

static int sfmt_printvl (char *buf, size_t bufsz, const char *fmt,
			 va_list ap, int argcc)
{
    const char *args[argcc + 1], *p, *q;
    char dfbf[1024];
    char *be = (buf ?: dfbf) + (buf ? bufsz : sizeof(dfbf)) - 1, *r;
    int next_arg = 0;
    size_t out_len = 0;
    for (int ix = 0; ix < argcc; ++ix) {
	args[ix] = va_arg (ap, char *);
    }
    args[argcc] = NULL;
    p = fmt; r = buf;
    while (*p) {
	q = p - 1;
	while (*++q && *q != '$') {
	    ++out_len; if (r < be) { *r++ = *q; }
	}
	if (! *q) { break; }
	p = q + 1;
	if (! *p || *p == '$') {
	    if (r < be) { *r++ = (*p ? *p++ : '$'); }
	    ++out_len;
	} else if (*p == '#') {
	    ++p;
	    if (next_arg < argcc) {
		const char *s = args[next_arg++] - 1;
		while (*++s) {
		    ++out_len; if (r < be) { *r++ = *s; }
		}
	    }
	} else {
	    int ix;
	    if (parseint (p, &ix, &p, 0)) {
		++out_len; if (r < be) { *r++ = '$'; }
	    } else if (ix > 0 && --ix < argcc) {
		const char *s = args[ix] - 1;
		while (*++s) {
		    ++out_len; if (r < be) { *r++ = *s; }
		}
	    }
	}
    }
    *r = '\0';
    return out_len;
}

static int sfmt_printv (char *buf, size_t bufsz, const char *fmt, va_list ap)
{
    int argcc = 0;
    va_list ap2;
    va_copy (ap2, ap);
    while (va_arg (ap2, char *)) { ++argcc; }
    va_end (ap2);

    return sfmt_printvl (buf, bufsz, fmt, ap, argcc);
}

# define sfmt_print(buf, bufsz, fmt, ...) \
    (_sfmt_print ((buf), (bufsz), (fmt), ##__VA_ARGS__, NULL))
static int _sfmt_print (char *buf, size_t bufsz, const char *fmt, ...)
{
    size_t out_len = 0;
    va_list ap;
    va_start (ap, fmt);
    out_len = sfmt_printv (buf, bufsz, fmt, ap);
    va_end (ap);
    return (int) out_len;
}

# ifdef __cplusplus
}
# endif

#endif /*SFMT_C*/
