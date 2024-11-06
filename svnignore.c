/* svnignore.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2024, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Setting the `svn:ignore` property of a directory from a file within this
** directory and additionally from a list of file(patern)s specified on the
** command line.
**
** SYNOPSIS
**
**    admin/svnignore [-w svnignore-file] file/pattern...
**
**    admin/svnignore [-r svnignore-file] file/pattern...
**
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <sysexits.h>

#include <sys/stat.h>
#include <sys/wait.h>

typedef struct list list_t;
struct list {
    list_t *next;
    char *s;
};

list_t *list_push (list_t *old, char *s, bool duplicate)
{
    list_t *new;
    size_t ssz = (duplicate ? strlen (s) + 1 : 0);
    size_t elsz = sizeof(list_t);
    if ((new = (list_t *) malloc (elsz + ssz))) {
	new->next = old;
	if (duplicate) {
	    char *p = (char *) new + elsz;
	    new->s = p; memcpy (p, s, ssz);
	} else {
	    new->s = s;
	}
    }
    return new;
}

void list_free (list_t *list)
{
    list_t *tail;
    int ec = errno;
    while (list) {
	tail = list->next; list->next = NULL;
	free (list); list = tail;
    }
    errno = ec;
}

const list_t *list_find (const char *s, list_t **_list)
{
    bool found = false;
    list_t *list = *_list, *curr, *prev;
    if (! list) { return NULL; }
    if (strcmp (list->s, s) == 0) { return list; }
    prev = list;
    while ((curr = prev->next)) {
	if (strcmp (curr->s, s) == 0) {
	    prev->next = curr->next;
	    curr->next = list; list = curr;
	    found = true;
	    break;
	}
	prev = curr;
    }
    *_list = list;
    return curr;
}

#define errguard(stmts) do { int ec = errno; stmts; errno = ec; } while (0)

static int write_ignore_file (const char *ignore_file, list_t *list)
{
    FILE *ofp;
    list_t *el;
    int ix, rc = -1, ec;

    if ((ofp = fopen (ignore_file, "w"))) {
	rc = 0;
	for (el = list; el; el = el->next) {
	    if (fputs (el->s, ofp) == EOF) { rc = -1; goto EXIT_POINT; }
	    if (fputs ("\n", ofp) == EOF) { rc = -1; goto EXIT_POINT; }
	}
	fclose (ofp);
    }
EXIT_POINT:
    list_free (list);
    if (ofp) { errguard (fclose (ofp)); }
    if (rc < 0) { errguard (unlink (ignore_file)); }
//    if (ofp) { ec = errno; fclose (ofp); errno = ec; }
//    if (rc < 0) { ec = errno; unlink (ignore_file); errno = ec; }
    return rc;
}

static int cuteol (char *s)
{
    char *p = s + strlen (s);
    if (p > s) {
	if (*--p == '\n') {
	    *p = '\0';
	    if (p > s && *--p == '\r') { *p = '\0'; return 2; }
	    return 1;
	} else if (*p == '\r') {
	    *p = '\0'; return 3;
	}
    }
    return 0;
}

static int read_ignore_file (const char *ignore_file, list_t **_list)
{
    FILE *ifp = NULL;
    list_t *list = *_list, *newel;
    char buf[4096], rest[4096];
    int rc = -1, ec;
    bool had_overflows = false;

    if ((ifp = fopen (ignore_file, "r"))) {
	rc = 0;
	while (fgets (buf, sizeof(buf), ifp)) {
	    if (! cuteol (buf)) {
		while (fgets (rest, sizeof(rest), ifp)) {
		    if (cuteol (rest)) { break; }
		}
		had_overflows = true;
	    }
	    if (! list_find (buf, &list)) {
		if (!(newel = list_push (list, buf, true))) {
		    rc = -1; goto EXIT_POINT;
		}
		list = newel;
	    }
	}
	if (ferror (ifp)) { rc = -1; }
    }
EXIT_POINT:
    *_list = list;
    if (ifp) { errguard (fclose (ifp)); }
    if (rc == 0 && had_overflows) {
	errno = ENAMETOOLONG; rc = -1;
    }
    return rc;
}

static int svn_propset (const char *prop, const char *dir, list_t *list)
{
    int pfd[2];
    pid_t pid;
    if (pipe (pfd) < 0) { return -1; }
    switch ((pid = fork())) {
	case -1:
	    /*ERROR*/
	    return -1;
	case 0:
	    /*CHILD*/ {
	    int rc, ec;
	    const char *cmd[] = {
		"svn", "propset", prop, "-F", "-", dir, NULL
	    };
	    dup2 (pfd[0], 0); close (pfd[1]);
	    execvp (*cmd, (char *const *) cmd);
	    fprintf (stderr, "svn propset - %s\n", strerror (errno));
	    exit (99);
	}
	default: /*PARENT*/ {
	    int wstat, ec = 0;
	    FILE *fp = fdopen (pfd[1], "wb");
	    if (! fp) {
		ec = errno;
		kill (pid, SIGPIPE);
	    } else {
		list_t *el;
		for (el = list; el; el = el->next) {
		    fprintf (fp, "%s\n", el->s);
		}
		fclose (fp);
	    }
	    waitpid (pid, &wstat, 0);
	    if (WIFEXITED(wstat)) {
		int xc = WTERMSIG(wstat);
		switch (xc) {
		    case 0:  return 0;
		    case 99: errno = ENOENT; return -1;
		    default: errno = ECOMM; return -1;
		}
	    }
	    if (WIFSIGNALED(wstat)) {
		int xc = WTERMSIG(wstat);
		errno = (xc != SIGPIPE ? ECOMM : (ec ? ec : ESPIPE));
		return -1;
	    }
	    /* Don't accept any process suspend! */
	    kill (pid, SIGKILL);
	    waitpid (pid, &wstat, 0);
	    errno = EPROTO;
	    return -1;
	}

    }
}

