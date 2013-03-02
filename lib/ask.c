/* ask.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Derived from `vfask()´, `ask()´ has a variable number of arguments and uses
** stdin/stdout for it's communication with the user ...
**
*/
#ifndef ASK_C
#define ASK_C

#include "lib/vfask.c"

static int ask (int defaultans, const char *prompt, ...)
{
    int res;
    va_list fpa;
    va_start (fpa, prompt);
    res = vfask (stdin, stdout, defaultans, prompt, fpa);
    va_end (fpa);
    return res;
}

#endif /*ASK_C*/
