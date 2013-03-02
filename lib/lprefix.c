/* lprefix.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Synopsis:
**   int isaprefix = is_lprefix (probe, string);
**
** Returns true if the string `probe´ is a prefix of `string´ (ignoring
** the letter case) ...
**
*/
#ifndef LPREFIX_C
#define LPREFIX_C

#include <ctype.h>

static int is_lprefix (const char *p, const char *s)
{
    int c;
    while ((c = tolower (*p++)) == tolower (*s++) && c);
    return !c;
}

#endif /*LPREFIX_C*/
