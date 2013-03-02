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
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#define EX_USAGE 64
#define EX_OSERR 71

#define ifnull(x) if ((void *)(x) == NULL)
#define unlessnull(x) if ((void *)(x) != NULL)
#define isNULL(x) ((void *)(x) == NULL)
#define noNULL(x) ((void *)(x) != NULL)
#define t_alloc(t) ((t *) malloc (sizeof (t)))
#define t_allocv(t, n) ((t *) malloc ((n) * sizeof (t)))
#define t_allocp(t, n) ((t *) malloc (sizeof (t) + (n)))

static char *prog = NULL;

static void report (int exitcode, int errcode, const char *format, ...)
{
    va_list ap;
    fprintf (stderr, "%s: ", prog);
    va_start (ap, format);
    vfprintf (stderr, format, ap);
    va_end (ap);
    if (errno != 0) { fprintf (stderr, " %s", strerror (errno)); }
    fputs ("\n", stderr); fflush (stderr);
    if (exitcode > 0) { exit (exitcode); }
}

static void check_ptr (const char *where, void *ptr)
{
    ifnull (ptr) { report (EX_OSERR, errno, "(in %s)", where); }
}

#include "lib/cwd.c"

#include "lib/which1.c"

int main (int argc, char *argv[])
{
    extern char **environ;
    int ix, jx, is_as, nargc = 0;
    char *p, *newname = NULL, **nargv = NULL;
    const char *cmd = NULL;
    p = strrchr (argv[0], '/'); if (p) { ++p; } else { p = argv[0]; }
    prog = p;
    if (argc < 2) {
	printf ("Usage: %s program [as new-name] arg...\n", prog);
	exit (0);
    }
    if (argc < 2) {
	report (EX_USAGE, 0, "missing argument(s)");
    }
    is_as = (argc > 2) && !strcmp (argv[2], "as");
    if (is_as && argc < 4) {
	report (EX_USAGE, 0, "missing argument(s)");
    }
    nargc = argc - (is_as ? 3 : 1);
    if (!(nargv = (char **) malloc ((nargc + 1) * sizeof(char *)))) {
	report (1, errno, "");
    }
    for (ix = 1, jx = 0; ix < argc; ++ix) {
	if (ix < 3 && is_as) { continue; }
	nargv[jx++] = argv[ix];
    }
    nargv[nargc] = NULL;
    cmd = which (argv[1]);
    if (!cmd) { report (1, errno, "%s -", argv[1]); }
    if (is_as) {
	if (!strrchr (nargv[0], '/')) {
	    if (!(p = strrchr (cmd, '/'))) {
		size_t nnsz = (size_t) (p - cmd) + strlen (nargv[0]) + 2;
		if (!(newname = t_allocv (char, nnsz))) {
		    report (1, errno, "");
		}
		snprintf (newname, nnsz, "%.*s/%s",
					 (int) (p - cmd), cmd, nargv[0]);
		nargv[0] = newname;
	    }
	}
    }
    execve (cmd, nargv, environ);
    report (0, errno, "%s -", cmd);
    return 1;
}
