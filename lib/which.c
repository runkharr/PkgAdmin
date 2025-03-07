/* which.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Search for a program in a PATH (either given by the environment variable
** `PATH` or a constant defined within this file) and return the complete
** pathname of this program.
**
*/
#ifndef WHICH_C
#define WHICH_C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
//#include <sys/types.h>

#include "empty.c"
#include "sfmt.c"
#include "separators.c"

#define ROOT_PATH_TMPL  "$1/bin:/bin:/sbin:/usr/bin:/usr/sbin:" \
			"/usr/local/bin:/usr/local/sbin"
#define USER_PATH_TMPL  "$1/bin:/bin:/usr/bin:/usr/local/bin"

static const char *which_path = NULL;

static char which_pthtpl[256];

const char *which_getpath (void)
{
    if (! which_path) {
	char *path = getenv ("PATH");
	if (! is_empty (path)) {
	    which_path = path;
	} else {
	    uid_t uid = geteuid();
	    struct passwd *pw = getpwuid (uid);
	    if (! pw) {
		errno = EINVAL;
	    } else {
		char *p;
		size_t which_path_len;
		if (uid == 0) {
		    strcpy (which_pthtpl, ROOT_PATH_TMPL);
		} else {
		    strcpy (which_pthtpl, USER_PATH_TMPL);
		}
		p = which_pthtpl; --p;
		while (*++p) {
		    if (*p == '/') {
			*p = DIRSEP;
		    } else if (*p == ':') {
			*p = PATHSEP;
		    }
		}
		which_path_len = strlen (pw->pw_dir) + strlen (which_pthtpl);
		if ((p = (char *) malloc (which_path_len + 1))) {
		    sfmt_print (p, which_path_len + 1,
				which_pthtpl, pw->pw_dir);
		    which_path = p;
		}
	    }
	}
    }
    return which_path;
}


static const char *which (const char *cmd)
{
    static char *rv = NULL;
    struct stat sb;
    if (strchr (cmd, '/')) {
	if (access (cmd, X_OK) < 0) { return NULL; }
	if (stat (cmd, &sb)) { return NULL; }
	if (! S_ISREG(sb.st_mode)) { return NULL; }
	rv = strdup (cmd);
    } else {
	int ec;
	const char *PATH = which_getpath();
	char *buf = NULL, *bp; size_t bufsz = 0;
	if (PATH) {
	    const char *p, *q;
	    p = PATH;
	    while ((q = strchr (p, PATHSEP)) || *(q = p + strlen (p))) {
		size_t pathname_len = strlen (cmd) + (size_t) (q - p) + 1;
		if (pathname_len >= bufsz) {
		    char *tmp;
		    bufsz = pathname_len + 1 + 16; bufsz -= bufsz % 16;
		    if (! (tmp = (char *) realloc (buf, bufsz))) {
			return NULL;
		    }
		    buf = tmp;
		}
		bp = stpncpy (buf, p, (size_t) (q - p));
		*bp++ = DIRSEP;
		bp = stpcpy (bp, cmd);
		if (access (cmd, X_OK) == 0 &&
		    stat (cmd, &sb) == 0 &&
		    S_ISREG (sb.st_mode)) {
		    rv = strdup (buf); break;
		}
		p = q + (*q ? 1 : 0);
	    }
	    ec = errno; free (buf); errno = ec;
	}
    }
    return rv;
}

#endif /*WHICH_C*/
