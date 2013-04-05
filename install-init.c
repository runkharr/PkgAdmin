/* install-init.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Try to install an init-file ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#define PROG "install-init"

#include "lib/set_prog.c"
#include "lib/mrmacs.c"
#include "lib/trans_path.c"
#include "lib/sdup.c"
#include "lib/pbCopy.c"
#include "lib/cwd.c"
#include "lib/cuteol.c"

static
char *abspath (const char *path)
{
    char *res, *p;
    if (*path == '/') {
	ifnull (res = sdup (path)) { return res; }
    } else {
	const char *wd = cwd ();
	ifnull (res = t_allocv (char, strlen (wd) + strlen (path) + 2)) {
	    return res;
	}
	p = pbCopy (res, wd); p = pbCopy (p, "/"); pbCopy (p, path);
    }
    if (!trans_path (res, res)) { free (res); res = NULL; }
    return res;
}

static
int is_placeholder (const char *p, const char *b, size_t bsz)
{
    size_t pl = strlen (p);
    if (bsz < strlen (p) + 1) { return 0; }
    return (*b == '%' && !memcmp (b + 1, p, pl) && b[pl + 1] == '%' ? 1 : 0);
}

static
int file_replace (FILE *in, char **vars, char **values, FILE *out)
{
    char buf[2048], *bp, *be, *p, *q, *var, *val;
    int ix; ssize_t bl;
    bl = fread (buf, 1, sizeof(buf) >> 1, in);
    if (bl > 0) {
	bp = buf + bl;
	while ((bl = fread (bp, 1, sizeof(buf) - bl, in)) > 0) {
	    be = bp + bl; p = buf; q = p;
	    while (p < bp) {
		var = NULL; val = NULL;
		for (ix = 0; vars[ix]; ++ix) {
		    if (is_placeholder (vars[ix], p, (size_t) (be - p))) {
			var = vars[ix]; val = values[ix]; break;
		    }
		}
		if (var) {
		    if (p > q) { fwrite (q, 1, (size_t) (p - q), out); }
		    fputs (val, out); q = p + strlen (var) + 2; p = q;
		} else {
		    ++p;
		}
	    }
	    bl = (size_t) (be - p); memcpy (buf, p, bl);
	    bp = buf + bl;
	}
	p = bp; q = p;
	while (p < be) {
	    var = NULL; val = NULL;
	    for (ix = 0; vars[ix]; ++ix) {
		if (is_placeholder (vars[ix], p, (size_t) (be - p))) {
		    var = vars[ix]; val = values[ix]; break;
		}
	    }
	    if (var) {
		if (p > q) { fwrite (q, 1, (size_t) (p - q), out); }
		fputs (val, out); q = p + strlen (var) + 2; p = q;
	    } else {
		++p;
	    }
	}
	if (p < be) { fwrite (p, 1, (size_t) (be - p), out); }
	bl = 0;
    }
    return (bl < 0 ? -1 : 0);
}


int lxdistrib (void)
{
    int errsave;
    FILE *issue;
    char line[1024];
    if (!(issue = fopen ("/etc/issue.net"))) {
	if (!(issue = fopen ("/etc/issue"))) { return -1; }
    }
    if (!fgets (line, sizeof(line), issue)) {
	errsave = errno; fclose (issue); errno = errsave; return -1;
    }
    fclose (issue);
    if (!cuteol (line)) { errno = EINVAL; return -1; }


void usage (const char *format, ...)
{
    unlessnull (format) {
	va_list ual;
	fprintf (stderr, "%s: ", prog);
	va_start (ual, format); vfprintf (stderr, format, ual); va_end (ual);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("Usage: %s [-q] [-m filemode] daemon-path init-path\n"
	    "       %s -h\n"
	    "\nArguments:"
	    "\n  -q (quiet)"
	    "\n    don't write installation messages to stdout"
	    "\n  -m filemode (permissions)"
	    "\n    change the permission bits of the init-file to the value of"
	    " 'filemode'"
	    "\n  -h (help)"
	    "\n    write this text to stdout and terminate"
	    "\n  daemon-path"
	    "\n    the absolute pathname of the daemon program to be started"
	    " by the init-"
	    "\nscript"
	    "\n  init-path"
	    "\n    path to the init-script prototype\n",
	    prog, prog);
    exit (0);
}


int main (int argc, char *argv[])
{
    int optx, fmask = 0755, quiet = 0, opt_m = 0;
    char *daemon = NULL, *init = NULL, *optstrg, *optarg;

    set_prog (argc, argv);

    for (optx = 1; optx < argc; ++optx) {
	optstrg = argv[optx];
	if (*optstrg != '-') { break; }
	if (!strcmp (optstrg, "--")) { ++optx; break; }
	++optstrg;
	if (*optstrg == 'q' && !optstrg[1]) {
	    quiet = 1; continue;
	}
	if (*optstrg == 'm') {
	    if (!optstrg[1] && optx >= argc - 1) {
		usage ("missing argument for option '-m'");
	    }
	    if (opt_m) { usage ("ambiguous option '-m'"); }
	    optarg = (optstrg[1] ? &optstrg[1] : argv[++optx]);
	    if ((fmask = get_fmode (optarg)) < 0) {
		usage ("invalid argument for option '-m'");
	    }
	    opt_m = 1;
	}
	if (!strcmp (optstrg, "h") || !strcmp (optstrg, "-help")) {
	    usage (NULL);
	}
    }
    if (optx > argc - 2) { usage ("missing argument(s)"); }

    daemon = argv[optx]; init = argv[optx + 1];
