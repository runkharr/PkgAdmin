/* empty.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Check if a string is either `NULL` or doesn't contain any chars besides
** blanks and/or tabs.
**
*/
#ifndef EMPTY_C
#define EMPTY_C

#include <stdbool.h>

#include "isws.c"

static bool is_empty (const char *s)
{
    bool res = true;
    if (s) {
	while (isws (*s)) { ++s; }
	if (*s) { res = false; }
    }
    return res;
}

#endif /*EMPTY_C*/
