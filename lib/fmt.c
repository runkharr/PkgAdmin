/* fmt.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Print a string with `$<number>` and `$#` placeholders to the given output
** FILE, while replacing the placeholders either with the <number>th argument
** from the remaining argument list (for `$#`, an internal index is managed,
** starting from `0` to the number of arguments counted), or nothing if
** <number> reaches outside of the argument list or the internally managed
** index reeaches the end of the argument list. A `$$` or a single `$` at the
** end of the format string leads to `$` being printed itself.
**
*/
#ifndef FMT_C
#define FMT_C

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "parseint.c"

# ifdef __cplusplus
extern "C" {
# endif

static size_t fmt_strlen (const char *s)
{
    const char *p = s;
    --p; while (*++p);
    return (size_t) (p - s);
}

static int fmt_printvl (FILE *out, const char *fmt, va_list ap, int argcc)
{
    const char *args[argcc + 1], *p, *q;
    int next_arg = 0;
    size_t out_len = 0, cut_len;
    for (int ix = 0; ix < argcc; ++ix) {
	args[ix] = va_arg (ap, char *);
    }
    args[argcc] = NULL;
    p = fmt;
    while (*p) {
	q = p - 1; while (*++q && *q != '$');
	cut_len = (size_t) (q - p);
	fwrite (p, 1, cut_len, out);
	out_len += cut_len;
	if (! *q) { break; }
	p = q + 1;
	if (! *p || *p == '$') {
	    fputc ((*p ? *p++ : '$'), out); ++out_len;
	} else if (*p == '#') {
	    ++p;
	    if (next_arg < argcc) {
		const char *s = args[next_arg++];
		fputs (s, out); out_len += fmt_strlen (s);
	    }
	} else {
	    int ix;
	    if (parseint (p, &ix, &p, 0)) {
		fputc ('$', out); ++out_len;
	    } else if (ix > 0 && --ix < argcc) {
		const char *s = args[ix];
		fputs (s, out); out_len += fmt_strlen (s);
	    }
	}
    }
    return out_len;
}

static int fmt_printv (FILE *out, const char *fmt, va_list ap)
{
    int argcc = 0;
    va_list ap2;
    va_copy (ap2, ap);
    while (va_arg (ap2, char *)) { ++argcc; }
    va_end (ap2);

    return fmt_printvl (out, fmt, ap, argcc);
}

# define fmt_print(out, fmt, ...) \
    (_fmt_print((out), (fmt), ##__VA_ARGS__, NULL))
static int _fmt_print (FILE *out, const char *fmt, ...)
{
    size_t out_len = 0;
    va_list ap;
    va_start (ap, fmt);
    out_len = fmt_printv (out, fmt, ap);
    va_end (ap);
    return (int) out_len;
}

# ifdef __cplusplus
}
# endif

#endif /*FMT_C*/
