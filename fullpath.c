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
** Expands pathname to an absolute pathname (removing occurrences of `.´ and
** `..´) and then shortens it by `level´ items ...
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

#define CCEOS 0
#define CCSLASH 1
#define CCDOT 2
#define CCOTHER 3

#define S0 0
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define S5 5
#define S6 6

static const char strans[7][4] = {
    /*S0*/ { S6, S1, S6, S6 },
    /*S1*/ { S5, S1, S2, S4 },
    /*S2*/ { S5, S1, S3, S4 },
    /*S3*/ { S5, S4, S4, S4 },
    /*S4*/ { S5, S1, S4, S4 },
    /*S5*/ { -1, -1, -1, -1 },
    /*S6*/ { -2, -2, -2, -2 }
};

#define NOP 0
#define STORE 1
#define SKIP 2
#define DOTSTORE 3
#define BACK 4
#define DOTDOTSTORE 5
#define TERMINATE 6
#define ERROR 7

static const char saction[7][4] = {
    /*S0*/ { ERROR, STORE, ERROR, ERROR },
    /*S1*/ { TERMINATE, SKIP, SKIP, STORE },
    /*S2*/ { TERMINATE, SKIP, SKIP, DOTSTORE },
    /*S3*/ { BACK, BACK, DOTDOTSTORE, DOTDOTSTORE },
    /*S4*/ { TERMINATE, STORE, STORE, STORE },
    /*S5*/ { TERMINATE, ERROR, ERROR, ERROR },
    /*S6*/ { ERROR, ERROR, ERROR, ERROR }
};

static int trans_path (char *p, char *q)
{
    int cclass, action, state = S0, lastch = 0;
    char *sp = p;
    for (;;) {
	switch (*q) {
	    case '\0': cclass = CCEOS; break;
	    case '/':  cclass = CCSLASH; break;
	    case '.':  cclass = CCDOT; break;
	    default:   cclass = CCOTHER; break;
	}
	action = saction[state][cclass];
	state = strans[state][cclass];
	switch (action) {
	    case NOP:
		lastch = *q; break;
	    case STORE:
		lastch = *q; *p++ = *q++; break;
	    case SKIP:
		lastch = *q; ++q; break;
	    case DOTSTORE:
		lastch = *q; *p++ = '.'; *p++ = *q++; break;
	    case BACK:
		--p; if (*p == '/' && p > sp) { --p; }
		while (p > sp && *--p != '/');
		lastch = *q;
		break;
	    case DOTDOTSTORE:
		lastch = *q; *p++ = '.'; *p++ = '.'; *p++ = *q++; break;
	    case TERMINATE:
		if (lastch == '/' && p > sp) { --p; }
		*p = '\0'; return 0;
	    case ERROR:
		return -1;
	}
    }
}

#define MAXLEVEL 1024

int main (int argc, char *argv[])
{
    int opt, level = 0, ix, errcc = 0, newitem, dots, lastch;
    unsigned long v;
    const char *wd;
    char *p, *q, *buf = NULL, *ep, *sp, *path;
    size_t bufsz = 0;

    set_prog (argc, argv);
    while ((opt = getopt (argc, argv, ":b:h")) != -1) {
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
