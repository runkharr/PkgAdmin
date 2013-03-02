/* nows.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Synopsis:
**    int flag = nows (char_value);
**
** Returns true if `char_value´ is neither '\0' nor '\t' nor ' ' ...
**
*/
#ifndef NOWS_C
#define NOWS_C

#include "lib/gnu-inline.c"

static int nows (char c)
{
    return (c != '\0' && c != ' ' && c != '\t');
}

#endif /*NOWS_C*/
