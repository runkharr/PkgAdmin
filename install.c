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

static int
getmode (const char *mode)
{
    int res = 0, dg, ix, is_num = 1, mm_shift, mask, def_perms, no_ugo;
    const char *p;
    size_t len = strlen (mode);
    for (ix = 0; ix < len; ++ix) {
	if ((dg = digit (mode[ix], 8)) < 0) { is_num = 0; break; }
	res = (res << 3) | dg;
    }
    if (is_num) {
	if (len < 1 || len > 4) { errno = EINVAL; return -1; }
	if (len < 3) {
	    mask = umask (0); umask (mask);
	    def_perms = (0777 & ~ mask) & ~ (-1 << 3 * (3 - len));
	    res = (res << 3 * (3 - len)) | def_perms;
	}
	return res;
    }
    mask = umask (0); umask (mask);
    def_perms = (0666 & ~ mask);
    no_ugo = 0; mask = 0;
    for (p = mode; p; ++p) {
	if (*p == ',') {
	    if (! no_ugo || ! perms) { goto INVALID; }
	    no_ugo = 0; continue;
	}
	if (*p == '+') {
	    if (no_ugo) { goto INVALID; }
	    opmode = 1; continue;
	}
	if (*p == '-') {
	    if (no_ugo) { goto INVALID; }
	    opmode = -1; continue;
	if (*p == 'u') {
	    if (no_ugo) { goto INVALID; }
	    mask |= 0700; continue;
	}
	if (*p == 'g') { mask |= 0070; continue; }
	if (*p == 'o') { mask |= 0007; continue; }
	if (*p == 'a') {
	    if (mask) { errno = EINVAL; return -1; }
	    mask = 0777; continue;
	}
	if (*p == '=') { no_ugo = 1;
	    mm_
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
	int keep_errno = errno;
	va_start (args, format);
	vfprintf (out, format, args);
	va_end (args);
	fflush (out);
	errno = keep_errno;
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
