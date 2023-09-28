/* printarg.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2023, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Small utility for writing an argument in shell-specific manner to
** an output `FILE *`.
**
*/
#ifndef PRINTARG_C
#define PRINTARG_C

static __inline__ void
bflush (char *buf, char *bp, FILE *out)
{
    fwrite (buf, 1, (size_t) (bp - buf), out);
}

static __inline__ char *
bput (char *buf, char *eb, char *bp, char ch, FILE *out)
{
    if (bp >= eb) { bflush (buf, bp, out); bp = buf; }
    *bp++ = ch;
    return bp;
}

static void
print_arg (const char *arg, FILE *out)
{
    char buf[128], *eb, *p;
    const char *argval = strchr (arg, '=');
    const char *specials = "\t \\$<>\"\'!`;#" ;
    fputs (" ", out);
    if (argval) {
	++argval; fprintf (out, "%.*s", (int) (argval - arg), arg);
    } else {
	argval = arg;
    }
    p = strpbrk (argval, specials);
    if (! p) { fputs (argval, out); return; }
    eb = buf + sizeof(buf);
    p = buf; fputs ("\"", out);
    while (*argval) {
	if (*argval == '\t' || *argval == ' ') {
	    p = bput (buf, eb, p, *argval++, out);
	} else if (*argval == '!') {
	    p = bput (buf, eb, p, '"', out);
	    p = bput (buf, eb, p, '\\', out);
	    p = bput (buf, eb, p, *argval++, out);
	    p = bput (buf, eb, p, '"', out);
	} else if (strchr (specials, *argval)) {
	    p = bput (buf, eb, p, '\\', out);
	}
	p = bput (buf, eb, p, *argval++, out);
    }
    if (p > buf) { bflush (buf, p, out); }
    fputs ("\"", out); fflush (out);
}

#endif /*PRINTARG_C*/
