/* rmmtdir.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2023, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Check if a directory is empty and remove it if so ...
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sysexits.h>
#include <stdarg.h>

#include <sys/types.h>
#include <dirent.h>

/** Displaying either the _usage_ message (on `stdout`), terminating the
**  program with the exit code 0, or a given usage message (`printf`-style),
**  terminating the program with `EX_USAGE` in this case.
*/
static void usage (const char *prog, const char *fmt, ...)
{
    if (fmt) {
	va_list ap;
	fprintf (stderr, "%s: ", prog);
	va_start(ap, fmt); vfprintf (stderr, fmt, ap); va_end(ap);
	fputs ("\n", stderr);
	exit (EX_USAGE);
    }
    printf ("Usage: %s [-[f]i] [-q...] [-s] <pathname>...\n"
	    "       %s [-h]\n"
	    "\nOptions/Arguments:"
	    "\n  -h  Help. This programm allows for an empty argument list or"
	    " the option `-h`"
	    "\n      specified for writing this usage message to `stdout`."
	    "\n  -f  Forcing the deletion of empty directories, even if the"
	    " interactive mode"
	    "\n      fails, because `stdin` is not a terminal."
	    "\n  -i  Turning the interactive mode on. Each time, an empty"
	    " directory is found,"
	    "\n      this program asks the user to permit a removal."
	    "\n  -q  Increase the `quietness` level. If set once, the output"
	    " of error messages"
	    "\n      is suppressed; if set twice or more, it even succeeds if"
	    " some errors"
	    "\n      occurred."
	    "\n  -s  Writing a small processing statistic before terminating."
	    "\n",
	    prog, prog);
    exit (0);
}

/** Check if a string consists entirely of `.` or `..` ... */
static __inline__ bool dotordotdot (const char *s)
{
    if (*s != '.') { return false; }
    if (s[1] == '\0') { return true; }
    if (s[1] != '.') { return false; }
    return s[2] == '\0';
}

/** Check if a directory is empty (save from `.` and `..`) */
static int check_if_empty (const char *path)
{
    bool is_empty = true;
    struct dirent *de = NULL;
    DIR *dh = opendir (path);
    if (! dh) { return -1; }

    while ((de = readdir (dh))) {
	if (dotordotdot (de->d_name)) { continue; }
	is_empty = false;
	break;
    }
    closedir (dh);
    return (is_empty ? 0 : 1);
}

/** Testing if a string `probe` (with a given length `plen`) exactly matches
**  one of the strings in the (non-empty) a NULL-terminated list of matches,
**  the remaining arguments consist of.
*/
static bool is_oneof (const char *probe, size_t plen, const char *set, ...)
{
    va_list ap;
    const char *element = set;
    size_t element_len = strlen (element);
    if (element_len == plen && strncmp (probe, element, plen) == 0) {
	return true;
    }
    va_start(ap, set);
    while ((element = va_arg(ap, const char *))) {
	element_len = strlen (element);
	if (element_len == plen && strncmp (probe, element, plen) == 0) {
	    return true;
	}
    }
    va_end (ap);
    return false;
}

/** Truncate an EOL sequence from the end of a given string and return a code
**  specifying the type of EOL sequence found.
*/
static __inline__ int cuteol (char *buf)
{
    char *p = buf + strlen (buf);
    if (p > buf) {
	if (*--p == '\n') {
	    *p = '\0'; if (p > buf && *--p == '\r') { *p = '\0'; return 3; }
	    return 1;
	}
	if (*p == '\r') { *p = '\0'; return 2; }
    }
    return 0;
}

