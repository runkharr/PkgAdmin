/* lib/which1.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** One possible implementation of `which()´, which searches in the PATH for an
** executable with the name which were supplied as argument to `which()´ ...
**
*/
#ifndef WHICH1_C
#define WHICH1_C

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "lib/mrmacs.c"
#include "lib/check_ptr.c"
#include "lib/cwd.c"

static const char *which (const char *cmd)
{
    static char *rv = NULL;
    static size_t rvsz = 0;
    static char *PATH = NULL;
    char *p, *q;
    size_t pl, cmdsz = strlen (cmd);
    struct stat sb;
    if (strchr (cmd, '/')) {
	if (stat (cmd, &sb)) { return NULL; }
	if (! S_ISREG(sb.st_mode)) { return NULL; }
	if (access (cmd, X_OK) < 0) { return NULL; }
	if ((rv = malloc (strlen (cmd) + 1))) { strcpy (rv, cmd); }
	return rv;
    }
    if (!PATH) {
	PATH = getenv ("PATH");
	if (!PATH) {
	    if (geteuid () == 0) {
		PATH = "/root/bin:/usr/bin:/usr/sbin:/bin:/sbin:"
		       "/usr/local/sbin";
	    } else {
		p = (char *) cwd ();
		q = t_allocv (char, strlen (p) + strlen ("/bin") + 1);
		check_ptr ("which", q);
		sprintf (q, "%s/bin", p);
		p = "/usr/local/bin:/usr/bin:/bin";
		PATH = t_allocv (char, strlen (q) + strlen (p) + 2);
		check_ptr ("which", PATH);
		sprintf (PATH, "%s:%s", q, p);
		cfree (q);
	    }
	}
    }
    p = PATH;
    while (*p != '\0') {
	q = strchr (p, ':'); if (!q) { q = &p[strlen (p)]; }
	if (p != q) {
	    pl = (q - p) + cmdsz + 2;
	    if (pl > rvsz) { check_ptr ("which", rv = realloc (rv, pl)); }
	    sprintf (rv, "%.*s/%s", (int) (q - p), p, cmd);
	    if (stat (rv, &sb)) { goto NEXT; }
	    if (! S_ISREG(sb.st_mode)) { goto NEXT; }
	    if (access (rv, X_OK) == 0) { return rv; }
	}
NEXT:
	p = q; if (*p == ':') { ++p; }
    }
    return NULL;
}

#endif /*WHICH1_C*/
