/* install.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** File installer (resembles the program `install´ from the `coreutils´), but
** allows for (partly) interactive operations ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>

#include <sys/types.h>

#include "lib/prog.c"

#include "lib/lprefix.c"

#include "lib/cuteol.c"

static int vfask (FILE *in, FILE *out, int defaultans, const char *prompt,
		  va_list fpa)
{
    int answer, waseol;
    char ans[128], *p, *q;
    va_list fpa1;
    if (!isatty (fileno (in)) || !isatty (fileno (out))) {
	errno = EINVAL; return -1;
    }
    for (;;) {
	va_copy (fpa1, fpa); vfprintf (out, prompt, fpa); va_end (fpa1);
	if (defaultans > 0) {
	    fputs (" (Y/n)? ", out);
	} else if (defaultans == 0) {
	    fputs (" (y/N)? ", out);
	} else {
	    fputs (" (y/n)? ", out);
	}
	fflush (out);
	if (!fgets (ans, sizeof(ans), in)) {
	    fputs ("<EOF>\n", out); fflush (out); return -1;
	}
	waseol = cuteol (ans);
	p = ans; while (isblank (*p)) { ++p; }
	q = p + strlen (p);
	while (q > p && isblank (*--q)) { *q = '\0'; }
	if (q == p) {
	    answer = (defaultans > 0 ? +1 : (defaultans == 0 ? 0 : -1));
	} else if (is_lprefix (p, "yes") || is_lprefix (p, "ja")) {
	    answer = +1;
	} else if (is_lprefix (p, "no") || is_lprefix (p, "nein")) {
	    answer = 0;
	} else {
	    answer = -1;
	}
	if (answer >= 0) { break; }
	fputs ("*** Wrong answer. Please answer only with (a prefix of)\n"
	       "    'yes' ('ja') or 'no' ('nein')!\n", out);
    }
    return answer;
}

static int fask (FILE *in, FILE *out, int defaultans, const char *prompt, ...)
{
    int res;
    va_list fpa;
    va_start (fpa, prompt);
    res = vfask (in, out, defaultans, prompt, fpa);
    va_end (fpa);
    return res;
}

static int ask (int defaultans, const char *prompt, ...)
{
    int res;
    va_list fpa;
    va_start (fpa, prompt);
    res = vfask (stdin, stdout, defaultans, prompt, fpa);
    va_end (fpa);
    return res;
}

static void set_prog (int argc, char *argv[])
{
    if (argc < 1) {
	prog = "install";
    } else if (!argv || !argv[0]) {
	prog = "install";
    } else if ((prog = strrchr (argv[0], '/'))) {
	++prog;
    } else {
	prog = argv[0];
    }
}

#include "lib/append.c"

static int is_regfile (const char *path)
{
    struct stat sb;
    if (stat (path, &sb)) { return -1; }
    return (S_ISREG(sb.st_mode) ? 1 : 0);
}

static char *which (const char *file)
{
    struct stat sb;
    if (strchr (file, '/')) {
	if (is_regfile (file) <= 0 || access (file, X_OK) < 0) { return NULL; }
	return strdup (file);
    } else {
	char *buf = NULL, *bp, *p, *q, *PATH, *res;
	size_t bufsz = 0;
	if (!(PATH = getenv ("PATH"))) { errno = ENOENT; return NULL; }
	p = PATH;
	while ((q = strchr (p, ':'))) {
	    if (!(bp = lappend (buf, bufsz, NULL, p, (q - p)))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, "/"))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, file))) { return NULL; }
	    if (is_regfile (buf) > 0 && access (file, X_OK) == 0) {
		res = strdup (buf); free (buf); return res;
	    }
	    p = q + 1;
	}
	if (*p) {
	    if (!(bp = append (buf, bufsz, NULL, p))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, "/"))) { return NULL; }
	    if (!(bp = append (buf, bufsz, bp, file))) { return NULL; }
	    if (is_regfile (buf) > 0 && access (file, X_OK) == 0) {
		res = strdup (buf); free (buf); return res;
	    }
	}
	free (buf); errno = ENOENT;
	return NULL;
    }
}

#include "lib/err.c"

static void usage (const char *fmt, ...)
{
    if (fmt) {
	va_list ual;
	fprintf (stderr, "%s: ", prog);
	va_start (ual, fmt); vfprintf (stderr, fmt, ual); va_end (ual);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("Usage: %s [-cqQsvz] [-m mode] [-o owner] [-g group] file... target"
	    "\n       %s [-qQv] -d [-m mode] [-o owner] [-g group] directory"
	    "\n       %s -h\n"
	    "\nOptions:"
	    "\n  -c"
	    "\n    ignored (kept for compatibility reasons)"
	    "\n  -q"
	    "\n    Ask for an existing file being overwritten"
	    "\n  -Q"
	    "\n    Don't overwrite an existing file; display an error message"
	    " instead"
	    "\n  -s"
	    "\n    Discard the symbols from a target (object-)file; only"
	    " possible if"
	    "\n    the 'strip' program exists on the underlying system"
	    "\n  -v"
	    "\n    Display a message for each file being installed"
	    "\n  -z"
	    "\n    Compress a target file; only possible if"
	    "\n    the 'gzip' program exists on the underlying system"
	    "\n  -m mode"
	    "\n    Change the permission-mode of the target file to 'mode'"
	    "\n  -o owner"
	    "\n    Change the ownership of the target file to 'owner'"
	    "\n  -g group"
	    "\n    Change the group of the target file to 'group'"
	    "\n  -d"
	    "\n    Install a target directory"
	    "\n  -h"
	    "\n    Display this message and terminate\n",
	    prog, prog, prog);
    exit (0);
}

static int getmode (const char *mode)
{
    int res = 0, dg;
    size_t len = strlen (mode);
    if (len < 1 || len > 4) { errno = EINVAL; return -1; }
    while (*mode) {
	switch (*mode) {
	    case '0': dg = 0; break;
	    case '1': dg = 1; break;
	    case '2': dg = 2; break;
	    case '3': dg = 3; break;
	    case '4': dg = 4; break;
	    case '5': dg = 5; break;
	    case '6': dg = 6; break;
	    case '7': dg = 7; break;
	    default: errno = EINVAL; return -1;
	}
	res = (res << 3) | dg; ++mode;
    }
    if (len < 3) { res = (res << 6) | 077; }
    return res;
}

static int getuser (const char *user)
{
    char *p;
    long v;
    struct passwd *pw;
    v = strtol (user, &p, 10);
    if (v < 0 || v > 0x7FFFFFFFl) { errno = EINVAL; return -1; }
    if (*p) {
	/* username! */
	pw = getpwnam (user);
    } else {
	/* userid! */
	pw = getpwuid ((uid_t) v);
    }
    if (pw) { return pw->pw_uid; }
    return -1;
}

