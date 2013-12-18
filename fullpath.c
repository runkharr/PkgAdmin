/* fullpath.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Synopsis:
**    fullpath [-b level] pathname
**
** Expands pathname to an absolute pathname (removing occurrences of '.' and
** '..') and then shortens it by 'level' items ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define PATH "fullpath"

#include "lib/mrmacs.c"
#include "lib/set_prog.c"
#include "lib/cwd.c"
#include "lib/check_ptr.c"
#include "lib/bconc.c"
/*#include "lib/sdup.c"*/
#include "lib/puteol.c"
#include "lib/bwhich.c"
#include "lib/trans_path.c"

static void usage (const char *format, ...)
{
    if (format) {
	va_list ual;
	fprintf (stderr, "%s: ", prog);
	va_start (ual, format); vfprintf (stderr, format, ual); va_end (ual);
	puteol (stderr);
	exit (64);
    }
    printf ("Usage: %s [-b level] pathname...\n"
	    "       %s -h\n",
	    prog, prog);
    exit (0);
}

#define MAXLEVEL 1024

int main (int argc, char *argv[])
{
    int opt, level = 0, ix, errcc = 0, newitem, dots, lastch, use_path = 0;
    unsigned long v;
    const char *wd;
    char *p, *q, *buf = NULL, *ep, *sp, *path;
    size_t bufsz = 0;

    set_prog (argc, argv);
    while ((opt = getopt (argc, argv, ":b:hp")) != -1) {
	switch (opt) {
	    case 'b':
		v = strtoul (optarg, &p, 10);
		if (*p || v > MAXLEVEL) {
		    usage ("invalid level (%ld) (must be in 0...%d)",
			   v, MAXLEVEL);
		}
		level = (int) v;
		break;
	    case 'h': usage (NULL); break;
	    case 'p': use_path = 1; break;
	    case ':':
		usage ("missing argument for option '-%c'", optopt);
		break;
	    default:
		usage ("invalid option '%s'", argv[optind]);
		break;
	}
    }

    if (optind >= argc) { usage ("missing argument(s)"); }

    wd = cwd ();
    for (ix = optind; ix < argc; ++ix) {
	if (*(path = argv[ix]) == '/') {
	    p = bconc (buf, bufsz, path);
	} else if (use_path && !strchr (path, '/')) {
	    p = bwhich (buf, bufsz, path);
	} else {
	    p = bconc (buf, bufsz, wd, "/", path);
	}
	check_ptr ("main", p);
	if (trans_path (p, p)) {
	    fprintf (stderr, "%s: '%s' - invalid\n", prog, p);
	    ++errcc; continue;
	}
	sp = p; p += strlen (p);
	while (level > 0) {
	    --level; if (p == sp) { level = 0; break; }
	    while (p > sp && *p != '/') { --p; }
	    if (p > sp) { *p-- = '\0'; }
	}
	fputs (sp, stdout); puteol (stdout);
    }
    free (buf);
    return (errcc > 0 ? 1 : 0);
}
