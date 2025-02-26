/* cout.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** The C-version of my small `cout` script.
**
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sysexits.h>
#include <fcntl.h>

#if defined(__WIN32) || defined(__WIN64) || \
    defined(__MINGW32) || defined(__MINGW64)
# define DIRSEP '\\'
# define EOL "\r\n"
#else
# define DIRSEP '/'
# define EOL "\n"
#endif
# define ESC_BS ""

#define _S(x) # x
#define S(x) _S(x)

#define streq(x, y) (strcmp ((x), (y)) == 0)

#define ERRSTR (strerror (errno))

static const char *prog;

static const char *bn (const char *path);

__attribute__((noreturn, format(printf, 2, 3)))
static void quit (int exc, const char *format, ...);

__attribute__((noreturn, format(printf, 1, 2)))
static void usage (const char *format, ...);

static char **gen_cmdvec (char **v, size_t v_len);

static int quote_print (const char *s, FILE *out);

int main (int argc, char *argv[])
{
    int ix = 1;
    char **cmdvec = NULL;
    bool verbose = false, quiet = false;
    prog = bn (*argv);
    if (ix >= argc) { usage (NULL); }
    if (streq (argv[ix], "-v") || streq (argv[ix], "--verbose")) {
	++ix; verbose = true;
    } else if (streq (argv[ix], "-q") || streq (argv[ix], "--quiet")) {
	++ix; quiet = true;
    }
    cmdvec = gen_cmdvec (&argv[ix], 0);
    if (verbose) {
	int ix = 0;
	quote_print (cmdvec[ix], stdout);
	while (cmdvec[++ix]) {
	    fputs (" ", stdout); quote_print (cmdvec[ix], stdout);
	}
	fputs (EOL, stdout);
    }
    if (quiet) {
	int out_fd = 1; /*fileno (stdout)?*/
	int err_fd = 1; /*fileno (stderr)?*/
	int fd = open ("/dev/null", O_CREAT|O_APPEND, 0644);
	if (fd < 0) { quit (EX_OSERR, "open() - %s", ERRSTR); }
	fflush (stdout); fflush (stderr);
	dup2 (fd, out_fd); dup2 (fd, err_fd);
	close (fd);
    }

    execvp (*cmdvec, cmdvec);

    return EX_UNAVAILABLE;
}

static const char *bn (const char *path)
{
    const char *res = strrchr (path, DIRSEP);
    if (*res) { ++res; } else { res = path; }
    return res;
}

static int veprintf (const char *format, va_list args)
{
    int res = fprintf (stderr, "%s: ", prog);
    res += vfprintf (stderr, format, args);
    fputs (EOL, stderr);
    res += strlen (EOL);
    return res;
}

__attribute__((noreturn, format(printf, 2, 3)))
static void quit (int exc, const char *format, ...)
{
    if (format) {
	va_list args;
	va_start (args, format); veprintf (format, args); va_end (args);
    }
    exit (exc);
}

__attribute__((noreturn, format(printf, 1, 2)))
static void usage (const char *format, ...)
{
    if (format) {
	va_list args;
	va_start (args, format); veprintf (format, args); va_end (args);
	exit (EX_USAGE);
    }
    printf ("Usage: %s [-q|-v] command [argument...]\n"
	    "       %s\n"
	    "\nOptions/Arguments"
	    "\n  -q (alt: --quiet)"
	    "\n     Suppress any output of `command`."
	    "\n  -v (alt: --verbose)"
	    "\n     Print the command to be executed beforehand"
	    "\n  command [argument...]"
	    "\n      The command (plus arguments) to be executed"
	    "\n"
	    "\n%s without arguments prints this usage message on `stdout`."
	    "\n", prog, prog, prog);
    exit (0);
}

static char **gen_cmdvec (char **v, size_t v_len)
{
    size_t ix;
    char **res;
    size_t res_len;
    if (v_len == 0) {
	for (ix = 0; v[ix]; ++ix);
    } else {
	for (ix = 0; ix < v_len; ++ix) {
	    if (! v[ix]) { break; }
	}
    }
    res_len = ix;
    if (! (res = (char **) malloc ((res_len + 1) * sizeof(char)))) {
	quit (EX_OSERR, "gen_cmdvec() - %s", ERRSTR);
    }
    for (ix = 0; ix < res_len; ++ix) { res[ix] = v[ix]; }
    res[ix] = NULL;
    return res;
}

#if 0
static const char *tesc[] = {
    "\\a", "\\b", "\\t", "\\n", "\\v", "\\f", "\\r", NULL
}
#endif

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
