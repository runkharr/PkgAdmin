/* bgetline.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Read a line from a file. The buffer for this line is supplied as a pair of
** (reference-)arguments '_line' and '_linesz' and will eventually resized
** during the retrieval of the line.
** Result is either '-2' (error) or '-1' (no content read due to EOF)
** or the length of the line read (without the EOL-character(s)) ...
*/
#ifndef BGETLINE_C
#define BGETLINE_C

#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
# define BGETLINE_EXT_DEFINED
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef BGETLINE_EXT_DEFINED
# undef _DEFAULT_SOURCE
# undef BGETLINE_EXT_DEFINED
#endif

#include "mrmacs.c"
#include "cuteol.c"

#define LSZ_INITIAL 1024
#define LSZ_INCREASE 1024

#define bgetline(f, line, linesz) (_bgetline ((f), &(line), &(linesz)))

static ssize_t _bgetline (FILE *in, char **_line, size_t *_linesz)
{
    char *line = *_line, *p, *rr;
    size_t linesz = *_linesz, len;
    if (!line) {
	linesz = LSZ_INITIAL; line = t_allocv (char, linesz);
	if (!line) { *_linesz = 0; return -2; }
    }
    p = line; len = linesz;
    while ((rr = fgets (p, len, in))) {
	if (cuteol (p)) { break; }
	p += strlen (p);
	len = (size_t) (p - line);
	if (len + 1 >= linesz) {
	    linesz += LSZ_INCREASE;
	    if (!(p = t_realloc (char, line, linesz))) { return -2; }
	    /**_line = p; *_linesz = linesz;*/
	    line = p; p += len;
	}
	len = linesz - len;
    }
    if (!rr && p == line) {
	if (! ferror (in)) { errno = 0; }
	return -1;
    }
/*    if (p == line && *p == '\0') { return -1; }*/
    *_line = line; *_linesz = linesz;
    return (ssize_t) (p - line);
}

#endif /*BGETLINE_C*/