/** Asking a question, this function returns either `true` if the answer is
**  positive, or `false`, otherwise. Any invalid answer leads to an error
**  message and the repeated display of the question.
**  Valid answers are: `yes`, `y`, `ja`, `j`, `ok` (positive answer), or
**  `no`, `nein`, `n` (negative answer).
*/
static int ask (const char *prompt, ...)
{
    va_list ap;
    char buf[1024], rest[1024], *p, *q;
    int res = 0;
    for (;;) {
	{   /* Constructing the prompt. */
	    va_start (ap, prompt); vprintf (prompt, ap); va_end (ap);
	    fputs (" ? ", stdout);
	    fflush (stdout);
	}
	p = fgets (buf, sizeof(buf), stdin);
	if (! p) {
	    if (ferror (stdin)) { fputs ("ERROR\n", stdout); res = -2; break; }
	    /*feof (stdin) is true*/
	    fputs ("EOF\n", stdout); res = -1; break;
	}
	while (cuteol (p) == 0) {
	    p = fgets (rest, sizeof(rest), stdin);
	    if (! p) { break; }
	}
	p = buf;
	while (isspace (*p)) { ++p; }
	q = p++; while (*p && !isspace (*p)) { ++p; }
	if (is_oneof (q, (p - q), "yes", "y", "ja", "j", "ok", NULL)) {
	    res = 0; break;
	}
	if (is_oneof (q, (p - q), "no", "nein", "n", NULL)) {
	    res = 1; break;
	}
	fputs ("** Incorrect answer. Please retry!\n\n", stderr);
    }
    return res;
}

/** Quick and dirty version of getting the _basename_ of a pathname, meaning:
**  the last path element.
*/
static __inline__ const char *get_basename (const char *path)
{
    const char *res = strrchr (path, '/');
    if (res) { ++res; } else { res = path; }
    return res;
}

/* Main program: */
int main (int argc, char *argv[])
{
    const char *prog = get_basename (*argv);	// Get the program name
    int optc = '\0', ix;			// Option character, loop-index
    unsigned int ec, dc;			// error and processing counters
    bool interactive = false;			// interactive mode
    bool force = false;				// force if interactive fails
    bool statistics = false;
    int quiet = 0;				// quietness level

    if (argc < 2) { usage (prog, NULL); }	// Standard help invocation

    while ((optc = getopt (argc, argv, ":fhiqs")) != -1) {
	switch (optc) {
	    case 'f':				// Continue if interactive mode
		force = 1;			// fails (stdin not a terminal)
		break;
	    case 'h':
		usage (prog, NULL);		// Additional help invocation
	    case 'i':
		interactive = true;		// Enable interactive mode
		break;
	    case 'q':
		++quiet;			// Increate the quietness level
		interactive = 0;
		force = 0;
		statistics = 0;
		break;
	    case 's':
		statistics = true;		// Enable statistic on end
		break;
	    case '?':				// Invalid option
		usage (prog, "Invalid option `-%c`", optopt);
		exit (EX_USAGE);
	    case ':':				// Missing an option argument
		usage (prog, "Missing argument for `-%c`", optopt);
		exit(EX_USAGE);
	    default:				// Should *never* occur!
		usage (prog, "%s: Unknown error.");
		exit(EX_USAGE);
	}
    }

    // Options, but no further arguments is a usage error.
    if (optind >= argc) {
	usage (prog, "Missing argument(s). Try `%s` for help, please!", prog);
    }

    // Processing each of the remaining (non-optional) arguments
    for (dc = ec = 0, ix = optind; ix < argc; ++ix) {
	const char *path = argv[ix];
	int rc = check_if_empty (path);
	++dc;				// Increase processing counter
	if (rc < 0) {
	    if (quiet < 1) {
		fprintf (stderr, "%s - %s\n", path, strerror (errno));
	    }
	    ++ec; continue;		// Increase error counter & process next
	}
	if (rc > 0) { continue; }	// Non-empty directory
	if (interactive) {			// Ask for the deletion
	    if (isatty (fileno (stdin))) {	// but only on a terminal
		int ans = ask ("Ok to delete %s", path);
		if (ans < 0) { break; }		// EOF found in input
		if (ans > 0) { continue; }	// Don't delete!
	    } else if (force) {			// No terminal, but `-f` given
		fprintf (stderr, "%s: `stdin` is not a terminal; continuing"
				 " non-interactive.\n",
				 prog);
		interactive = false;		// Continue non-interactive
	    } else {				// No terminal, no `-f` given
		fprintf (stderr, "%s: `stdin` is not a terminal; aborting.\n",
				 prog);
		++ec; break;			// Aborting!
	    }
	}

	if (rmdir (path)) {			// Remove the (empty) directory
	    if (quiet < 1) {
		fprintf (stderr, "%s - %s\n", path, strerror (errno));
	    }
	    ++ec; continue;			// Removal error
	}
    }

    // Print a statistic if requested
    if (statistics && quiet < 1) {
	printf ("Processed: %u\n", dc);
	printf ("Errors:    %u\n", ec);
    }

    return (ec > 0 && quiet < 2 ? 1 : 0);
}
