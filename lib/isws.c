/* isws.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Synopsis:
**    int flag = isws (char_value)
**
** Returns true if 'char_value' is either a '\t' or a ' ' ...
**
*/
#ifndef ISWS_C
#define ISWS_C

#include "gnu-inline.c"

static __inline__ int isws (int c)
{
    return (c == ' ') || (c == '\t');
}

#endif /*ISWS_C*/
