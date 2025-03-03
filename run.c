/* run.c
**
** $Id$
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2010, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Start a (binary) program with another name.
**
** Synopsis:
**
**   run program as newname arg...
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "lib/cmdvec.c"
#include "lib/quit.c"
#include "lib/separators.c"
#include "lib/which.c"

#define streq(x, y) (strcmp ((x), (y)) == 0)

#define PROG "run"

#define ERRSTR (strerror (errno))

const char *prog;

static const char *bn (const char *path)
{
    const char *res = strrchr (path, DIRSEP);

    if (res) { ++res; } else { res = path; }
    return res;
}

int main (int argc, char *argv[])
{
    extern char **environ;
    int is_as, argx, nargc = 0;
    char *newname = NULL, **nargv = NULL;
    const char *cmd = NULL, *p;

    prog = bn (*argv);

    if (argc < 2) {
	printf ("Usage: %s program [as new-name] arg...\n", prog);
	exit (0);
    }
    is_as = (argc > 2) && streq (argv[2], "as");
    if (is_as && argc < 4) {
	quit (EX_USAGE, "Missing command after `as` keyword.");
    }
    argx = (is_as ? 3 : 1);
    nargc = argc - argx;
    if (! (nargv = gen_cmdvec (&argv[argx], nargc))) {
	quit (EX_OSERR, "gen_cmdvec() - %s", ERRSTR);
    }

    if (! (cmd = which (argv[1]))) {
	quit (EX_UNAVAILABLE, "which() - %s", ERRSTR);
    }
    if (is_as) {
	if (! strchr (nargv[0], DIRSEP)) {
	    if (!(p = strrchr (cmd, DIRSEP))) {
		size_t nnsz = (size_t) (p - cmd) + strlen (nargv[0]) + 2;
		if (!(newname = (char *) malloc (sizeof(char) * nnsz))) {
		    quit (EX_OSERR, "main() - %s", ERRSTR);
		}
		snprintf (newname, nnsz,
			  "%.*s%c%s", (int) (p - cmd), cmd, DIRSEP, nargv[0]);
		nargv[0] = newname;
	    }
	}
    }
    execve (cmd, nargv, environ);
    quit (EX_UNAVAILABLE, "%s - %s", cmd, ERRSTR);
    return 0;
}
