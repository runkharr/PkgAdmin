/* vfask.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Function for interactively communicating to a user by asking a question
** - which must be answered (by the user) with either 'yes' ('ja') or 'no'
** ('nein') or any prefix of these possible answers. This function uses
** a 'va_list'-value as it's last argument, so it can be used by derived
** functions which use a variable number of arguments ...
**
*/
#ifndef VFASK_C
#define VFASK_C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>

#include "lib/cuteol.c"
#include "lib/lprefix.c"

static int vfask (FILE *in, FILE *out, int defaultans, const char *prompt,
		  va_list fpa)
{
    int answer, waseol;
    char ans[128], *p, *q;
    va_list fpa1;
    if (!isatty (fileno (in)) || !isatty (fileno (out))) {
	errno = EINVAL; return -1;
    }
    for (;;) {
	va_copy (fpa1, fpa); vfprintf (out, prompt, fpa); va_end (fpa1);
	if (defaultans > 0) {
	    fputs (" (Y/n)? ", out);
	} else if (defaultans == 0) {
	    fputs (" (y/N)? ", out);
	} else {
	    fputs (" (y/n)? ", out);
	}
	fflush (out);
	if (!fgets (ans, sizeof(ans), in)) {
	    fputs ("<EOF>\n", out); fflush (out); return -1;
	}
	waseol = cuteol (ans);
	p = ans; while (isblank (*p)) { ++p; }
	q = p + strlen (p);
	while (q > p && isblank (*--q)) { *q = '\0'; }
	if (q == p) {
	    answer = (defaultans > 0 ? +1 : (defaultans == 0 ? 0 : -1));
	} else if (is_lprefix (p, "yes") || is_lprefix (p, "ja")) {
	    answer = +1;
	} else if (is_lprefix (p, "no") || is_lprefix (p, "nein")) {
	    answer = 0;
	} else {
	    answer = -1;
	}
	/* Skip the remaining line (if it is longer than sizeof(ans) - 1
	** characters) ...
	*/
	while (!waseol) {
	    if (!fgets (ans, sizeof(ans), in)) { break; }
	}
	if (answer >= 0) { break; }
	fputs ("*** Wrong answer. Please answer only with (a prefix of)\n"
	       "    'yes' ('ja') or 'no' ('nein')!\n", out);
    }
    return answer;
}

#endif /*VFASK_C*/
