/* fask.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Derived from `vfask()´, `fask()´ has a variable number of arguments ...
**
*/
#ifndef FASK_C
#define FASK_C

#include "lib/vfask.c"

static int fask (FILE *in, FILE *out, int defaultans, const char *prompt, ...)
{
    int res;
    va_list fpa;
    va_start (fpa, prompt);
    res = vfask (in, out, defaultans, prompt, fpa);
    va_end (fpa);
    return res;
}

#endif /*FASK_C*/