int add_ignores (int argc, char *argv[], list_t **_list)
{
    list_t *list = *_list, *new;
    int ix, rc = 0;

    for (ix = 0; ix < argc; ++ix) {
	char *arg = argv[ix];
	if (! list_find (arg, &list)) {
	    if (!(new = list_push (list, arg, false))) {
		rc = -1; goto EXIT_POINT;
	    }
	    list = new;
	}
    }
EXIT_POINT:
    *_list = list;
    return rc;
}

const char *prog;

const char *bn (const char *path)
{
    const char *res = strrchr (path, '/');
    if (res) { ++res; } else { res = path; }
    return res;
}

__attribute__((noreturn, format(printf, 1, 2)))
static void usage (const char *format, ...)
{
    if (format) {
	va_list args;
	fprintf (stderr, "%s: ", prog);
	va_start (args, format); vfprintf (stderr, format, args); va_end (args);
	fputs ("\n", stderr);
	exit (EX_USAGE);
    }
    printf ("Usage: %s [-r file] dir file/pattern...\n"
	    "       %s [-w file] file/pattern...\n"
	    "       %s [-h]\n"
	    "\nOptions/Arguments:"
	    "\n  -h (or an empty argument list)"
	    "\n     Write this usage message to the standard output and terminate."
	    "\n  -r file"
	    "\n     Read entries from an 'file', append each given 'file/pattern' and send the result to 'svn propset svn:ignore -F - dir'"
	    "\n  -w file"
	    "\n     Create (or overwrite) 'file' and fill it with the given 'file/pattern' values (one per line)."
	    "\n  file/pattern"
	    "\n     A list of file(-pattern)s"
	    "\n", prog, prog, prog);
    exit (0);
}

__attribute__((noreturn, format(printf, 2, 3)))
static void quit (int xc, const char *format, ...)
{
    va_list args;
    fprintf (stderr, "%s: ", prog);
    va_start (args, format); vfprintf (stderr, format, args); va_end (args);
    fputs ("\n", stderr);
    exit (xc);
}


bool is_dir (const char *file)
{
    struct stat sb;
    if (lstat (file, &sb)) { return false; }
    return S_ISDIR(sb.st_mode);
}

bool no_file (const char *file)
{
    struct stat sb;
    if (lstat (file, &sb)) { return false; }
    if (S_ISREG (sb.st_mode)) { return false; }
    errno = EACCES; return true;
}

bool is_file (const char *file)
{
    struct stat sb;
    if (lstat (file, &sb)) { return false; }
    if (S_ISREG(sb.st_mode)) { return true; }
    errno = EACCES; return false;
}

int main (int argc, char *argv[])
{
    int opt;
    const char *file = NULL, *dir = NULL;
    bool wflag = false, rflag = false;
    list_t *list = NULL;

    prog = bn (*argv);

    if (argc < 2) { usage (NULL); }
    while ((opt = getopt (argc, argv, "+:r:w:")) != -1) {
	switch (opt) {
	    case 'h': usage (NULL);
	    case 'r':
		if (rflag) { usage ("Ambiguous option '-%c'", opt); }
		if (wflag) {
		    usage ("Options '-r' and '-w' are mutually exclusive.");
		}
		file = optarg; rflag = true;
		break;
	    case 'w':
		if (wflag) { usage ("Ambiguous '-%c' option.", opt); }
		if (rflag) {
		    usage ("Options '-r' and '-w' are mutually exclusive.");
		}
		file = optarg; wflag = true;
		break;
	    case ':': usage ("Missing argument for option '-%c'.", optopt);
	    case '?': default: usage ("Invalid option '-%c'.", optopt);
	}
    }
    if (optind >= argc) {
	usage ("Not enough arguments.");
    }
    if (rflag || !wflag) {
	FILE *rfp;
	dir = argv[optind++];
	if (! is_dir (dir)) {
	    quit (EX_NOPERM, "\"%s\" - not a directory.", dir);
	}
    }
    if (rflag) {
	if (! is_file (file)) {
	    quit (EX_NOINPUT, "\"%s\" - %s", file, strerror (errno));
	}
	if (read_ignore_file (file, &list) < 0) {
	    quit (EX_NOINPUT, "\"%s\" - %s", file, strerror (errno));
	}
    }
    if (add_ignores (argc - optind, &argv[optind], &list) < 0) {
	quit (EX_OSERR, "add_ignores() - %s", strerror (errno));
    }
    if (! list) {
	quit (EX_NOINPUT, "%s", strerror (EX_NOINPUT));
    }
    if (wflag) {
	if (no_file (file)) {
	    quit (EX_NOPERM, "\"%s\" - %s", file, strerror (errno));
	}
	if (write_ignore_file (file, list) < 0) {
	    quit (EX_CANTCREAT, "\"%s\" - %s", file, strerror (errno));
	}
    } else {
	const char *prop = "svn:ignore";
	if (svn_propset (prop, dir, list)) {
	    quit (EX_PROTOCOL, "svn propset %s ... failed.", prop);
	}
    }
    return 0;
}
