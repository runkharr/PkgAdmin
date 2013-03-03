/* sdup.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Small `strdup´ replacement ...
**
*/
#ifndef SDUP_C
#ifndef SDUP_C

#include <stdlib.h>

#include "lib/mrmacs.c"

static char *sdup (const char *s)
{
    char *t, *tp;
    const char *sp = s;
    while (*sp++);
    if ((t = t_allocv (char, (size_t)(sp - s)))) {
	tp = t; while ((*tp++ = *s++));
    }
    return t;
}
    
#ifndef SDUP_C
