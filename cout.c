/* cout.c
**
** $Id: cout.c 7193 2025-03-03 12:14:03Z bj@rhiplox $
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** The C-version of my small `cout` script.
**
*/

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sysexits.h>
#include <fcntl.h>

#include "lib/cmdvec.c"
#include "lib/quit.c"
#include "lib/separators.c"

# define ESC_BS ""

#define _S(x) # x
#define S(x) _S(x)

#define streq(x, y) (strcmp ((x), (y)) == 0)
#define strpfx(x, y) \
    ({ typeof(x) _xp_ = (x); strncmp (_xp_, (y), strlen (_xp_)) == 0; })

#define ERRSTR (strerror (errno))

const char *prog;

static const char *bn (const char *path)
{
    const char *res = strrchr (path, DIRSEP);
    if (*res) { ++res; } else { res = path; }
    return res;
}

__attribute__((noreturn, format(printf, 1, 2)))
static void usage (const char *format, ...)
{
    if (format) {
	va_list args;
	va_start (args, format); vfprintf (stderr, format, args); va_end (args);
	exit (EX_USAGE);
    }
    printf ("Usage: %s [-q|-v|-qv|-vq] [-m message] command [argument...]\n"
	    "       %s\n"
	    "\nOptions/Arguments"
	    "\n  -m message"
	    "\n      Write `message` to the standard output instead of the"
	    " `command`"
	    "\n      if `-v` was not set."
	    "\n  -q  Suppress any output of `command`."
	    "\n  -v  Print the command to be executed beforehand"
	    "\n  -qv (alt: -vq)"
	    "\n      This is a combination of `-q` and `-v`."
	    "\n  command [argument...]"
	    "\n      The command (plus arguments) to be executed"
	    "\n"
	    "\n%s without arguments prints this usage message on `stdout`."
	    "\n", prog, prog, prog);
    exit (0);
}

static int quote_print (const char *s, FILE *out)
{
    int rc = 0;
    if (s) {
	if (! *s) {
	    fputs ("\"\"", out); rc = 2;
	} else if (! strpbrk (s, "\a\b\t\n\v\f\r !\"$'" ESC_BS "`")) {
	    fputs (s, out); rc = (int) strlen (s);
	} else if (! strpbrk (s, "!\"'" ESC_BS "`")) {
	    fputc ('"', out); fputs (s, out); fputc ('"', out);
	    rc = strlen (s) + 2;
	} else {
	    int ch;
	    const char *p = s;
	    fputc ('"', out); rc = 1;
	    while ((ch = (int) *p++ & 0xFF)) {
		switch (ch) {
		    case '"': case '!': case '`':
			fputc ('\\', out); ++rc;
			break;
		    case '\\':
			if (*ESC_BS) { fputc ('\\', out); ++rc; }
			break;
		    default:
			break;
		}
		fputc (ch, out); ++rc;
	    }
	    fputc ('"', out); ++rc;
	}
	fflush (out);
    }
    return rc;
}

int main (int argc, char *argv[])
{
    int ix = 1;
    FILE *xout = NULL; int xfd = -1;
    char **cmdvec = NULL, *opt, *msg = NULL;
    bool verbose = false, quiet = false;
    prog = bn (*argv);
    if (ix >= argc) { usage (NULL); }
    opt = argv[ix];
    if (streq (opt, "-v") || streq (opt, "--verbose")) {
	++ix; verbose = true;
    } else if (streq (opt, "-q") || streq (opt, "--quiet")) {
	++ix; quiet = true;
    } else if (streq (opt, "-qv") || streq (opt, "-vq")) {
	++ix; quiet = true; verbose = true;
    }
    opt = argv[ix];
    if (strpfx ("-m", opt)) {
	++ix; opt += 2;
	if (*opt) {
	    msg = opt;
	} else {
	    if (ix >= argc) { usage ("Missing argument for option `-m`."); }
	    msg = argv[ix++];
	    if (! *msg) { usage ("The `message` must not be empty."); }
	}
    }
    if (ix >= argc) { usage ("Missing `command`."); }
    if (! (cmdvec = gen_cmdvec (&argv[ix], argc - ix))) {
	quit (EX_OSERR, "gen_cmdvec() - %s", ERRSTR);
    }
    if (verbose) {
	int ix = 0;
	quote_print (cmdvec[ix], stdout);
	while (cmdvec[++ix]) {
	    fputs (" ", stdout); quote_print (cmdvec[ix], stdout);
	}
	fputs (EOL, stdout);
    } else if (msg) {
	fputs (msg, stdout); fputs (EOL, stdout);
    }

    if (quiet) {
	int out_fd = 1; /*`stdout`*/
	int err_fd = 2; /*`stderr`*/
	int fd = open ("/dev/null", O_CREAT|O_APPEND|O_WRONLY, 0644);
	if (fd < 0) { quit (EX_OSERR, "open() - %s", ERRSTR); }
	fflush (stdout); fflush (stderr);
	// Duplicate for `execvp()` failure
	if ((xfd = dup (err_fd)) >= 0) { xout = fdopen (xfd, "ab"); }
	(void) dup2 (fd, out_fd);
	(void) dup2 (fd, err_fd);
	(void) close (fd);
    } else {
	xout = stderr;
    }

    execvp (*cmdvec, cmdvec);

    if (xout) {
	fprintf (xout, "%s: `%s` failed - %s\n", prog, *cmdvec, ERRSTR);
	fflush (xout);
    }

    return EX_UNAVAILABLE;
}
