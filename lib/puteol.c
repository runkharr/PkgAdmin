/* puteol.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Simply write an end of line character(-sequence) to a file ...
**
*/
#ifndef PUTEOL_C
#define PUTEOL_C

#include <stdio.h>

#define EOLSTR "\n"

static const char *eolstr = EOLSTR;

static int puteol (FILE *out)
{
    int rc = fputs (eolstr, out);
    fflush (out);
    return rc;
}

#endif /*PUTEOL_C*/
