/* pbCopy.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Copy a string into another and return the end-position of the destination
** string - alot alike 'stpcpy()'; required, because 'stpcpy()' is not always
** available (it's a GNU-extension) ...
**
*/
#ifndef PBCOPY_C
#define PBCOPY_C

#include "lib/gnu-inline.c"

static __inline__ char *pbCopy (char *d, const char *s)
{
    while ((*d++ = *s++));
    return --d;
}

#endif /*PBCOPY_C*/
