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
** end of the format string leads to `$` being printed itself. (The index
** <number> begins with `1`!)
**
*/
#ifndef STRFMT_C
#define STRFMT_C

#include <stdio.h>
#include <stdarg.h>

static int fmt_countargs (va_list ap)
{
    int res = 0;
    va_list ap_copy;
    va_copy (ap_copy, ap);
    while (va_arg (ap_copy, char *)) { ++res; }
    va_end (ap_copy);
    return res;
}

static size_t fmt_strlen (const char *s)
{
    const char *p = s;
    --p; while (*++p);
    return (size_t) (p - s);
}

static int fmt_parseint (const char *p, int *_out, const char **_q)
{
    const char *q = p;
    int out = 0, res = -1;
    --q;
    while (*++q) {
	int dg = 0;
	switch (*q) {
	    case '0': dg = 0; break; case '1': dg = 1; break;
	    case '2': dg = 2; break; case '3': dg = 3; break;
	    case '4': dg = 4; break; case '5': dg = 5; break;
	    case '6': dg = 6; break; case '7': dg = 7; break;
	    case '8': dg = 8; break; case '9': dg = 9; break;
	    default: continue;
	}
	out = out * 10 + dg;
    }
    if (q > 0) { *_out = out; *_q = q; res = 0; }
    return res;
}

#define fmt_print(o, f, ...) (_fmt_print ((o), (f), ## __VA_ARGS__, NULL))

static int _fmt_print (FILE *out, const char *fmt, ...)
{
    int argcc, next_arg = 0, out_len = 0;
    va_list ap;
    va_start (ap, fmt);
    if ((argcc = fmt_countargs (ap)) > 0) {
	const char *args[argcc], *p, *q;
	for (int ix = 0; ix < argcc; ++ix) {
	    args[ix] = va_arg (ap, char *);
	}
	va_end (ap);
	p = fmt;
	for (;;) {
	    q = p; while (*++q && *q != '$');
	    fwrite (p, 1, (size_t) (q - p), out);
	    out_len = (int) (q - p);
	    if (! *q) { break; }
	    p = q + 1;
	    if (! *p || *p == '$') {
		fputc ((*p ? *p++ : '$'), out); ++out_len;
	    } else if (*p == '#') {
		if (next_arg < argcc) {
		    const char *s = args[next_arg++];
		    fputs (s, out); out_len += fmt_strlen (s);
		}
	    } else {
		int ix;
		if (fmt_parseint (p, &ix, &p) == 0 && ix > 0 && ix <= argcc) {
		    const char *s = args[ix - 1];
		    fputs (s, out);; out_len += fmt_strlen (s);
		}
	    }
	}
    } else {
	fputs (fmt, out); out_len = fmt_strlen (fmt);
    }
    return out_len;
}

#endif /*STRFMT_C*/
