/* install.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** File installer (resembles the program 'install' from the 'coreutils'), but
** allows for (partially) interactive operations ...
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
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PROG "install"

#include "lib/mrmacs.c"
#include "lib/set_prog.c"
#include "lib/lprefix.c"
#include "lib/cuteol.c"
#include "lib/append.c"
/*#include "lib/err.c"*/
#include "lib/ask.c"
#include "lib/regfile.c"
#include "lib/which2.c"

static void
usage (const char *format, ...)
{
    if (format) {
	va_list args;
	fprintf (stderr, "%s: ", prog);
	va_start (args, format); vfprintf (stderr, format, args); va_end (args);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("Usage: %s [-clpqQsvz] [-m mode] [-o owner] [-g group] file..."
	    " target\n"
	    "       %s [-clpqQsvz] [-m mode] [-o owner] [-g group] -t dir"
	    " file...\n"
	    "       %s [-qQv] -d [-m mode] [-o owner] [-g group] directory\n"
	    "       %s -h\n"
	    "\nOptions:"
	    "\n  -c  ignored (kept for compatibility reasons)"
	    "\n  -d  Install a target directory."
	    "\n  -g group"
	    "\n      Change the group of the target file to 'group'."
	    "\n  -h  Display this message and terminate."
	    "\n  -l  keep symbolic links (don't follow them during the"
	    " installation but"
	    "\n      recreate them in the target directory)."
	    "\n  -m mode"
	    "\n      Change the permission-mode of the target file to 'mode'."
	    "\n  -o owner"
	    "\n      Change the ownership of the target file to 'owner'."
	    "\n  -p  If the last file is a directory and any of the source"
	    " names are relative"
	    "\n    pathnames, append source and directory and create the"
	    " directories between"
	    "\n    them as necessary before creating the target file. The"
	    " normal operation"
	    "\n    would be copying of the source files into the target"
	    " directory."
	    "\n  -Q  Don't overwrite an existing file; display an error"
	    "\n  -q  Ask for an existing file being overwritten"
	    " message instead"
	    "\n  -s  Discard the symbols from a target (object-)file. This"
	    " operation is not"
	    "\n      possible without the 'strip' program being installed."
	    "\n  -v  Display a message for each file being installed."
	    "\n  -z  Compress a target file. This operation is not possible"
	    " without the 'gzip'"
	    "\n      program being installed."
	    "\n",
	    prog, prog, prog, prog);
    exit (0);
}

static void emesg (int exit_code, const char *format, ...)
{
    va_list args;
    fprintf (stderr, "%s: ", prog);
    va_start (args, format); vfprintf (stderr, format, args); va_end (args);
    fputs ("\n", stderr);
    if (exit_code > 0) { exit (exit_code); }
}

static const char *current_error (void)
{
    return strerror (errno);
}

static void vout (const char *format, ...)
{
    int ec = errno;
    va_list args;
    va_start (args, format); vfprintf (stdout, format, args); va_end (args);
    fflush (stdout);
    errno = ec;
}

static int
digit (int c, int b)
{
    int res = -1;
    if (b >= 2 && b <= 36) {
	switch (c) {
	    case '0': res = 0; break; case '1': res = 1; break;
	    case '2': res = 2; break; case '3': res = 3; break;
	    case '4': res = 4; break; case '5': res = 5; break;
	    case '6': res = 6; break; case '7': res = 7; break;
	    case '8': res = 8; break; case '9': res = 9; break;
	    case 'A': case 'a': res = 10; break;
	    case 'B': case 'b': res = 11; break;
	    case 'C': case 'c': res = 12; break;
	    case 'D': case 'd': res = 13; break;
	    case 'E': case 'e': res = 14; break;
	    case 'F': case 'f': res = 15; break;
	    case 'G': case 'g': res = 16; break;
	    case 'H': case 'h': res = 17; break;
	    case 'I': case 'i': res = 18; break;
	    case 'J': case 'j': res = 19; break;
	    case 'K': case 'k': res = 20; break;
	    case 'L': case 'l': res = 21; break;
	    case 'M': case 'm': res = 22; break;
	    case 'N': case 'n': res = 23; break;
	    case 'O': case 'o': res = 24; break;
	    case 'P': case 'p': res = 25; break;
	    case 'Q': case 'q': res = 26; break;
	    case 'R': case 'r': res = 27; break;
	    case 'S': case 's': res = 28; break;
	    case 'T': case 't': res = 29; break;
	    case 'U': case 'u': res = 30; break;
	    case 'V': case 'v': res = 31; break;
	    case 'W': case 'w': res = 32; break;
	    case 'X': case 'x': res = 33; break;
	    case 'Y': case 'y': res = 34; break;
	    case 'Z': case 'z': res = 35; break;
	    default: break;
	}
    }
    if (res >= b) { res = -1; }
    return res;
}

static unsigned long x2ul (const char *s, const char **_p, int base)
{
    int dg;
    unsigned long res = 0;
    if (base == 0) {
	if (*s == '0') {
	    base = 8; if (s[1] == 'X' || s[1] == 'x') { base = 16; s += 2; }
	} else {
	    base = 10;
	    while ((dg = digit (*s, base)) >= 0) {
		res = res * base + (unsigned long) dg; ++s;
	    }
	    if (*s || *s != '#') { *_p = s; return res; }
	    if (*s == '#') {
		if (res < 2l || res > 36l) { *_p = s; return res; }
		base = (int) res; res = 0;
	    }
	}
    }
    while ((dg = digit (*s, base)) >= 0) {
	res = res * base + (unsigned long) dg; ++s;
    }
    *_p = s; return res;
}


static int
install_directory (int optflags,
		   const char *mode, const char *user, const char *group,
		   const char *stripcmd, const char *gzipcmd,
		   int filesc, char **files);

static int install_files (int opt_flags, const char *mode,
			  const char *user, const char *group,
			  const char *stripcmd, const char *gzipcmd,
			  const char *tdir, int filesc, char **files);

#define OPT_VERBOSE (1)
#define OPT_QUERY1 (2)
#define OPT_QUERY2 (4)
#define OPT_STRIP (8)
#define OPT_COMPRESS  (16)
#define OPT_KEEPLINK (32)
#define OPT_INSTPATH  (64)
#define OPT_RMDST (128)

int main (int argc, char *argv[])
{
    int rc, opt, dirmode;
    int optflags = 0;
    char *mode = NULL, *user = NULL, *group = NULL, *tdir = NULL;
    char *gzipcmd = NULL, *stripcmd = NULL;

    set_prog (argc, argv);

    opterr = 0;
    while ((opt = getopt (argc, argv, "+:Qcdg:hlm:o:pqst:vz")) != -1) {
	switch (opt) {
	    case 'Q':
		/* Query mode 1 - Don't overwrite existing files */
		if (optflags & (OPT_QUERY1|OPT_QUERY2)) {
		    usage ("The options '-%c' and '-q' are mutually exclusive",
			   opt);
		}
		optflags |= OPT_QUERY2; break;
	    case 'c':
		/* Only for compatibility reasons, but ignored */
		break;
	    case 'd': dirmode = 1; break;
	    case 'g':
		/* Set the group for the installed files ... */
		if (group) { usage ("ambiguous '-g' option"); }
		group = optarg;
		break;
	    case 'h':
		/* Write a usage message and terminate. */
		usage (NULL); break;
	    case 'l':
		/* Copy symbolic links as symbolic links and not the files
		** they point to ...
		*/
		optflags |= OPT_KEEPLINK; break;
	    case 'm':
		/* Set the permission mask - either numerical or symbolical */
		if (mode) { usage ("ambiguous '-m' option"); }
		mode = optarg;
		break;
	    case 'o':
		/* Set the owner of the installed files ... */
		if (user) { usage ("ambiguous '-o' option"); }
		user = optarg;
		break;
	    case 'p':
		/* Keep the path of the source file (if it is relative and
		** contains no '../') when copying it to the destination. This
		** may lead to the automatical creation of further sub-
		** directories which will have a default permission mask of
		** (0777 & ~ <current user's umask>) ...
		** This option is currently not implemented!
		*/
		optflags |= OPT_INSTPATH; break;
	    case 'q':
		/* Query mode 2 - Ask for overwriting existing files */
		if (optflags & (OPT_QUERY1|OPT_QUERY2)) {
		    usage ("The options '-%c' and '-Q' are mutually exclusive",
			   opt);
		}
		optflags |= OPT_QUERY1; break;
	    case 's':
		/* Strip a regular file if it is a program or shared library
		** file ...
		*/
		if (stripcmd) {
		    usage ("Ambiguous ose of the option '-%c'", opt);
		}
		if (!stripcmd && !(stripcmd = which ("strip"))) {
		    usage ("option '-s' not available; check if the 'strip'"
			   " program is installed!");
		}
		optflags |= OPT_STRIP; break;
	    case 't':
		/* Set a target directory explicitely ... */
		if (dirmode) {
		    usage ("The options '-d' and '-t' are mutually exclusive");
		}
		if (tdir) { usage ("Ambiguous use of the option '-%c'", opt); }
		tdir = optarg; break;
	    case 'v':
		/* Display messages about the progress of an install operation
		** namly the file installed, an error message if something
		** doesn't work, and a complettion message if the file could
		** be successfully installed ...
		*/
		optflags |= OPT_VERBOSE; break;
	    case 'z':
		/* Compress a regular file (after installing it). This is
		** mainly used for documentation (like manual pages) ...
		*/
		if (gzipcmd) {
		    usage ("Ambiguous ose of the option '-%c'", opt);
		}
		if (!gzipcmd && !(gzipcmd = which ("gzip"))) {
		    usage ("option '-z' not available; check if the 'gzip'"
			   " program is installed!");
		}
		optflags |= OPT_COMPRESS; break;
	    case ':': usage ("missing argument for option '-%c'", optopt);
	    default: usage ("invalid option '-%c'", optopt);
	}
    }
    if (dirmode) {
	rc = install_directory (optflags, mode, user, group, stripcmd, gzipcmd,
				argc - optind, &argv[optind]);
    } else {
	rc = install_files (optflags, mode, user, group, stripcmd, gzipcmd,
			    tdir, argc - optind, &argv[optind]);
    }
    return rc;
}

#define DONE do { if (verbose) vout (" done\n"); } while (0)

#define FAILED \
    do { if (verbose) vout (" failed (%s)\n", current_error ()); } while (0)


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

static uid_t get_user (const char *user);
static gid_t get_group (const char *group);
static mode_t get_mode (mode_t st_mode, const char *mode);
static mode_t get_umask (void);

/* Install a list of directories ... */
static int install_directory (int optflags, const char *mode,
			      const char *user, const char *group,
			      const char *stripcmd, const char *gzipcmd,
			      int filesc, char **files)
{
    uid_t uid; gid_t gid;
    int pmask = 0777 & ~ get_umask (), ix;
    int verbose = (optflags & OPT_VERBOSE) != 0;
    char *path = NULL, *p;
    size_t pathsz = 0;
    struct stat sb;
    if (optflags & OPT_KEEPLINK) {
	usage ("option '-l' is invalid when installing a directory");
    }
    if (optflags & OPT_INSTPATH) {
	usage ("option '-p' is invalid when installing a directory");
    }
    if (optflags & OPT_STRIP) {
	usage ("option '-s' is invalid when installing a directory");
    }
    if (optflags & OPT_COMPRESS) {
	usage ("option '-z' is invalid when installing a directory");
    }
    if ((pmask = get_mode (pmask, mode)) < 0) {
	usage ("invalid permission mask");
    }
    if (user) {
	if ((uid = get_user (user)) < 0) { usage ("invalid user"); }
    }
    if (group) {
	if ((gid = get_group (group)) < 0) { usage ("invalid group"); }
    }
    if (filesc < 1) { usage ("missing directory argument"); }
    for (ix = 0; ix < filesc; ++ix) {
	if (!append (path, pathsz, NULL, files[ix])) {
	    emesg (1, "%s", current_error ());
	}
	p = path; if (*p == '/') { ++p; }
	if (verbose) { vout ("Installing directory %s ...", path); }
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
	pmask &= ~ S_IFMT;
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

static int copy_to_dir (int opt_flags, const char *mode,
			const char *user, const char *group,
			const char *stripcmd, const char *gzipcmd,
			const char *file_path, const char *tdir);
static int copy_file (int opt_flags,
		      const char *mode, const char *user, const char *group,
		      const char *stripcmd, const char *gzipcmd,
		      const char *src, const char *dst);

static int
install_files (int opt_flags, 
	       const char *mode, const char *user, const char *group,
	       const char *stripcmd, const char *gzipcmd, const char *tdir,
	       int filesc, char **files)
{
    int last_is_dir = 0, rc = 0;
    if (filesc < 2) {
	usage ("Missing argument(s). Try '%s -h' for help, please!",
	       prog);
    }
    if (tdir) {
	if (! is_dir (tdir, 0)) { usage ("'%s' is no directory", tdir); }
    } else if (! tdir) {
	last_is_dir = is_dir (files[filesc - 1], 0);
	if (filesc > 2 && last_is_dir <= 0) {
	    if (last_is_dir < 0) {
		emesg (69, "'%s' - %s\n", files[filesc - 1], current_error ());
	    }
	    usage ("If more than two files are specified, the last one must"
		   " be a directory");
	}
	tdir = files[filesc - 1]; --filesc;
    }
    if (tdir) {
	int ix, errs = 0;
	const char *file;
	for (ix = 0; ix < filesc; ++ix) {
	    file = files[ix];
	    rc = copy_to_dir (opt_flags, mode, user, group,
			      stripcmd, gzipcmd, file, tdir);
	    if (rc) { ++errs; }
	}
	if (errs > 0) { rc = -1; }
    } else {
	rc = copy_file (opt_flags, mode, user, group, stripcmd, gzipcmd,
			files[filesc - 2], files[filesc - 1]);
    }
    return rc;
}

static int do_cmd (const char *cmdprog, const char *file)
{
    extern char **environ;
    pid_t pid;
    const char *cmd[] = { cmdprog, file, NULL };
    switch (pid = fork ()) {
	case -1:	/* FAILED */
	    emesg (0, "fork() failed - %s", current_error ());
	    return -1;
	case 0:  {	/* CHILD */
	    execve (cmd[0], (char **) cmd, environ);
	    exit (99);
	}
	default: {	/* PARENT */
	    int wstat;
	    waitpid (pid, &wstat, 0);
	    if (WIFSIGNALED (wstat)) {
		emesg (0, "'%s' - %s", cmdprog, strsignal (WTERMSIG (wstat)));
		return -1;
	    }
	    if (WIFEXITED (wstat)) {
		int exit_code = WEXITSTATUS (wstat);
		if (exit_code == 0) { return 0; }
		emesg (0, "'%s' terminated with code %d", exit_code);
		return -1;
	    }
	    return -1;
	}
    }
}

/* Convert the string 'user' into a user id. The string may be given either
** as a numerical user id (which is returned directly) or as a user name which
** is searched in the 'passwd' user database ...
*/
static uid_t get_user (const char *user)
{
    const char *p;
    uid_t res;
    unsigned long lv, max;
    if (! user || *user == '\0') { return 0; }
    lv = x2ul (user, &p, 0);
    if (p && *p == '\0') {
	max = ((1l << (8 * sizeof(uid_t) - 1)) - 1l);
	if (lv > max) { return -1; }
	res = (uid_t) lv;
    } else {
	struct passwd *pwe = getpwnam (user);
	if (! pwe) { int ec = errno; errno = ec; return -1; }
	res = pwe->pw_uid;
    }
    return res;
}

/* Convert the string 'group' into a group id. The string may be given either
** as a numerical group id (which is returned directly) or as a group name
** which is searched in the 'group' database ...
*/
static gid_t get_group (const char *group)
{
    const char *p;
    gid_t res;
    unsigned long lv, max;
    if (! group || *group == '\0') { return 0; }
    lv = x2ul (group, &p, 0);
    if (p && *p == '\0') {
	max = ((1l << (8 * sizeof(gid_t) - 1)) - 1l);
	if (lv > max) { return -1; }
	res = (gid_t) lv;
    } else {
	struct group *gre = getgrnam (group);
	if (! gre) { int ec = errno; errno = ec; return -1; }
	res = gre->gr_gid;
    }
    return res;
}

/* 'get_mode()' converts the string argument 'mode' into a permission mask
** which then is returned. 'mode' can be supplied either as an octal number
** consisting of upto 4 digits or as symbolic mask (like in the shell command
** 'chmod'). This function returns the mode on success. On error, it returns
** 'S_IFMT'. I chose these return values because a mode of 0 (all bits reset)
** is a valid mode and must be distinguished from an error value and the only
** way to do this is to use either one or all of the 'S_IFMT' bits, and i chose
** the first option. The mode may be given as modification of an existing
** permission mask. This existing mask is that one of the source file, supplied
** via the 'struct stat *' first argument ...
*/
static
mode_t
get_mode (mode_t st_mode, const char *mode)
{
    const char *p, *x;
    mode_t res;
    int what_mode = 0;
    unsigned long lv, max = 07777;
    /* No permission mask specified ==> return 0 ... */
    if (mode && *mode != '\0') {
	res = st_mode & (S_IRWXU|S_IRWXG|S_IRWXO);
	return res | S_IFREG;
    }
    /* Try a numerical value (with an optional prefix of '+', '-' or '=') */
    if (*mode == '+' || *mode == '-' || *mode == '=') {
	what_mode = *mode; lv = x2ul (&mode[1], &x, 8);
    } else {
	lv = x2ul (mode, &x, 8);
    }
    if (! x || *x == '\0') {
	/* The numerical conversion was successful, meaning: the 'mode' was
	** given in a numerical fashion ...
	*/
	if (lv > max) { return S_IFMT; }
	res = st_mode & (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO);
	switch (what_mode) {
	    case '+': /* add mode */ res |= (mode_t) lv;
	    case '-': /* sub mode */ res &= ~ (mode_t) lv;
	    default:  /* set mode */ res = (mode_t) lv;
	}
    } else {
	mode_t xmask = 0;
	int xstate = 0, what_mask = 0;
	res = st_mode; p = mode;
	while (*p) {
	    if (*p == '+' || *p == '-' || *p == '=') {
		/* The setting mode flags are allowed as first character or
		** after a 'u', 'g' or 'o' ...
		*/
		if (xstate > 1) { goto ERROR; }
		what_mode = *p;
		if (xstate == 0) {
		    /* No preceding 'u', 'g' or 'o' means: set for all ... */
		    what_mask = 15;
		}
		/* Expect a 'r', 'w', 'x', 's' or 't' as next character ... */
		xstate = 2;
	    } else if (*p == 'u' || *p == 'g' || *p == 'o') {
		/* 'u', 'ug' 'ugo', 'g', 'go', 'o' and any permutations are
		** allowed in this state, but neither of these characters can
		** occur after a 'r', 'w', 'x', 's' or 't' ...
		*/
		if (xstate > 1) { goto ERROR; }
		what_mask |= (*p == 'u' ? 1 : *p == 'g' ? 2 : 4);
		xstate = 1;
	    } else if (*p == 'r' || *p == 'w' || *p == 'x') {
		/* In state 2 or 3, only 'r', 'w', 'x' may occur here ... */
		if (xstate < 2) { goto ERROR; }
		switch (*p) {
		    case 'r':
			if (what_mask & 1) { xmask |= S_IRUSR; }
			if (what_mask & 2) { xmask |= S_IRGRP; }
			if (what_mask & 4) { xmask |= S_IROTH; }
			break;
		    case 'w':
			if (what_mask & 1) { xmask |= S_IWUSR; }
			if (what_mask & 2) { xmask |= S_IWGRP; }
			if (what_mask & 4) { xmask |= S_IWOTH; }
			break;
		    case 'x':
			if (what_mask & 1) { xmask |= S_IXUSR; }
			if (what_mask & 2) { xmask |= S_IXGRP; }
			if (what_mask & 4) { xmask |= S_IXOTH; }
			break;
		}
		xstate = 3;
	    } else if (*p == 's') {
		/* In state 2 or 3, 's' may occur here, but not together with
		** 'o' ...
		*/
		if (xstate < 2 || (what_mask & 4) != 0) { goto ERROR; }
		if (what_mask & 1) { xmask |= S_ISUID; }
		if (what_mask & 2) { xmask |= S_ISGID; }
		xstate = 3;
	    } else if (*p == 't') {
		/* In state 2 or 3, 't' may occur here, but only if there was
		** no leading 'u', 'g' or 'o' ...
		*/
		if (xstate < 2 || what_mask != 15) { goto ERROR; }
		xmask |= S_ISVTX;
		xstate = 3;
	    } else if (*p == ',') {
		/* A ',' in state 3 (and ONLY here) terminates the current
		** mask. This means that all the mode masks must be tranferred
		** (added, subtracted or set) to the resulting mode and all
		** variables (xmask, what_mask, xstate and what_mode) reset
		** for the next round ...
		*/
		if (xstate < 3) { goto ERROR; }
		/* Transfer the modes ... */
		switch (what_mode) {
		    case '+': /* add mode */ res |= xmask; break;
		    case '-': /* sub mode */ res &= ~ xmask; break;
		    case '=': /* set mode */ res = xmask; break;
		}
		xmask = 0; xstate = 0; what_mask = 0; what_mode = 0;
	    } else {
		goto ERROR;
	    }
	    ++p;
	}
	/* One 'r', 'w', 'x', 's' or 't' had to be set for the mode to be
	** valid ...
	*/
	if (xstate < 3) { goto ERROR; }
	switch (what_mode) {
	    case '+': /* add mode */ res |= xmask; break;
	    case '-': /* sub mode */ res &= ~ xmask; break;
	    case '=': /* set mode */ res = xmask; break;
	}
	/* No need to reset the state variables here, as this is the last
	** round ...
	**
	**xmask = 0; xstate = 0; what_mask = 0; what_mode = 0;
	*/
    }
    return res | S_IFREG;
ERROR:
    errno = EINVAL; return S_IFMT;
}

static mode_t get_umask (void)
{
    mode_t um = umask (0022);
    (void) umask (um);
    return um;
}

static int tmp_id1 = 0;
static int tmp_id2 = -1;

/* Generate a "pseudo" random number (either by using the standard lib's
** utility function 'rand_r()' (with a seed generated from the current time)
** or via the Linux pseudo device '/dev/urandom' ...
*/
static int get_rand (void)
{
    int fd;
    if ((fd = open ("/dev/urandom", O_RDONLY)) < 0) {
	time_t t; unsigned int seed;
	time (&t);
	if (sizeof(t) > sizeof(seed)) {
	    unsigned long tx = (unsigned long) t;
	    tx = (tx >> 15) | (tx << (8 * sizeof(tx) - 15));
	    seed = (unsigned int) (tx ^ (tx >> (8 * sizeof(tx) / 2)));
	} else {
	    seed = (unsigned int) t;
	    seed = (seed >> 13) | (seed << (8 * sizeof(seed) - 13));
	}
	return rand_r (&seed);
    } else {
	unsigned int rv = 0;
	unsigned char buf[sizeof(tmp_id2)], *p;
	unsigned char ov, bb; size_t sz = sizeof(buf);
	if (read (fd, &ov, 1) != 1) { close (fd); return -1; }
	ov %= 17; p = buf;
	while (ov-- > 0) { read (fd, &bb, 1); }
	if (read (fd, buf, sizeof(buf)) != sizeof (buf)) {
	    close (fd); return -1;
	}
	close (fd);
	p = buf; ov = bb;
	while (ov > 0) {
	    if ((ov & 1) != 0) {
		if ((size_t) (p - buf) >= sizeof(buf)) {
		    p = buf; bb = *p; *p++ = buf[sizeof(buf) - 1];
		    buf[sizeof(buf) - 1] = bb;
		} else {
		    bb = *p; *p = p[1]; *++p = bb;
		}
	    } else {
		if ((size_t) (p - buf) >= sizeof(buf)) {
		    p = buf; bb = *p++;
		    bb = (bb << 3) | (bb > (8 * sizeof(bb) - 3));
		    buf[sizeof(buf) - 1] ^= bb;
		} else {
		    bb = p[1];
		    bb = (bb << 3) | (bb >> (8 * sizeof(bb) - 3));
		    *p++ ^= bb;
		}
	    }
	    ov >>= 1;
	}
	sz = sizeof (buf); p = buf;
	while (sz-- > 0) { rv = (rv << 8) | (unsigned int) *p++; }
	if (rv > RAND_MAX) { rv %= RAND_MAX; }
	return (int) rv;
    }
}

static int expand_buffer (char **_buf, size_t *_bufsz, size_t newsz)
{
    if (newsz > *_bufsz) {
	char *newp = (char *) realloc (*_buf, newsz);
	if (! newp) { return -1; }
	*_buf = newp; *_bufsz = newsz;
    }
    return 0;
}

static char *tmpf = NULL;
static size_t tmpfsz = 0;

static const char *gen_tempname (const char *file)
{
    int pl;
    size_t sz = 0;
    const char *p = strrchr (file, '/');
    if (p) { sz = (size_t) (p - file); p = file; } else { p = "."; sz = 1; }
    pl = (int) sz;
    sz += strlen (prog) + sizeof(tmp_id1)*3 + sizeof(tmp_id2)*2 + 4;
    if (expand_buffer (&tmpf, &tmpfsz, sz + 1)) { return NULL; }
    if (tmp_id1 == 0) { tmp_id1 = (int) getpid (); }
    if (tmp_id2 < 0) { tmp_id2 = get_rand (); }
    snprintf (tmpf, tmpfsz, "%.*s/%s.%d%%%x", pl, p, prog, tmp_id1, tmp_id2);
    return tmpf;
}

static int save_file (const char *file)
{
    const char *saved_as = gen_tempname (file);
    if (! saved_as) { return -1; }
    return rename (file, saved_as);
}

static int restore_file (const char *file)
{
    const char *saved_as = gen_tempname (file);
    if (! saved_as) { return -1; }
    if (unlink (file)) {
	if (errno != ENOENT) { return -1; }
    }
    return rename (saved_as, file);
}

static int remove_saved (const char *file)
{
    const char *saved_as = gen_tempname (file);
    if (! saved_as) { return -1; }
    return unlink (saved_as);
}

static int rc_check (int rc, int flags, const char *fn, const char *file)
{
    int ec = errno;
    int verbose = (flags & OPT_VERBOSE) != 0;
    int rmdst = (flags & OPT_RMDST) != 0;
    if (rc == 0) { return 0; }
    if (rmdst) { restore_file (file); }
    if (verbose) { vout (" %s() failed (%s)\n", strerror (ec)); }
    errno = ec;
    return -1;
}

static int set_ownermode (int opt_flags, mode_t pmask, uid_t uid, gid_t gid,
			  const char *file)
{
    int rc = 0;

    /* Setting either 'owner', 'group' or both ... */
    if (uid > 0 || gid > 0) {
	/* Because 'chown()' sets both (user and group) simultaneously, each of
	** them must be set to a default, if the corresponding argument ('user'
	** or 'group') was not specified (NULL or empty string). The manual
	** page of 'chown()' states that this default value is '-1' (not
	** setting the corresponding property) ...
	*/
	if (uid == 0) { uid = -1; }
	if (gid == 0) { gid = -1; }
	rc = rc_check (chown (file, uid, gid), opt_flags, "chown", file);
	if (rc) { return rc; }
    }

    /* Setting the permission mask can be suppressed by setting it to an empty
    ** value (if this is requested) ...
    */
    if (pmask) {
	rc = rc_check (chmod (file, pmask), opt_flags, "chmod", file);
	if (rc) { return rc; }
    }
    return rc;
}

static int recreate_special (int opt_flags, const char *mode,
			     const char *user, const char *group,
			     struct stat *sp, const char *file)
{
    int rc, ft, verbose = (opt_flags & OPT_VERBOSE) != 0;
    if (verbose) {
	const char *ft = (S_ISBLK (sp->st_mode) ? "block device " :
			  S_ISCHR (sp->st_mode) ? "character device " :
			  S_ISFIFO (sp->st_mode) ? "named pipe " :
			  S_ISSOCK (sp->st_mode) ? "local socket " :
						   "");
	vout ("Installing %s%s ...", ft, file);
    }
    ft = sp->st_mode & S_IFMT;
    switch (ft) {
	case S_IFBLK: case S_IFCHR: case S_IFIFO: case S_IFSOCK: {
	    uid_t uid; gid_t gid; mode_t pmask;
	    if ((uid = get_user (user)) < 0) {
		if (verbose) { vout (" failed (invalid user)\n"); }
		return -1;
	    }
	    if ((gid = get_group (group)) < 0) {
		if (verbose) { vout (" failed (invalid group)\n"); }
		return -1;
	    }
	    if ((pmask = get_mode (sp->st_mode, mode)) == S_IFMT) {
		if (verbose) { vout (" failed (invalid mode)\n"); }
		return -1;
	    }
	    if ((opt_flags & OPT_RMDST) != 0) {
		if (save_file (file)) {
		    vout (" rename() failed (%s)\n", current_error ());
		    return -1;
		}
	    }
	    rc = rc_check (mknod (file, (ft | pmask), sp->st_rdev), opt_flags,
			   "mknod", file);
	    if (rc) { return rc; }
	    rc = set_ownermode (opt_flags, pmask, uid, gid, file);
	    if (rc) { return rc; }
	    if ((opt_flags & OPT_RMDST) != 0) { remove_saved (file); }
	    if (verbose) { vout (" done\n"); }
	    return 0;
	}
	default:
	    errno = EINVAL; return -1;
    }
}


static char *ltarget = NULL;
size_t ltargetsz = 0;

static int recreate_link (int opt_flags, const char *mode,
			  const char *user, const char *group,
			  const char *src, struct stat *sp, const char *dst)
{
    uid_t uid; gid_t gid; mode_t pmask;
    int rc, verbose = (opt_flags & OPT_VERBOSE) != 0;
    size_t sz = (size_t) sp->st_size;
    if (verbose) { vout ("Installing symbolic link %s ...", dst); }
    if (expand_buffer (&ltarget, &ltargetsz, sz + 1)) { return -1; }
    memset (ltarget, 0, ltargetsz);
    if (readlink (src, ltarget, ltargetsz - 1) < 0) {
	if (verbose) { vout (" readlink() failed (%s)\n", current_error ()); }
	return -1;
    }
    if ((uid = get_user (user)) < 0) {
	if (verbose) { vout (" failed (invalid user)\n"); }
	return -1;
    }
    if ((gid = get_group (group)) < 0) {
	if (verbose) { vout (" failed (invalid group)\n"); }
	return -1;
    }
    if ((pmask = get_mode (sp->st_mode, mode)) == S_IFMT) {
	if (verbose) { vout (" failed (invalid mode)\n"); }
	return -1;
    }
    if ((opt_flags & OPT_RMDST) != 0) {
	if (save_file (dst)) {
	    vout (" rename() failed (%s)\n", current_error ()); return -1;
	}
    }
    rc = rc_check (symlink (ltarget, dst), opt_flags, "symlink", dst);
    if (rc) { return rc; }
    rc = set_ownermode (opt_flags, pmask, uid, gid, dst);
    if (rc) { return rc; }
    if ((opt_flags & OPT_RMDST) != 0) { remove_saved (dst); }
    if (verbose) { vout (" done\n"); }
    return 0;
}

static int copy_to (int opt_flags, const char *mode,
		    const char *user, const char *group,
		    const char *src, struct stat *sp, const char *dst)
{
    uid_t uid; gid_t gid; mode_t pmask;
    int rc, verbose = (opt_flags & OPT_VERBOSE) != 0, pc = 0;
    int rm_dst = (opt_flags & OPT_RMDST) != 0;
    FILE *sfp, *dfp;
    char buf[8192]; size_t rlen, wlen;
    if (verbose) { vout ("Installing file %s as %s", src, dst); }
    if ((uid = get_user (user)) < 0) {
	if (verbose) { vout (" failed (invalid user)\n"); }
	return -1;
    }
    if ((gid = get_group (group)) < 0) {
	if (verbose) { vout (" failed (invalid group)\n"); }
	return -1;
    }
    if ((pmask = get_mode (sp->st_mode, mode)) == S_IFMT) {
	if (verbose) { vout (" failed (invalid mode)\n"); }
	return -1;
    }
    if (! (sfp = fopen (src, "rb"))) {
	if (verbose) { vout (" ... failed (%s)\n", current_error ()); }
	return -1;
    }
    if (rm_dst) {
	if (save_file (dst)) {
	    vout (" rename() failed (%s)\n", current_error ()); return -1;
	}
    }
    if (! (dfp = fopen (dst, "wb"))) {
	int ec = errno;
	fclose (sfp); restore_file (dst);
	if (verbose) { vout (" ... failed (%s)\n", strerror (ec)); }
	return -1;
    }
    wlen = 0;
    while ((rlen = fread (buf, 1, sizeof(buf), sfp)) > 0) {
	if ((wlen = fwrite (buf, 1, rlen, dfp)) != rlen) { break; }
	if (verbose) { 
	    const char *alive = "|/-\\"; /* alt: ".oOo" (heartbeat alike) */
	    vout ("%c\b", alive[pc]); pc = (pc + 1) % strlen (alive);
	}
    }
    if (ferror (sfp) || ferror (dfp)) {
	int ec = errno; 
	fclose (dfp); fclose (sfp); restore_file (dst);
	if (verbose) { vout (" ... failed (%s)\n", strerror (ec)); }
	errno = ec; return -1;
    }
    rc = set_ownermode (opt_flags, pmask, uid, gid, dst);
    if (rc) { return rc; }
    if (rm_dst) { remove_saved (dst); }
    if (verbose) { vout (" done\n"); }
    return 0;
}

/* Was ist zu tun?
** 1. Feststellen, um was fürt einen Typ von Datei es sich handelt.
** 2. Reguläre Dateien kopieren.
** 3. Special Files (Character- und Block-Devices, Named Pipes, Sockets) neu
**    anlegen mit den gleichen Permissions und der gleichen Ownership/Group
**    wie die Quelldatei.
** 3. Symbolic Links neu anlegen, mit dem gleichen Pfad wie der ursprüngliche
**    Link.
** 4. Verzeichnisse rekursiv kopieren.
** 5. Zusatzoperationen wie 'strip' oder 'compression' ausführen.
*/
static int copy_file (int opt_flags,
		      const char *mode, const char *user, const char *group,
		      const char *stripcmd, const char *gzipcmd,
		      const char *src, const char *dst)
{
    int rc = 0, allow_xcmd = 0;
    struct stat sb;
    if (lstat (dst, &sb) == 0) {
	if ((opt_flags & OPT_QUERY1) != 0) {
	    errno = EEXIST; return -1;
	} else if ((opt_flags & OPT_QUERY2) != 0) {
	    if (! ask (-1, "Ok to overwrite %s", dst)) {
		errno = EEXIST; return -1;
	    }
	}
	opt_flags |= OPT_RMDST;
    }
    if (lstat (src, &sb)) { return -1; }
    switch (sb.st_mode & S_IFMT) {
	case S_IFSOCK:	/* Unix Domain Socket */
	case S_IFBLK:	/* Block device */
	case S_IFCHR:	/* Character device */
	case S_IFIFO:	/* Named Pipe (FiFo device) */
	    rc = recreate_special (opt_flags, mode, user, group, &sb, dst);
	    break;
	case S_IFLNK:	/* Symbolic link */
	    if ((opt_flags & OPT_KEEPLINK) != 0) {
		rc = recreate_link (opt_flags, mode, user, group, src,
				    &sb, dst);
	    } else {
		allow_xcmd = 1;
		rc = copy_to (opt_flags, mode, user, group, src, &sb, dst);
	    }
	    break;
	case S_IFREG:
	    allow_xcmd = 1;
	    rc = copy_to (opt_flags, mode, user, group, src, &sb, dst);
	    break;
	case S_IFDIR:	/* Directory */
	    /* Not implemented (yet) ... */
	    errno = EINVAL; return -1;
	default:
	    errno = EINVAL; return -1;
    }
    if (rc) {
	if ((opt_flags & OPT_VERBOSE) == 0) {
	    emesg (0, "Installing %s failed - %s\n", src, current_error ());
	}
	return rc;
    }
    if (! allow_xcmd) { return 0; }
    if ((opt_flags & OPT_STRIP) != 0) {
	rc = do_cmd (stripcmd, dst);
	if (rc) { emesg (0, "WARNING! '%s %s' failed", stripcmd, dst); rc = 0; }
    }
    if ((opt_flags & OPT_COMPRESS) != 0) {
	rc = do_cmd (gzipcmd, dst);
	if (rc) { emesg (0, "WARNING! '%s %s' failed", stripcmd, dst); rc = 0; }
    }
    return rc;
}


static size_t pathsz = 0;
static char *path = NULL;

static int copy_to_dir (int opt_flags, const char *mode,
			const char *user, const char *group,
			const char *stripcmd, const char *gzipcmd,
			const char *file_path, const char *tdir)
{
    const char *file;
    /*int verbose = (opt_flags & OPT_VERBOSE) != 0;*/
    size_t tdlen = strlen (tdir), sz;
    file = strrchr (file_path, '/');
    if (file) { ++file; } else { file = file_path; }
    while (tdlen > 0 && tdir[tdlen - 1] == '/') { --tdlen; }
    sz = tdlen + strlen (file) + 2;
    if (expand_buffer (&path, &pathsz, sz)) { return -1; }
    snprintf (path, pathsz, "%.*s/%s", (int) tdlen, tdir, file);
    return copy_file (opt_flags, mode, user, group, stripcmd, gzipcmd,
		      file_path, path);
}

#if 0
static int
parse_mode (const char *mode)
{
    enum alphabet { plus = 0, minus = 1, equal = 2, letter_a = 3,
		    letter_u = 4, letter_g = 5, letter_o = 6, letter_r = 7,
		    letter_w = 8, letter_x = 9, letter_s = 10, letter_t = 11,
		    comma = 12, eos = 13, other = 14 };
    const int st_columns = other + 1;
    enum state { initial = 0, next = 1, pgoup = 2, set_perms = 3, get_mask = 4,
		 get_mask1 = 5, finish = 6, error = 7 };
    const int st_rows = get_mask1 + 1;

    enum state state_transition[st_rows][st_columns] = {
	/* initial */
	{ /* + */ get_mask, /* - */ get_mask, /* = */ get_mask, /* a */ pgroup,
	  /* u */ pgroup, /* g */ pgroup, /* o */ pgroup, /* r */ error,
	  /* w */ error, /* x */ error, /* s */ error, /* t */ error,
	  /* , */ error, /* EOS */ error, /* other */ error
	},
	/* next */
	{ /* + */ get_mask, /* - */ get_mask, /* = */ get_mask, /* a */ pgroup,
	  /* u */ pgroup, /* g */ pgroup, /* o */ pgroup, /* r */ error,
	  /* w */ error, /* x */ error, /* s */ error, /* t */ error,
	  /* , */ error, /* EOT */ finish, /* other */ error
	},
	/* pgroup */
	{ /* + */ get_mask, /* - */ get_mask, /* = */ get_mask, /* a */ error,
	  /* u */ pgroup, /* g */ pgroup, /* o */ pgroup, /* r */ error,
	  /* w */ error, /* x */ error, /* s */ error, /* t */ error,
	  /* , */ error, /* EOS */ error, /* other */ error
	},
	/* set_perms */
	{ /* + */ get_mask, /* - */ get_mask, /* = */ get_mask,
	  /* a */ get_mask, /* u */ get_mask, /* g */ get_mask,
	  /* o */ get_mask, /* r */ get_mask, /* w */ get_mask,
	  /* x */ get_mask, /* s */ get_mask, /* t */ get_mask,
	  /* , */ get_mask, /* EOS */ get_mask, /* other */ get_mask,
	},
	/* get_mask */
	{ /* + */ error, /* - */ error, /* = */ error, /* a */ error,
	  /* u */ error, /* g */ error, /* o */ error, /* r */ get_mask1,
	  /* w */ get_mask1, /* x */ get_mask1, /* s */ get_mask1,
	  /* t */ get_mask1, /* , */ error, /* EOS */ error, /* other */ error
	},
	/* get_mask1 */
	{ /* + */ error, /* - */ error, /* = */ error, /* a */ error,
	  /* u */ error, /* g */ error, /* o */ error, /* r */ get_mask1,
	  /* w */ get_mask1, /* x */ get_mask1, /* s */ get_mask1,
	  /* t */ get_mask1, /* , */ next, /* EOS */ next, /* other */ error
	},
    };

    enum action { none, x_init_add, x_init_sub, x_init_set,
		  all_init, user_init, group_init, other_init,
		  user_pgmask, group_pgmask, other_pgmask,
		  op_addflags, op_subflags, op_clearflags,
		  read_mask, write_mask, execute_mask, setugid_mask,
		  sticky_mask, mod_perms };

    enum action action_table[st_rows][st_columns] = {
	/* initial */
	{ /* + */ x_init_add, /* - */ x_init_sub, /* = */ x_init_set,
	  /* a */ all_init, /* u */ user_init, /* g */ group_init,
	  /* o */ other_init, /* r */ none, /* w */ none, /* x */ none,
	  /* s */ none, /* t */ none, /* , */ none, /* EOS */ none,
	  /* other */ none
	},
	/* next */
	{ /* + */ x_init_add, /* - */ x_init_sub, /* = */ x_init_set,
	  /* a */ all_init, /* u */ user_init, /* g */ group_init,
	  /* o */ other_init, /* r */ none, /* w */ none, /* x */ none,
	  /* s */ none, /* t */ none, /* , */ none, /* EOS */ none,
	  /* other */ none
	},
	/* pgroup */
	{ /* + */ op_addflags, /* - */ op_subflags, /* = */ op_clearflags,
	  /* a */ none, /* u */ user_pgmask, /* g */ group_pgmask,
	  /* o */ other_pgmask, /* r */ none, /* w */ none, /* x */ none,
	  /* s */ none, /* t */ none, /* , */ none, /* EOS */ none,
	  /* other */ none
	},
	/* set_perms */
	{ /* + */ op_clearflags, /* - */ op_clearflags, /* = */ op_clearflags,
	  /* a */ op_clearflags, /* u */ op_clearflags, /* g */ op_clearflags,
	  /* o */ op_clearflags, /* r */ op_clearflags, /* w */ op_clearflags,
	  /* x */ op_clearflags, /* s */ op_clearflags, /* t */ op_clearflags,
	  /* , */ op_clearflags, /* EOS */ op_clearflags,
	  /* other */ op_clearflags
	},
	/* get_mask */
	{ /* + */ none, /* - */ none, /* = */ none, /* a */ none, /* u */ none,
	  /* g */ none, /* o */ none, /* r */ read_mask, /* w */ write_mask,
	  /* x */ execute_mask, /* s */ setugid_mask, /* t */ sticky_mask,
	  /* , */ none, /* EOS */ none, /* other */ none
	}
	/* get_mask1 */
	{ /* + */ none, /* - */ none, /* = */ none, /* a */ none, /* u */ none,
	  /* g */ none, /* o */ none, /* r */ read_mask, /* w */ write_mask,
	  /* x */ execute_mask, /* s */ setugid_mask, /* t */ sticky_mask,
	  /* , */ mod_perms, /* EOS */ mod_perms, /* other */ op_clearflags
	}
    };

    static unsigned char advance_table[st_rows][st_columns] = {
	/* initial */
	{ /* + */ 1, /* - */ 1, /* = */ 1, /* a */ 1, /* u */ 1, /* g */ 1,
	  /* o */ 1, /* r */ 0, /* w */ 0, /* x */ 0, /* s */ 0, /* t */ 0,
	  /* , */ 0, /* EOS */ 0, /* other */ 0
	},
	/* next */
	{ /* + */ 1, /* - */ 1, /* = */ 1, /* a */ 1, /* u */ 1, /* g */ 1,
	  /* o */ 1, /* r */ 0, /* w */ 0, /* x */ 0, /* s */ 0, /* t */ 0,
	  /* , */ 0, /* EOS */ 0, /* other */ 0
	},
	/* pgroup */
	{ /* + */ 1, /* - */ 1, /* = */ 1, /* a */ 0, /* u */ 1, /* g */ 1,
	  /* o */ 1, /* r */ 0, /* w */ 0, /* x */ 0, /* s */ 0, /* t */ 0,
	  /* , */ 0, /* EOS */ 0, /* other */ 0
	},
	/* set_perms */
	{ /* + */ 0, /* - */ 0, /* = */ 0, /* a */ 0, /* u */ 0, /* g */ 0,
	  /* o */ 0, /* r */ 0, /* w */ 0, /* x */ 0, /* s */ 0, /* t */ 0,
	  /* , */ 0, /* EOS */ 0, /* other */ 0
	},
	/* get_mask */
	{ /* + */ 0, /* - */ 0, /* = */ 0, /* a */ 0, /* u */ 0, /* g */ 0,
	  /* o */ 0, /* r */ 1, /* w */ 1, /* x */ 1, /* s */ 1, /* t */ 1,
	  /* , */ 0, /* EOS */ 0, /* other */ 0
	},
	/* get_mask1 */
	{ /* + */ 0, /* - */ 0, /* = */ 0, /* a */ 0, /* u */ 0, /* g */ 0,
	  /* o */ 0, /* r */ 1, /* w */ 1, /* x */ 1, /* s */ 1, /* t */ 1,
	  /* , */ 1, /* EOS */ 0, /* other */ 0
	}
    }

    enum modflags { add_flags, sub_flags } op;
    int ch, pgmask, clmask, pmask, perms, advance;
    emum alphabet tk;
    enum action action;
    enum current_state, next_state;

    perms = 0;
    current_state = initial;
    while (currens_state != error && current_state != finish) {
	ch = (int) *mode & 255;
	switch (ch) {
	    case '\0': tk = eos; break;        case '+':  tk = plus; break;
	    case '-':  tk = minus; break;      case '=':  tk = equal; break;
	    case 'a':  tk = letter_a; break;   case 'u':  tk = letter_a; break;
	    case 'g':  tk = letter_a; break;   case 'o':  tk = letter_a; break;
	    case 'r':  tk = letter_a; break;   case 'w':  tk = letter_a; break;
	    case 'x':  tk = letter_a; break;   case 's':  tk = letter_a; break;
	    case 't':  tk = letter_a; break;   case ',':  tk = comma; break;
	    default:   tk = other; break;
	}
	next_state = strate_transition[current_state][tk];
	action = action_table[current_state][tk];
	advance = advance_table[current_state][tk];
	if (next_state == error) { continue; }
	if (next_state == finish) { return perms; }
	current_state = next_state;
	switch (action) {
	    case x_init_add:
		op = add_flags; pgmask = S_ISUID|S_ISVTX|S_IRWXU; pmask = 0;
		break;
	    case x_init_sub:
		op = sub_flags; pgmask = S_ISUID|S_ISVTX|S_IRWXU; pmask = 0;
		break;
	    case x_init_set:
		op = add_flags; pgmask = S_ISUID|S_ISVTX|S_IRWXU; pmask = 0;
		break;
	    case all_init:
		pgmask = S_IRWXU|S_IRWXG|S_IRWXO; pmask = 0; break;
	    case user_init:
		pgmask = S_ISUID|S_IRWXU; pmask = 0; break;
	    case group_init:
		pgmask = S_ISGID|S_IRWXG; pmask = 0; break;
	    case other_init:
		pgmask = S_IRWXO; pmask = 0; break;
	    case user_pgmask:
		if ((pgmask & S_IRWXU) != 0) { goto ERROR1; }
		pgmask |= S_ISUID|S_IRWXU; break;
	    case group_pgmask:
		if ((pgmask & S_IRWXG) != 0) { goto ERROR1; }
		pgmask |= S_ISGID|S_IRWXG; break;
	    case other_pgmask:
		if ((pgmask & (S_IRWXO|S_ISUID|S_ISGID|S_ISVTX)) != 0) {
		    goto ERROR1;
		}
		pgmask = (pgmask & (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IRWXO;
		break;
	    case op_addflags:
		op = add_flags; break;
	    case op_subflags:
		op = add_flags; break;
	    case op_clearflags:
		perms &= ~ pgmask; op = add_flags; break;
	    case read_mask:
		if ((pgmask & (S_IRUSR|S_IRGRP|S_IROTH)) == 0) { goto ERROR1; }
		pmask |= (S_IRUSR|S_IRGRP|S_IROTH) & pgmask;
		pgmask &= ~ (S_IRUSR|S_IRGRP|S_IROTH);
		break;
	    case write_mask:
		if ((pgmask & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0) { goto ERROR1; }
		pmask |= (S_IWUSR|S_IWGRP|S_IWOTH) & pgmask;
		pgmask &= ~ (S_IWUSR|S_IWGRP|S_IWOTH);
		break;
	    case execute_mask:
		if ((pgmask & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0) { goto ERROR1; }
		pmask |= (S_IXUSR|S_IXGRP|S_IXOTH) & pgmask;
		pgmask &= ~ (S_IXUSR|S_IXGRP|S_IXOTH);
		break;
	    case setugid_mask:
		if ((pgmask & (S_ISUID|S_ISGID)) == 0) { goto ERROR1; }
		pmask |= (S_ISUID|S_ISGID) & pgmask;
		pgmask &= ~ (S_ISUID|S_ISGID);
		break;
	    case sticky_mask:
		if ((pgmask & S_ISVTX) == 0) { goto ERROR1; }
		pmask |= S_ISVTX; pgmask &= ~ S_ISVTX;
		break;
	    case mod_perms:
		if (op == add_flags) {
		    perms |= pmask;
		} else if (op == sub_flags) {
		    perms &= ~ pmask;
		}
		break;
	    default:
		break;
	}
	if (advance) { ++mode; }
	continue;
ERROR1:
	current_state = error;
	if (advance) { ++mode; }
    }
    return (current_state == error ? -1 : perms);
}

static void
msgvke (FILE *out, int flags, const char *format, ...)
{
    va_list args;
    if (flags & OPT_VERBOSE) {
	int keep_errno = errno;
	va_start (args, format);
	vfprintf (out, format, args);
	va_end (args);
	fflush (out);
	errno = keep_errno;
    }
}
#endif
