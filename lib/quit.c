/* quit.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Terminate a program with a given exit code, plus an (optional) error
** message.
**
*/
#ifndef QUIT_C
#define QUIT_C

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "separators.c"

#define EX_USAGE 64
#define EX_NOUSER 67
#define EX_UNAVAILABLE 69
#define EX_OSERR 71

extern const char *prog;

__attribute__((noreturn, format(printf, 2, 3)))
static void quit (int exc, const char *format, ...)
{
    if (format) {
	va_list args;
	fprintf (stderr, "%s: ", prog);
	va_start (args, format); vfprintf (stderr, format, args); va_end (args);
	fputs (EOL, stderr);
    }
    exit (exc);
}

#endif /*QUIT_C*/
