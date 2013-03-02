/* admin/uid.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** uid - write the numerical (effective or real) user id to stdout
** gid - write the numerical (effective or real) group id to stdout
** ugid - write the numerical (effective or real) user and group id's to stdout
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define PROG "uid"

#include "lib/set_prog.c"

static void
usage (const char *fmt, ...)
{
    if (fmt) {
	va_list av;
	fprintf (stderr, "%s: ", prog);
	va_start (av, fmt); vfprintf (stderr, fmt, av); va_end (av);
	fputs ("\n", stderr);
	exit (64);
    }
    if (!strcmp (prog, "uid") || !strcmp (prog, "gid")
    ||  !strcmp (prog, "ugid")) {
	printf ("Usage: %s [cmd] [-e|-r] [-f format]\n", prog);
	printf ("       %s [cmd] -h\n", prog);
    } else {
	printf ("Usage: %s cmd [-e|-r] [-f format]\n", prog);
	printf ("       %s cmd -h\n\n", prog);
	printf ("  where" "\n    cmd is one of 'uid', 'gid', 'ugid'\n");
    }
    printf ("\nOptions:"
	    "\n  -e"
	    "\n    write the effective uid/gid (or both) to stdout (default)"
	    "\n  -r"
	    "\n    write the real uid/gid (or both) to stdout"
	    "\n  -f format"
	    "\n    write 'format' to stdout, replacing each occurrence of"
	    "\n    %%u and %%g with the effective/real user-id/group-id\n");
    exit (0);
}

static int
cmdnum (const char *s)
{
    if (!strcmp (s, "uid")) { return 1; }
    if (!strcmp (s, "gid")) { return 2; }
    if (!strcmp (s, "ugid")) { return 3; }
    return 0;
}

int main (int argc, char *argv[])
{
    int opt, optx = 0, use_realid = 0, cmd = 0;
    char *format = NULL, *p;
    uid_t uid;
    gid_t gid;

    set_prog (argc, argv);

    cmd = cmdnum (prog);
    if (argc > 1 && *argv[1] != '-') {
	optx = 1;
	if (argc < 2) { usage (NULL); }
	if ((cmd = cmdnum (argv[1])) <= 0) {
	    usage ("invalid cmd; try `%s cmd -h´ for help, please!", prog);
	}
    }
    while ((opt = getopt (argc - optx, argv + optx, "ef:hr")) != -1) {
	switch (opt) {
	    case 'e':
		break;
	    case 'f':
		if (format) { usage ("ambiguous argument to option `-f´"); }
		format = optarg; cmd = 3;
		break;
	    case 'h':
		usage (NULL);
		break;
	    case 'r':
		use_realid = 1;
		break;
	    default:
		break;
	}
    }
    if (cmd == 3 && format == NULL) { format = "%u:%g"; }
    if (use_realid) {
	uid = getuid (); gid = getgid ();
    } else {
	uid = geteuid (); gid = getegid ();
    }
    switch (cmd) {
	case 1:
	    printf ("%d\n", uid);
	    break;
	case 2:
	    printf ("%d\n", gid);
	    break;
	case 3:
	    p = format;
	    while (*p) {
		if (*p == '%') {
		    switch (*++p) {
			case 'u': ++p; printf ("%d", uid); break;
			case 'g': ++p; printf ("%d", gid); break;
			case '%': fputc (*p++, stdout); break;
			default:
			    fputc ('%', stdout); fputc (*p++, stdout); break;
		    }
		} else {
		    fputc (*p++, stdout);
		}
	    }
	    fputs ("\n", stdout);
	    break;
    }
    return 0;
}
