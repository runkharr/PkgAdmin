/* which2.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Second implementation of 'which' ...
**
*/
#ifndef WHICH2_C
#define WHICH2_C

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "lib/mrmacs.c"
#include "lib/regfile.c"
#include "lib/append.c"
#include "lib/sdup.c"

static char *which (const char *file)
{
    if (strchr (file, '/')) {
	if (is_regfile (file) <= 0 || access (file, X_OK) < 0) { return NULL; }
	return sdup (file);
    } else {
	char *buf = NULL, *bp, *p, *q, *PATH, *res;
	size_t bufsz = 0;
	if (!(PATH = getenv ("PATH"))) { errno = ENOENT; return NULL; }
	p = PATH;
	while ((q = strchr (p, ':'))) {
	    if (!(bp = lappend (buf, bufsz, NULL, p, (q - p)))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, "/"))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, file))) { return NULL; }
	    if (is_regfile (buf) > 0 && access (file, X_OK) == 0) {
		res = sdup (buf); cfree (buf); return res;
	    }
	    p = q + 1;
	}
	if (*p) {
	    if (!(bp = append (buf, bufsz, NULL, p))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, "/"))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, file))) { return NULL; }
	    if (is_regfile (buf) > 0 && access (file, X_OK) == 0) {
		res = sdup (buf); cfree (buf); return res;
	    }
	}
	cfree (buf); errno = ENOENT;
	return NULL;
    }
}

#endif /*WHICH2_C*/