static int getgroup (const char *group)
{
    char *p;
    long v;
    struct group *gr;
    v = strtol (group, &p, 10);
    if (v < 0 || v > 0x7FFFFFFFl) { errno = EINVAL; return -1; }
    if (*p) {
	/* groupname! */
	gr = getgrnam (group);
    } else {
	/* groupid! */
	gr = getgrgid ((gid_t) v);
    }
    if (gr) { return gr->gr_gid; }
    return -1;
}

int main (int argc, char *argv[])
{
    int rc, opt, query = 0, strip = 0, verbose = 0, compress = 0, dirmode;
    char *mode = NULL, *user = NULL, *group = NULL, *tgt = NULL;
    char *gzipcmd = NULL, *stripcmd = NULL;

    set_prog (argc, argv);

    if (!(gzip = which ("gzip")) && errno != ENOENT) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }
    opterr = 0;
    while ((opt = getopt (argc, argv, "+:Qcdg:hm:o:qsvz")) != -1) {
	switch (opt) {
	    case 'Q':
		if (query) { usage ("ambiguous '-Q' option"); }
		query = 2; break;
	    case 'c': break;
	    case 'd': dirmode = 1; break;
	    case 'g':
		if (group) { usage ("ambiguous '-g' option"); }
		group = optarg;
		break;
	    case 'h': usage (NULL); break;
	    case 'm':
		if (mode) { usage ("ambiguous '-m' option"); }
		mode = optarg;
		break;
	    case 'o':
		if (user) { usage ("ambiguous '-o' option"); }
		user = optarg;
		break;
	    case 'q':
		if (query) { usage ("ambiguous '-q' option"); }
		query = 1; break;
	    case 's':
		if (!stripcmd && !(stripcmd = which ("strip"))) {
		    usage ("option '-s' not available; check if the 'strip'"
			   " program is installed!");
		}
		strip = 1; break;
	    case 'v':  verbose = 1; break;
	    case 'z';
		if (!gzipcmd && !(gzipcmd = which ("gzip"))) {
		    usage ("option '-z' not available; check if the 'gzip'"
			   " program is installed!");
		}
		compress = 1; break;
	    case ':': usage ("missing argument for option '%s'", argv[optind]);
	    default: usage ("invalid option '%s'", argv[optind]);
	}
    }
    if (dirmode) {
	rc = install_directory (query, strip, verbose, compress,
				mode, user, group, stripcmd, gzipcmd,
				argc - optind, &argv[optind]);
    } else {
	rc = install_files (query, strip, verbose, compress,
			    mode, user, group, stripcmd, gzipcmd,
			    argc - optind, &argv[optind]);
    }
    return rc;
}

static int install_directory (int query, int strip, int verbose, int compress,
			      const char *mode, const char *user,
			      const char *group, const char *stripcmd,
			      const char *gzipcmd, int filesc, char **files)
{
    uid_t uid = -1;
    gid_t gid = -1;
    int mode = -1, rc, ix;
    char *path = NULL, *bp, *p;
    size_t bufsz = 0;
    if (strip) {
	usage ("option '-s' is invalid when installing a directory");
    }
    if (compress) {
	usage ("option '-z' is invalid when installing a directory");
    }
    if (mode && (mode = getmode (mode)) < 0) {
	usage ("invalid '-m mode' value");
    }
    if (user) {
	if ((rc = getuser (user)) < 0) { user ("invalid '-u user' value"); }
	uid = (uid_t) rc;
    }
    if (group) {
	if ((rc = getgroup (group)) < 0) { user ("invalid '-g group' value"); }
	gid = (gid_t) rc;
    }
    if (filesc < 1) { usage ("missing directory argument"); }
    for (ix = 0; ix < filesc; ++ix) {
	if (!(bp = append (buf, bufsz, NULL, files[ix]))) {
	    err (1, "%me");
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	
