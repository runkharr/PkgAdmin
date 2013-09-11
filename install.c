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
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>

#define PROG "install"

#include "lib/mrmacs.c"
#include "lib/set_prog.c"
#include "lib/lprefix.c"
#include "lib/cuteol.c"
#include "lib/append.c"
#include "lib/err.c"
#include "lib/ask.c"
#include "lib/regfile.c"
#include "lib/which2.c"

static void
usage (const char *fmt, ...)
{
    if (fmt) {
	va_list ual;
	fprintf (stderr, "%s: ", prog);
	va_start (ual, fmt); vfprintf (stderr, fmt, ual); va_end (ual);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("Usage: %s [-clpqQsvz] [-m mode] [-o owner] [-g group] file..."
	    " target"
	    "\n       %s [-qQv] -d [-m mode] [-o owner] [-g group] directory"
	    "\n       %s -h\n"
	    "\nOptions:"
	    "\n  -c"
	    "\n    ignored (kept for compatibility reasons)"
	    "\n  -l"
	    "\n    keep symbolic links (don't follow them during the"
	    " installation but copy"
	    "\n    them directly)"
	    "\n  -p"
	    "\n    If the last file is a directory and any of the source names"
	    " is a relative"
	    "\n    pathname, append source and directory and create the"
	    " directories between"
	    "\n    them as necessary before creating the target file; the"
	    " normal operation"
	    "\n    would consist of concatenating the directory and the last"
	    " element of the"
	    "\n    source path and then creating the file pointed to by the"
	    " constructed path"
	    "\n    directly"
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

static int
getmode (const char *mode)
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

static int
getuser (const char *user)
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

static int
getgroup (const char *group)
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

static int
install_directory (int optflags,
		   const char *mode, const char *user, const char *group,
		   const char *stripcmd, const char *gzipcmd,
		   int filesc, char **files);

static int
install_files (int query, int strip, int verbose, int compress,
	       int tpe, int keeplinks,
	       const char *mode, const char *user, const char *group,
	       const char *stripcmd, const char *gzipcmd,
	       int filesc, char **files);

#define VERBOSE   1
#define QUERYMODE1 2
#define QUERYMODE2 4
#define STRIPFILE 8
#define COMPRESS  16
#define INSTPATH  32
#define KEEPLINKS 64

int main (int argc, char *argv[])
{
    int rc, opt, query = 0, strip = 0, verbose = 0, compress = 0, dirmode;
    int tpe = 0, keeplinks = 0;
    int optflags = 0;
    char *mode = NULL, *user = NULL, *group = NULL;
    char *gzipcmd = NULL, *stripcmd = NULL;

    set_prog (argc, argv);

    opterr = 0;
    while ((opt = getopt (argc, argv, "+:Qcdg:hlm:o:pqsvz")) != -1) {
	switch (opt) {
	    case 'Q':
		if (optflags & (QUERYMODE1|QUERYMODE2)) {
		    usage ("ambiguous '-Q' option");
		}
		optflags |= QUERYMODE2; break;
	    case 'c': break;
	    case 'd': dirmode = 1; break;
	    case 'g':
		if (group) { usage ("ambiguous '-g' option"); }
		group = optarg;
		break;
	    case 'h': usage (NULL); break;
	    case 'l': optflags |= KEEPLINKS; break;
	    case 'm':
		if (mode) { usage ("ambiguous '-m' option"); }
		mode = optarg;
		break;
	    case 'o':
		if (user) { usage ("ambiguous '-o' option"); }
		user = optarg;
		break;
	    case 'p': optflags |= INSTPATH; break;
	    case 'q':
		if (optflags & (QUERYMODE1|QUERYMODE2)) {
		    usage ("ambiguous '-q' option");
		}
		optflags |= QUERYMODE1; break;
	    case 's':
		if (!stripcmd && !(stripcmd = which ("strip"))) {
		    usage ("option '-s' not available; check if the 'strip'"
			   " program is installed!");
		}
		optflags |= STRIPFILE; break;
	    case 'v': optflags |= VERBOSE; break;
	    case 'z':
		if (!gzipcmd && !(gzipcmd = which ("gzip"))) {
		    usage ("option '-z' not available; check if the 'gzip'"
			   " program is installed!");
		}
		optflags |= COMPRESS; break;
	    case ':': usage ("missing argument for option '%s'", argv[optind]);
	    default: usage ("invalid option '%s'", argv[optind]);
	}
    }
    if (dirmode) {
	rc = install_directory (optflags, mode, user, group, stripcmd, gzipcmd,
				argc - optind, &argv[optind]);
    } else {
	rc = install_files (optflags, mode, user, group, stripcmd, gzipcmd,
			    argc - optind, &argv[optind]);
    }
    return rc;
}

static void
msgvke (FILE *out, int flags, const char *format, ...)
{
    va_list args;
    if (flags & VERBOSE) {
	int ke = errno;
	va_start (args, format);
	vfprintf (out, format, args);
	va_end (args);
	fflush (out);
	errno = ke;
    }
}

#define DONE (done (stdout, optflags))
static void
done (FILE *out, int flags)
{
    msgvke (out, flags, " done\n");
}

#define FAILED (failed (stdout, optflags))
static void
failed (FILE *out, int flags)
{
    msgvke (out, flags, " failed\n");
}


static int
is_dir (const char *path, int err)
{
    struct stat sb;
    if (lstat (path, &sb)) { return -1; }
    if (!S_ISDIR (sb.st_mode)) {
	errno = (err ? err : ENOTDIR); return 0;
    }
    return 1;
}

static int
install_directory (int optflags,
		   const char *mode, const char *user, const char *group,
		   const char *stripcmd, const char *gzipcmd,
		   int filesc, char **files)
{
    uid_t uid = geteuid ();
    gid_t gid = getegid ();
    int pmask = 0755, rc, ix;
    char *path = NULL, *p;
    size_t pathsz = 0;
    struct stat sb;
    if (optflags & KEEPLINKS) {
	usage ("option '-l' is invalid when installing a directory");
    }
    if (optflags & USEPATH) {
	usage ("option '-p' is invalid when installing a directory");
    }
    if (optflags & STRIPFILE) {
	usage ("option '-s' is invalid when installing a directory");
    }
    if (optflags & COMPRESS) {
	usage ("option '-z' is invalid when installing a directory");
    }
    if (mode && (pmask = getmode (mode)) < 0) {
	usage ("invalid '-m mode' value");
    }
    if (user) {
	if ((rc = getuser (user)) < 0) { usage ("invalid '-u user' value"); }
	uid = (uid_t) rc;
    }
    if (group) {
	if ((rc = getgroup (group)) < 0) { usage ("invalid '-g group' value"); }
	gid = (gid_t) rc;
    }
    if (filesc < 1) { usage ("missing directory argument"); }
    for (ix = 0; ix < filesc; ++ix) {
	if (!append (path, pathsz, NULL, files[ix])) {
	    err (1, "%me");
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	p = path; if (*p == '/') { ++p; }
	msgvke (stdout, optflags, "Installing directory %s ...", path);
	for (;;) {
	    while (*p && *p != '/') { ++p; }
	    if (!*p) { break; }
	    if (mkdir (path, 0755)) {
		if (errno != EEXIST) { FAILED; cfree (path); return -1; }
		if (is_dir (path, EEXIST) <= 0) {
		    FAILED; cfree (path); return -1;
		}
	    }
	    *p++ = '/';
	}
	if (mkdir (path, pmask)) {
	    if (errno != EEXIST) { FAILED; cfree (path); return -1; }
	    if (is_dir (path, EEXIST) <= 0) {
		FAILED; cfree (path); return -1;
	    }
	}
	if (user || group) {
	    int fd = open (path, O_RDWR, 0);
	    if (fd < 0) { cfree (path); return -1; }
	    if (!fstat (fd, &sb)) {
		FAILED; close (fd); cfree (path); return -1;
	    }
	    if (!S_ISDIR (sb.st_mode)) {
		FAILED; close (fd); cfree (path); errno = EEXIST; return -1;
	    }
	    if (fchown (fd, uid, gid)) {
		FAILED; close (fd); cfree (path); return -1;
	    }
	    if (fchmod (fd, pmask)) {
		FAILED; close (fd); cfree (path); return -1;
	    }
	    close (fd);
	}
	DONE;
    }
    if (path) { cfree (path); }
    return 0;
}

static int
install_files (int query, int strip, int verbose, int compress,
	       int tpe, int keeplinks,
	       const char *mode, const char *user, const char *group,
	       const char *stripcmd, const char *gzipcmd,
	       int filesc, char **files)
{
    int last_is_dir = 0, rc = 0;
    if (filesc < 2) {
	usage ("missing argument(s); please invoke '%s -h' for help, please!",
	       prog);
    }
    last_is_dir = is_dir (files[filesc - 1]);
    if (filesc > 2 && last_is_dir <= 0) {
	if (last_is_dir < 0) {
	    fprintf (stderr, "%s: %s - %s\n", prog, files[filesc - 1],
					      strerror (errno));
	    exit (69);
	}
	usage ("if more than two files are specified, the last one must be a"
	       " directory");
    }
    if (last_is_dir) {
	int ix, errs = 0;
	const char *tgdir = files[filesc - 1], *file;
	for (ix = 0; ix < filesc - 1; ++ix) {
	    file = files[ix];
	    rc = copy_to_dir (query, strip, verbose, compress, tpe,
			      mode, user, group, stripcmd, gzipcmd,
			      file, tgdir);
	    if (rc) { ++errs; }
	}
	if (errs > 0) { rc = -1; }
    } else {
	rc = copy_file (query, strip, verbose, compress,
			mode, user, group, stripcmd, gzipcmd,
			files[filesc - 2], files[filesc - 1]);
    }
    err (1, "not implemented (yet)");
    return 0;
}

static size_t pathsz = 0;
static char *path = NULL;

static int
copy_to_dir (int query, int strip, int verbose, int compress, int tpe,
	     const char *mode, const char *user, const char *group,
	     const char *stripcmd, const char *gzipcmd,
	     const char *file, const char *tgdir)
{
    size_t pz;
    const char *f = file;
    if (!tpe) {
	xxx
    = strlen (tgdir) + strlen (file) + 2;
    if (
