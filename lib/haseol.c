/* haseol.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Simple `end of line´-detection in a string (like in `lib/cuteol.c´, but
** without the removal); detects all three existing `end of line´-schemes ...
**
*/
#ifndef HASEOL_C
#define HASEOL_C

static int haseol (const char *s)
{
    const char *p = s - 1;
    while (*++p);
    if (p == s) { return 0; }
    if (*--p == '\r') { return 2; }
    if (*p == '\n') {
	if (p > s && *--p == '\r') { return 3; }
	return 1;
    }
    return 0;
}

#endif /*HASEOL_C*/
