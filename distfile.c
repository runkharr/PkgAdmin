/* distfile.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: fbj@blinx.de
** Copyright: (c) 2009, Boris Jakubith <fbj@blinx.de>
** License: GPL (version 2)
**
** C-implementation of my small `admin/distfile' utility.
**
** Synopsis: distfile [-x exclude-file] [-c 'cleancmd-template'] \
**                    [-p 'packcmd-template' ] srcdist [suffix [dir]]
**           distfile [-x exclude-file] [-i 'installcmd-template'] \
**                    [-p 'packcmd-template' ] bindist [prefix [suffix [dir]]]
**
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <regex.h>
#include <errno.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#define VERSION "0.30"

#define LSZ_INITIAL 1024
#define LSZ_INCREASE 1024

#define t_allocv(t, n) ((t *) malloc ((n) * sizeof(t)))
#define t_allocp(t, n) ((t *) malloc (sizeof(t) + (n)))
#define t_realloc(t, p, n) ((t *) realloc ((p), (n) * sizeof(t)))
#ifdef __cplusplus
static void cfree (void *&p)
{
    if (p) { free (p); p = NULL; }
}
#else
static void distfile_cfree (void *p)
{
    void **_q = (void **) p;
    if (*_q) { free (*_q); *_q = NULL; }
}
#define cfree(p) (distfile_cfree (&(p)))
#endif

static char *prog = 0, *progpath = 0;

static const char *src_excludes = ".srcdist-excludes";
static const char *bin_excludes = ".bindist-excludes";

static const char *def_insttpls[] = {
    "make && make DESTDIR=%d %?(PREFIX=%p )install",
    "make && make DESTDIR=%d %?(PREFIX=%p )install install-man",
    NULL
};

static const char *def_cluptpls[] = {
    "make cleanall",
    "make distclean",
    "make pristine",
    "make clean",
    NULL
};

static const char *def_packtpls[] = {
    "%p%s.tar.gz\ttar cf '%p%s.tar' '%p' && gzip -9f '%p%s.tar'",
    "%p%s.tar.gz\ttar cf '%p%s.tar' '%p' && bzip2 -9f '%p%s.tar'",
    "%p%s.tar.gz\tzip -9r '%p%s.zip' '%p'",
    NULL
};

#ifndef __GNUC__
#define __inline__
#endif

static __inline__ int isws (int c)
{
    return (c == ' ' || c == '\t');
}

static __inline__ int nows (int c)
{
    return (c != '\0' && c != ' ' && c != '\t');
}

/* Cut the end of line chars from the supplied string and return a number
** which tells which type of end of line was found (0 = no EOL, 1 = *nix EOL,
** 2 = MacOS EOL, 3 = DOS/Windows EOL) ...
*/
int cuteol (char *l)
{
    char *p = l + strlen (l);
    if (l == p) { return 0; }
    if (*--p == '\r') { *p = '\0'; return 2; }
    if (*p == '\n') {
	if (l != p) {
	    if (*--p == '\r') { *p = '\0'; return 3; }
	    ++p;
	}
	*p = '\0';
	return 1;
    }
    return 0;
}

/* Read a line from a file. The buffer for this line is supplied as
** (reference-)arguments `_line' and `_linesz' and will eventually
** resized during the retrieval of the line.
** Result is either `-2' (error) or `-1' (no content read due to EOF)
** or the length of the line read (without the EOL-character(s)) ...
*/
int my_getline (FILE *in, char **_line, size_t *_linesz)
{
    char *line = *_line, *p, *rr;
    size_t linesz = *_linesz, len;
    if (!line) {
	linesz = LSZ_INITIAL; line = t_allocv (char, linesz);
	if (!line) { *_linesz = 0; return -2; }
    }
    p = line; len = linesz;
    while ((rr = fgets (p, len, in))) {
	if (cuteol (p)) { break; }
	p += strlen (p);
	len = (size_t) (p - line);
	if (len + 1 >= linesz) {
	    linesz += LSZ_INCREASE;
	    if (!(p = t_realloc (char, line, linesz))) { return -2; }
	    /**_line = p; *_linesz = linesz;*/
	    line = p; p += len;
	}
	len = linesz - len;
    }
    if (!rr && p == line) { errno = 0; return -1; }
/*    if (p == line && *p == '\0') { return -1; }*/
    *_line = line; *_linesz = linesz;
    return (size_t) (p - line);
}

static char *x_strdup (const char *s)
{
    char *res = t_allocv (char, strlen (s) + 1);
    if (!res) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }
    strcpy (res, s);
    return res;
}

static void store_prog (char *argv[])
{
    char *p = strrchr (argv[0], '/');
    if (p) { ++p; } else { p = argv[0]; }
    cfree (prog); /*if (prog) { cfree (prog); }*/
    if (!(prog = t_allocv (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", p, strerror (errno)); exit (1);
    }
    strcpy (prog, p);
    progpath = x_strdup (argv[0]);
}

static void usage (const char *fmt, ...)
{
    va_list ap;
    if (fmt) {
	fprintf (stderr, "%s: ", prog);
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
	exit (64);
    }
    printf ("\nUsage: %s [-p 'packcmd' ] [-c 'cleancmd' ] [-x 'excludes']"
	    " srcdist \\\n                   [suffix [dir]]"
	    "\n       %s [-p 'packcmd' ] [-i 'installcmd' ] [-x 'excludes']"
	    " bindist \\\n                   [prefix [suffix [dir]]]"
	    "\n       %s -h"
	    "\n       %s -V"
	    "\n"
	    "\nOptions:"
	    "\n  -c 'cleancmd'"
	    "\n     Specify a template for cleaning up (removing files from a"
	    " previous build-"
	    "\n     process (like a 'make cleanall')"
	    "\n     (Default: '%s')"
	    "\n  -i 'installcmd'"
	    "\n     Specify a template for the command which installs the"
	    " binary data to be"
	    "\n     packed. A '%%p' is replaced with the target directory of"
	    " the installation."
	    "\n     (Default: '%s')"
	    "\n  -p 'packcmd'"
	    "\n     Specify a template for the packing-command. A '%%p' is"
	    " replaced with the"
	    "\n     name of the directory to be packed."
	    "\n     (Default: '%s')"
	    "\n  -x 'excludes'"
	    "\n     A file which contains pathname-patterns to be excluded"
	    " from the"
	    "\n     'srcdist'/'bindist' process."
	    "\n     (Default: a compiled builtin)"
	    "\n  prefix"
	    "\n     The installation prefix (e.g. /usr, /usr/local, ...)"
	    "\n  suffix"
	    "\n     An additional suffix to be inserted between the archive's"
	    " name and the"
	    "\n     packager's suffix (e.g. the `-bin´ in"
	    " AVLtree-0.75-bin.tar.gz). For"
	    "\n     `srcdist´, this defaults to an empty string, for `bindist´"
	    " to \"-bin\"."
	    "\n  dir"
	    "\n     The directory wherein the 'srcdist'/'bindist' process"
	    " creates the"
	    "\n     temporary directory and leaves the archive after"
	    " completion."
	    "\n     (Default: The basename of the current directory plus a '-'"
	    " plus the"
	    "\n     content of (the first line of) the file 'VERSION' which"
	    " resides in this"
	    "\n     directory.)\n"
	    "\nFor each of the `-c´, `-i´ and `-p´ options a non-negative"
	    " integer may be"
	    "\nspecified, which is then used for selecting a template from a"
	    " list which is read"
	    "\nfrom a file. If matching file exists, a hard-coded list is used"
	    " instead."
	    "\nFor"
	    "\n  -c the file to be used is either `.cleanupcmds´ in the top-"
	    "level directory"
	    "\n      of the source tree or `admin/cleanupcmds´,"
	    "\n  -i it is either `.installcmds´ (again at top-level) or"
	    " `admin/installcmds´,"
	    "\n  -p it is either `.packcmds´ or `admin/packcmds´.\n\n",
	    prog, prog, prog, prog,
	    def_cluptpls[0],
	    def_insttpls[0],
	    def_packtpls[0]
	    );
    exit (0);
}

static void buf_clear (char **_buf, size_t *_bufsz)
{
    if (!*_buf) {
	char *buf = 0;
	size_t bufsz = 1024;
	if (!(buf = t_allocv (char, bufsz))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno));
	    exit (1);
	}
	*_buf = buf; *_bufsz = bufsz;
    }
    memset (*_buf, 0, *_bufsz);
}

static void buf_puts (const char *p, size_t pl, char **_buf, size_t *_bufsz)
{
    size_t bl = (*_buf ? strlen (*_buf) : 0), bs;
    char *buf = *_buf;
    if (pl + bl + 1 >= *_bufsz) {
	bs = *_bufsz + bl + pl + 1025;
	bs -= bs % 1024;
	if (!(buf = t_realloc (char, *_buf, bs))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno));
	    exit (1);
	}
	if (!*_buf) { *buf = '\0'; }
	*_buf = buf; *_bufsz = bs;
    }
    buf += bl; memcpy (buf, p, pl); buf[pl] = '\0';
}

void buf_delete (char **_buf, size_t *_bufsz)
{
    cfree (*_buf); *_bufsz = 0;
}

#define ccharp(c) ((const char *) &(c))

static void conv_path (const char *p, size_t len, char **_buf, size_t *_bufsz)
{
    char cc;
    int brctx = 0;
    while (len-- > 0) {
	cc = *p++;
	switch ((int) cc & 0xFF) {
	    case '\\':
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		if (len > 0) {
		    buf_puts (p++, 1, _buf, _bufsz); --len;
		}
		if (brctx > 0) { brctx = 3; }
		break;
	    case '[':
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		brctx = (brctx > 0 ? 3 : 1);
		break;
	    case '^': case '!':
		if (brctx) {
		    if (brctx < 2) { brctx = 2; }
		} else {
		    if (cc == '^') { buf_puts ("\\", 1, _buf, _bufsz); }
		}
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
	    case ']':
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		brctx = (brctx > 0 && brctx < 3 ? 3 : 0);
		break;
	    case '?':
		if (brctx) {
		    buf_puts (ccharp (cc), 1, _buf, _bufsz); brctx = 3;
		} else {
		    buf_puts (".", 1, _buf, _bufsz);
		}
		break;
	    case '*':
		if (brctx) {
		    buf_puts (ccharp (cc), 1, _buf, _bufsz); brctx = 3;
		} else {
		    buf_puts (".*", 2, _buf, _bufsz);
		}
		break;
	    case '$': case '{': case '}': case '+': case ' ': case '\t':
	    case '.':
		if (brctx) {
		    brctx = 3;
		} else {
		    buf_puts ("\\", 1, _buf, _bufsz);
		}
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		break;
	    default:
		if (brctx > 0) { brctx = 3; }
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		break;
	}
    }
}


int is_dir (const char *path)
{
    struct stat sb;
    if (access (path, F_OK)) { return 0; }
    if (stat (path, &sb)) { return 0; }
    return (S_ISDIR (sb.st_mode) ? 1 : 0);
}

static const char *word_class = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
				"abcdefghijklmnopqrstuvwxyz";
static const char *digit_class = "0123456789";
static const char *blank_class = "\t\n\f\r ";

static int put_class (int class_type, int *_qmode, char **_buf, size_t *_bufsz)
{
    const char *pfx, *x_class;
    char buf[10];
    if (!*_qmode) {
	switch (class_type) {
	    case 's': pfx = "["; x_class = blank_class; break;
	    case 'S': pfx = "[^"; x_class = blank_class; break;
	    case 'd': pfx = "["; x_class = digit_class; break;
	    case 'D': pfx = "[^"; x_class = digit_class; break;
	    case 'w': pfx = "[", x_class = word_class; break;
	    case 'W': pfx = "[^", x_class = word_class; break;
	    case 'Q': *_qmode = 1; return 0;
	    default:  goto ESCAPED_CHAR;
	}
	buf_puts (pfx, strlen (pfx), _buf, _bufsz);
	buf_puts (x_class, strlen (x_class), _buf, _bufsz);
	buf_puts ("]", 1, _buf,_bufsz);
	return 1;
    }
    if (class_type == 'E') { *_qmode = 0; return 0; }
ESCAPED_CHAR:
    sprintf (buf, "\\%c", class_type);
    buf_puts (buf, strlen (buf), _buf, _bufsz);
    return 0;
}

static void put_special (char special, int qmode, char **_buf, size_t *_bufsz)
{
    const char *cf = (qmode ? "\\%c" : "%c");
    char buf[10];
    sprintf (buf, cf, special);
    buf_puts (buf, strlen (buf), _buf, _bufsz);
}


static void unquote_rx (const char *regex_un, char **_buf, size_t *_bufsz)
{
    char cc;
    int qmode = 0;
    while ((cc = *regex_un++)) {
	switch ((int) cc & 0xFF) {
	    case '\\':
		if (*regex_un) {
		    cc = *regex_un++;
		    put_class ((int) cc & 0xFF, &qmode, _buf, _bufsz);
		} else {
		    buf_puts (ccharp (cc), 1, _buf, _bufsz);
		}
		break;
	    case '(': case ')': case '[': case ']': case '{': case '}':
	    case '*': case '+': case '?': case '|': case '^': case '$':
	    case '.':
		put_special ((int) cc & 0xFF, qmode, _buf, _bufsz);
		break;
	    default:
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		break;
	}
    }
}

static void quote_rx (const char *s, char **_buf, size_t *_bufsz)
{
    while (*s) {
	switch (*s) {
	    case '(': case ')': case '[': case ']': case '{': case '}':
	    case '*': case '+': case '?': case '|': case '^': case '$':
	    case '.': case '\\':
		buf_puts ("\\", 1, _buf, _bufsz);
		break;
	    default:
		break;
	}
	buf_puts (s++, 1, _buf, _bufsz);
    }
}

typedef struct rxlist_s *rxlist_t;
struct rxlist_s {
    rxlist_t next;
    regex_t rx;
    char s[1];
};

static int append_regex (const char *regex, rxlist_t *_first, rxlist_t *_last)
{
    int rc;
    char errbuf[1024];
    rxlist_t el = t_allocp (struct rxlist_s, strlen (regex));
    if (!el) {
	fprintf (stderr, "%s: %s - %s\n", prog, regex, strerror (errno));
	exit (1);
    }
    if ((rc = regcomp (&el->rx, regex, REG_EXTENDED|REG_NOSUB))) {
	regerror (rc, &el->rx, errbuf, sizeof (errbuf));
	fprintf (stderr, "%s: %s - %s\n", prog, regex, errbuf);
	rc = -1;
    } else {
	strcpy (el->s, regex);
	el->next = 0;
	if (!*_first) {
	    *_first = *_last = el;
	} else {
	    (*_last)->next = el; *_last = el;
	}
	rc = 0;
    }
    return rc;
}

static int add_pattern (const char *p, rxlist_t *_first, rxlist_t *_last,
			char **_buf, size_t *_bufsz)
{
    buf_clear (_buf, _bufsz);
    if (*p == '~') {
	/* Regular expression match */
	/*buf_puts ("^(", 2, _buf, _bufsz);*/
	unquote_rx (++p, _buf, _bufsz);
	/*buf_puts (")$", 2, _buf, _bufsz);*/
    } else if (*p == '!') {
	/* A (sub-)string match */
	++p; if (*p == '!') {
	    /* Exact match */
	    buf_puts ("^(", 2, _buf, _bufsz);
	    quote_rx (++p, _buf, _bufsz);
	    buf_puts (")$", 2, _buf, _bufsz);
	} else if (*p == '$') {
	    /* End of string match */
	    buf_puts ("(", 1, _buf, _bufsz);
	    quote_rx (++p, _buf, _bufsz);
	    buf_puts (")$", 2, _buf, _bufsz);
	} else if (*p == '^') {
	    /* Begin of string match */
	    buf_puts ("^(", 2, _buf, _bufsz);
	    quote_rx (++p, _buf, _bufsz);
	    buf_puts (")", 1, _buf, _bufsz);
	} else {
	    /* Sub-string match (default after `!´) */
	    if (*p == ':') { ++p; }
	    buf_puts ("(", 1, _buf, _bufsz);
	    quote_rx (p, _buf, _bufsz);
	    buf_puts (")", 1, _buf, _bufsz);
	}
    } else {
	/* Shell-alike wildcard match */
	if (*p == ':') { ++p; }
	buf_puts ("^(", 2, _buf, _bufsz);
	if (*p != '/' && strncmp (p, "./", 2) != 0
	&&  strncmp (p, "../", 3) != 0) {
	    conv_path ("./", 2, _buf, _bufsz);
	}
	conv_path(p, strlen (p), _buf, _bufsz);
	buf_puts (")$", 2, _buf, _bufsz);
    }
    return append_regex (*_buf, _first, _last);
}

static int load_pattern_list (const char *filename, rxlist_t *_out)
{
    int errcnt = 0;
    FILE *file;
    char *line = 0, *buf = 0, *p;
    size_t linesz = 0, bufsz = 0;
    rxlist_t first = 0, last = 0;

    /* Start the exclude-list with the program's name ... */
    buf_clear (&buf, &bufsz);
    if (*progpath != '/') { buf_puts ("./", 2, &buf, &bufsz); }
    buf_puts ("!", 1, &buf, &bufsz);
    buf_puts (progpath, strlen (progpath), &buf, &bufsz);
    p = x_strdup (buf);
    if (add_pattern (p, &first, &last, &buf, &bufsz) < 0) { ++errcnt; }
    cfree (p);

    /* Add the filename-patterns from the supplied exclude-file ... */
    if ((file = fopen (filename, "rb"))) {
	while (my_getline (file, &line, &linesz) >= 0) {
	    p = line; while (isws (*p)) { ++p; }
	    if (*p == '\0' || *p == '#') { continue; }
	    if (add_pattern (p, &first, &last, &buf, &bufsz) < 0) { ++errcnt; }
	}
	fclose (file);
    } else {
	fprintf(stderr, "WARNING! %s - %s\n", filename, strerror (errno));
    }

    /* Add the filename-patterns from a hard-coded file, too ... */
    p = "admin/excludes";
    if ((file = fopen (p, "rb"))) {
	while (my_getline (file, &line, &linesz) >= 0) {
	    p = line; while (isws (*p)) { ++p; }
	    if (*p == '\0' || *p == '#') { continue; }
	    if (add_pattern (p, &first, &last, &buf, &bufsz) < 0) { ++errcnt; }
	}
	fclose (file);
    } else {
	fprintf(stderr, "WARNING! %s - %s\n", p, strerror (errno));
    }
    cfree (line); cfree (buf);
    *_out = first;
    return (errcnt > 0 ? -1 : 0);
}

static int is_prefix (const char *p, const char *s)
{
    size_t pl = strlen (p), sl = strlen (s);
    return (pl <= sl && strncmp (p, s, pl) == 0);
}

/* Read a template-file (code-generation templates or cleanup-templates or
** packing-templates) into a (dynamically allocated) vector of strings which
** is returned via the second argument of this function. The return-value is
** -1 if the template file couldn't be opened because it doesn't exist, -2 on
** each other error from `fopen()' and 0 otherwise. The returned string-vector
** may be NULL even for a return-value of 0, so it must be checked nonetheless
** ...
*/
static int read_tplfile (const char *tplfname, char ***_result)
{
    char *line = 0, **res = 0, *p, *q, *tpl;
    size_t linesz = 0, reslen = 0;
    FILE *tplfile;
    if (!(tplfile = fopen (tplfname, "r"))) {
	*_result = 0; if (errno == ENOENT) { errno = 0; return -1; }
	return -2;
    }
    while (my_getline (tplfile, &line, &linesz) >= 0) {
	p = line; while (isws (*p)) { ++p; }
	if (*p == '\0' || *p == '#') { continue; }
	q = p + strlen (p); while (q != p && isws (*--q));
	*++q = '\0';
	if (!(tpl = t_allocv (char, strlen (p)))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	strcpy (tpl, p);
	if (!(res = t_realloc (char *, res, reslen + 2))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	res[reslen] = tpl;
    }
    cfree (line); fclose (tplfile);
    if (res) { res[reslen + 1] = 0; }
    *_result = res;
    return (int) reslen;
}

static const char *get_template (char tplopt,
				 const char *tplcmd,
				 const char *tplfname,
				 const char *tpl_altfname,
				 const char **def_list)
{
    long lv = 0;
    int rc, ix, jx;
    const char *res = NULL;
    char *p, **tpls = 0;
    const char *tpl_file;
    /* Examine `tplcmd´ only if it holds a value; otherwise, the default
    ** (index-)value of `lv´ (0) is used ...
    */
    if (tplcmd) {
	/* In the first step, assume that `tplcmd´ holds a decimal number */
	lv = (long) strtol (tplcmd, &p, 10);
	if (p && *p == '\0') {
	    if (lv < 0) { 
		/* It is an error if the conversion succeeded but resulted in a
		** negative number ...
		*/
		fprintf (stderr, "%s: argument of option `-%c' out of range\n",
				 prog, tplopt);
		return res;
	    }
	    /* if `tplcmd' was not a number, it is assumed that it is already a
	    ** valid template, so it is returned directly in this case ...
	    */
	    return tplcmd;
	}
    }
    tpl_file = tplfname;
    rc = read_tplfile (tplfname, &tpls);
    if (rc == -1) {
	tpl_file = tpl_altfname;
	rc = read_tplfile (tpl_altfname, &tpls);
    }
    ix = 0;
    if (rc == -2) {
	/* error: The file couldn't be opened ... */
	fprintf (stderr, "%s: %s - %s\n", prog, tpl_file, strerror (errno));
	return res;
    }
    if (rc == 0) {
	/* At least one of the template files could be opened, but it didn't
	** contain a valid template ...
	*/
	fprintf (stderr, "%s: %s was found but didn't hold any template\n",
			 prog, tpl_file);
	errno = EINVAL;
	return res;
    }
    if (rc > 0) {
	/* A value of rc > 0 is the number of templates found in this
	** file ...
	*/
	while (lv > 0 && tpls[ix]) { --lv; ++ix; }

	/* Keep only either the last template or the one which matches the
	** number which was supplied instead of a valid `tplcmd´ ...
	*/
	for (jx = 0; tpls[jx]; ++jx) {
	    if (jx == ix) { continue; }
	    cfree (tpls[jx]);
	}
	res = tpls[ix];
	cfree (tpls);
    } else {
	/* Only -1 (neither of the templates files were found) remains here; in
	** this case, we will use the hard-coded list ...
	*/
	while (lv > 0 && def_list[ix]) { --lv; ++ix; }
	res = def_list[ix];
    }
    if (!res) { errno = EINVAL; }
    return res;
}

static char *get_version (void)
{
    const char *novers = "unknown";
    char *line = 0, *res = 0;
    size_t linesz = 0;
    FILE *versionfile;
    if ((versionfile = fopen ("VERSION", "r"))) {
	novers = "noversion";
	if (my_getline (versionfile, &line, &linesz) >= 0) {
	    if (*line)  {
		if (!(res = t_allocv (char, strlen (line)))) {
		    fprintf (stderr, "%s: %s\n", prog, strerror (errno));
		    exit (1);
		}
		strcpy (res, line);
	    }
	}
	fclose (versionfile);
    }
    if (!res) {
	if (!(res = t_allocv (char, strlen (novers) + 1))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno));
	    exit (1);
	}
	strcpy (res, novers);
    }
    return res;
}

static char *get_thisdir (void)
{
    char *pwdbuf = 0, *p, *res = 0;
    size_t pwdbufsz = 0;
    for (;;) {
	pwdbufsz += 1024;
	if (!(pwdbuf = t_realloc (char, pwdbuf, pwdbufsz))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	if (getcwd (pwdbuf, pwdbufsz)) { break; }
	if (errno != ERANGE) {
	    fprintf (stderr, "%s: getcwd() failed - %s\n",
			     prog, strerror (errno));
	    exit (1);
	}
    }
    if ((p = strrchr (pwdbuf, '/'))) { ++p; }
    if (!p || *p == '\0') {
	fprintf (stderr, "%s: invalid path\n", prog); exit (1);
    }
    if (!(res = t_allocv (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }
    strcpy (res, p);
    return res;
}

static char *get_packdir (const char *packdir)
{
    char *res = NULL, *wd, *vers, *p;
    size_t pl, resl;
    if (packdir) {
	if (!(res = t_allocv (char, strlen (packdir)))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	strcpy (res, packdir);
    } else {
	wd = get_thisdir (); vers = get_version ();
	if (!(res = t_allocv (char, strlen (wd) + strlen (vers) + 2))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	strcpy (res, wd);
	resl = strlen (res);
	pl = strlen (vers) + 1;
	if (resl < pl
	||  (p = &res[resl - pl], *p != '-')
	||  strcmp (&p[1], vers)) {
	    strcat (res, "-"); strcat (res, vers);
	}
	cfree (wd); cfree (vers);
    }
    return res;
}

/*#### list basetype plus removal function ####*/
typedef struct list_s *list_t;
struct list_s { list_t next; };

void distfile_list_free (list_t *_list)
{
    list_t lh;
    while ((lh = *_list)) {
	*_list = lh->next; lh->next = 0; free (lh);
    }
}
#define list_free(l) (distfile_list_free ((list_t *) &(l)))
/*#### end of list basetype plus removal function ####*/

/*#### copy_tree #### */
typedef struct sdlist_s *sdlist_t;
struct sdlist_s {
    sdlist_t next;
    char *path;
};

static sdlist_t sdlist_add (sdlist_t sdlist, const char *sdpath)
{
    sdlist_t newit = t_allocv (struct sdlist_s, sizeof(struct sdlist_s) +
					strlen (sdpath) + 1);
    if (newit) {
	newit->next = sdlist;
	newit->path = (char *) newit + sizeof(struct sdlist_s);
	strcpy (newit->path, sdpath);
    }
    return newit;
}

static void copy_xattrs (const char *src, const char *dst)
{
#if HAVE_XATTR
    /* declare the variables here, so i need to enclose it in a block for
    ** conforming with older C-compilers ...
    */
    char *xattrlist = 0, *xattr = 0, *p, *q;
    size_t xattrlistsz = 0, xattrsz = 0;
    ssize_t xattrlistlen = 0, xattrlen;
    int done = 0, rc;
    while (!done) {
	xattrsz += 1024;
	if (!(p = t_realloc (char, xattrlist, xattrlistsz))) { break; }
	xattr = p; xattrlistlen = llistxattr (src, xattrlist, xattrszlist);
	done = (xattrlistlen >= 0);
    }
    if (done) {
	p = xattrlist;
	while ((ssize_t) (p - xattrlist) < xattrlistlen) {
	    xattrlen = lgetxattr (src, p, xattr, xattrsz);
	    if (xattrlen < 0) {
		if (errno != ERANGE) { break; }
		xattrsz = (size_t) xattrlen;
		if (!(q = t_realloc (char, xattr, xattrsz))) { break; }
		xattr = q; xattrlen = lgetxattr (src, p, xattr, xattrsz);
		if (xattrlen < 0) { break; }
	    }
	    if (lsetxattr (dst, p, xattr, (size_t) xattrlen, 0) < 0) {
		break;
	    }
	    p += strlen (p) + 1;
	}
    }
    p = q = 0; cfree (xattr); cfree (xattrlist);
#else
    ;
#endif
}

/* copy the ownership, the permissions and the extended attributes from
** a source-file to the destination file ...
*/
static void fix_perms (const char *src, const char *dst)
{
    struct stat sb;
    int mode, mask;
    uid_t uid;
    gid_t gid;
    struct utimbuf ftimes;
    if (lstat (src, &sb) == 0) {
	mask = S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO;
	mode = sb.st_mode;
	uid = sb.st_uid; gid = sb.st_gid;
	ftimes.actime = sb.st_atime;
	ftimes.modtime = sb.st_mtime;
	copy_xattrs (src, dst);
#if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500
	lchown (dst, uid, gid);
	if (!S_ISLNK(mode)) {
	    chmod (dst, mode & mask);
	    utime (dst, &ftimes);
	}
#else
	if (!S_ISLNK(mode)) {
	    chown (dst, uid, gid);
	    chmod (dst, mode & mask);
	    utime (dst, &ftimes);
	}
#endif
    }
}

static int icopy_file (const char *src, const char *dst)
{
    struct stat sb;
    if (link (src, dst) == 0) { return 0; }
    if (access (dst, F_OK) == 0) { errno = EEXIST; return -1; }
    if (stat (src, &sb) < 0) { return -1; }
    if (S_ISLNK (sb.st_mode)) {
	char *linkpath = 0;
	size_t linkpathsz = 0;
	int linkpathlen = 0, rc;
	if (!(linkpath = t_allocv (char, sb.st_size + 1))) { return -1; }
	linkpathsz = sb.st_size;
	linkpathlen = readlink (src, linkpath, linkpathsz);
	if (linkpathlen < 0) { cfree (linkpath); return -1; }
	linkpath[linkpathlen] = '\0';
	rc = symlink (linkpath, dst);
	cfree (linkpath); if (rc < 0) { return -1; }
    } else if (!S_ISREG (sb.st_mode)) {
	if (mknod (dst, sb.st_mode, sb.st_rdev) < 0) { return -1; }
    } else {
	FILE *sfp, *dfp;
	char buf[8192];
	ssize_t rlen;
	int done = 1;
	if (!(sfp = fopen (src, "rb"))) { return -1; }
	if (!(dfp = fopen (dst, "wb"))) { fclose (sfp); return -1; }
	while ((rlen = fread (buf, 1, sizeof(buf), sfp)) > 0) {
	    if (fwrite (buf, 1, (size_t) rlen, dfp) != rlen) {
		done = 0; break;
	    }
	}
	fclose (dfp); fclose (sfp);
	if (!done) { return -1; }
    }
    fix_perms (src, dst);
    return 0;
}

static int copy_tree (const char *srcdir, const char *dstdir, rxlist_t excl)
{
    char *spath = NULL, *dpath = NULL, *p;
    size_t spathsz = 0, dpathsz = 0;
    struct dirent *de = NULL;
    rxlist_t rx = NULL;
    regmatch_t m_dummy[1];
    int skip_entry = 0, ec;
    sdlist_t sdlist = NULL, newsd;
    DIR *dfp = opendir (srcdir);
    if (!dfp) {
	fprintf (stderr, "%s: attempt to read directory '%s' failed - %s\n",
			 prog, srcdir, strerror (errno));
	return -1;
    }
    while ((de = readdir (dfp))) {
	if (*de->d_name == '\0') { continue; }
	if (*de->d_name == '.') {
	    if (de->d_name[1] == '\0'
	    ||  (de->d_name[1] == '.' && de->d_name[2] == '\0')) {
		continue;
	    }
	}
	skip_entry = 0;
	buf_clear (&spath, &spathsz);
	buf_puts (srcdir, strlen (srcdir), &spath, &spathsz);
	p = spath + strlen (spath);
	while (--p != spath && *p == '/') { *p = '\0'; }
	if (*p == '/') { *p = '\0'; }
	buf_puts ("/", 1, &spath, &spathsz);
	buf_puts (de->d_name, strlen (de->d_name), &spath, &spathsz);
	for (rx = excl; rx; rx = rx->next) {
	    if (regexec (&rx->rx, spath, 1, m_dummy, 0) == 0) {
		skip_entry = 1; break;
	    }
	}
	if (skip_entry) { continue; }
	if (is_dir (spath)) {
	    if (!(newsd = sdlist_add (sdlist, spath))) { goto ERROR; }
	    sdlist = newsd; continue;
	}
	buf_clear (&dpath, &dpathsz);
	buf_puts (dstdir, strlen (dstdir), &dpath, &dpathsz);
	p = dpath + strlen (dpath);
	while (--p != dpath && *p == '/') { *p = '\0'; }
	if (*p == '/') { *p = '\0'; }
	if (*spath != '/') { buf_puts ("/", 1, &dpath, &dpathsz); }
	buf_puts (spath, strlen (spath), &dpath, &dpathsz);
	if (icopy_file (spath, dpath) < 0) { goto ERROR; }
    }
    closedir (dfp); dfp = NULL;
    buf_delete (&spath, &spathsz);
    /* create the sub-directories and call copy_tree() with each of them
    ** recursively ...
    */
    for (newsd = sdlist; newsd; newsd = sdlist) {
	sdlist = newsd->next;
	buf_clear (&dpath, &dpathsz);
	buf_puts (dstdir, strlen (dstdir), &dpath, &dpathsz);
	p = dpath + strlen (dpath);
	while (p != dpath && *--p == '/') { *p = '\0'; }
	if (*p == '/') { *p = '\0'; }
	if (*newsd->path != '/') { buf_puts ("/", 1, &dpath, &dpathsz); }
	buf_puts (newsd->path, strlen (newsd->path), &dpath, &dpathsz);
	if (mkdir (dpath, 0755) < 0) { goto ERROR; }
	if (chmod (dpath, 0755) < 0) { goto ERROR; }
	if (copy_tree (newsd->path, dstdir, excl) < 0) { goto ERROR; }
	fix_perms (newsd->path, dpath);
	cfree (newsd);
    }
    buf_delete (&dpath, &dpathsz);
    return 0;
ERROR:
    ec = errno;
    if (dfp) { closedir (dfp); dfp = NULL; }
    list_free (sdlist);
    buf_delete (&spath, &spathsz);
    buf_delete (&dpath, &dpathsz);
    errno = ec;
    return -1;
}
/*#### end copy_tree #### */

/*#### cleanup ####*/
/* Perform a `cleanup´-operation in the supplied directory ...
*/
static int cleanup (const char *packdir, const char *cluptpl)
{
    int rc = 0;
    char *cmd = NULL;
    size_t cmdsz = 0;
    buf_clear (&cmd, &cmdsz);
    buf_puts ("cd '", 4, &cmd, &cmdsz);
    buf_puts (packdir, strlen (packdir), &cmd, &cmdsz);
    buf_puts ("'; ", 3, &cmd, &cmdsz);
    buf_puts (cluptpl, strlen (cluptpl), &cmd, &cmdsz);
    rc = system (cmd);
    buf_delete (&cmd, &cmdsz);
    return rc;
}
/*#### end cleanup ####*/

/*#### gen_package #### */
struct rplc_struct {
    char c;
    const char *s;
};

static int pf_subst (struct rplc_struct *rs, const char *tpl,
		     char **_buf, size_t *_bufsz)
{
    int ix, found, rplc, pc, escm;
    const char *p, *q, *s, *r;
    /*char *s;*/
    struct rplc_struct *rx;
    buf_clear (_buf, _bufsz);
    p = tpl; q = p; rplc = 0;
    while (*q) {
	if (*q != '%' || !q[1]) { ++q; continue; }
	if (q[1] == '%') {
	    ++q; buf_puts (p, (size_t) (q - p), _buf, _bufsz);
	    p = q + 2; q = p; ++rplc; continue;
	}
	if (q[1] == '?' && q[2] == '(') {
	    char *ib = NULL; size_t ibsz = 0; int any_failure = 0;
	    buf_puts (p, (size_t) (q - p), _buf, _bufsz);
	    buf_clear (&ib, &ibsz);
	    q += 2; pc = 1; escm = 0; r = q++;
	    while (*++r) {
		if (*r == '\\') {
		    if (!escm) {
			buf_puts (q, (size_t) (r - q), &ib, &ibsz);
			q = &r[1]; escm = 1; continue;
		    }
		    escm = 0; buf_puts (r, 1, &ib, &ibsz); q = &r[1];
		    continue;
		}
		if (*r == '(') {
		    if (!escm) {
			buf_puts (q, (size_t) (r - q), &ib, &ibsz);
			++pc; 
		    }
		    buf_puts (r, 1, &ib, &ibsz); escm = 0; q = &r[1];
		    continue;
		}
		if (*r == ')') {
		    if (!escm) {
			buf_puts (q, (size_t) (r - q), &ib, &ibsz);
			if (--pc == 0) { break; }
		    }
		    buf_puts (r, 1, &ib, &ibsz); escm = 0; q = &r[1];
		    continue;
		}
		if (*r == '%') {
		    if (escm) {
			buf_puts ("\\", 1, &ib, &ibsz); escm = 0;
		    } else {
			buf_puts (q, (size_t) (r - q), &ib, &ibsz); q = &r[1];
		    }
		    if (!*++r) { --r; any_failure = 1; break; }
		    if (*r == '%') {
			buf_puts (r, 1, &ib, &ibsz); q = &r[1]; continue;
		    }
		    s = NULL; found = 0;
		    for (ix = 0; (rx = &rs[ix], rx->c); ++ix) {
			if (*r == rx->c) { s = rx->s; found = 1; continue; }
		    }
		    if (!found) { any_failure = 1; q = &r[1]; continue; }
		    if (!s) { any_failure = 1; q = &r[1]; continue; }
		    buf_puts (s, strlen (s), &ib, &ibsz);
		    q = &r[1]; continue;
		}
		if (escm) { buf_puts ("\\", 1, &ib, &ibsz); escm = 0; }
	    }
	    p = q = ++r;
	    if (!any_failure) { buf_puts (ib, strlen (ib), _buf, _bufsz); }
	    buf_delete (&ib, &ibsz);
	    continue;
	}
	s = NULL; found = 0;
	for (ix = 0; (rx = &rs[ix], rx->c); ++ix) {
	    if (q[1] == rx->c) { s = rx->s; found = 1; break; }
	}
	if (!found) { continue; }
	buf_puts (p, (size_t) (q - p), _buf, _bufsz);
	if (s) { buf_puts (s, strlen (s), _buf, _bufsz); }
	p = q + 2; q = p; ++rplc;
    }
    if (q != p) { buf_puts (p, (size_t) (q - p), _buf, _bufsz); }
    return rplc;
}

static int gen_package (const char *packcmd, const char *packdir,
		        const char *suffix, const char *targetdir,
			char **_package)
{
    char *cmd = NULL, *cp, *package = NULL;
    size_t cmdsz = 0;
    int rc;


    char *fn = NULL;
    size_t fnsz = 0;
    struct rplc_struct r1[3], r2[2];

    r1[0].c = 'p'; r1[0].s = packdir;
    r1[1].c = 's'; r1[1].s = suffix;
    r1[2].c = 0; r1[2].s = NULL;

    buf_clear (&cmd, &cmdsz);
    
    pf_subst (r1, packcmd, &cmd, &cmdsz);

    if ((cp = strchr (cmd, '\t'))) {
	*cp++ = '\0';
	fn = cmd; cmd = NULL;
	fnsz = cmdsz; cmdsz = 0;
	buf_clear (&cmd, &cmdsz);
	r2[0].c = 'F'; r2[0].s = fn;
	r2[1].c = 0; r2[1].s = NULL;
	pf_subst (r2, cp, &cmd, &cmdsz);
	if (!(package = t_allocv (char, strlen (fn) + 1))) {
	    fprintf (stderr, "%s: attempt to allocate memory for the"
			     " package-name failed\n", prog);
	} else {
	    strcpy (package, fn);
	}
	buf_delete (&fn, &fnsz);
    }
    rc = system (cmd);
    if (rc) { cfree (package); } else { *_package = package; }

    buf_delete (&cmd, &cmdsz);
    return rc;
}
/*#### end gen_package #### */

/*#### helper function for removing a file tree ####*/
static int remove_tree (const char *dir, char **_buf, size_t *_bufsz)
{
    int rc;
    char *path = NULL, *p;
    sdlist_t sdlist = NULL, newsd;
    DIR *dfp = opendir (dir);
    struct dirent *de = NULL;
    if (!dfp) {
	fprintf (stderr, "%s: attempt to read directory '%s' failed - %s\n",
			 prog, dir, strerror (errno));
	return -1;
    }
    while ((de = readdir (dfp))) {
	if (*de->d_name == '\0') { continue; }
	if (*de->d_name == '.') {
	    if (de->d_name[1] == '\0'
	    ||  (de->d_name[1] == '.' && de->d_name[2] == '\0')) {
		continue;
	    }
	}
	buf_clear (_buf, _bufsz);
	buf_puts (dir, strlen (dir), _buf, _bufsz);
	p = *_buf + strlen (*_buf);
	while (--p != path && *p == '/') { *p = '\0'; }
	if (*p == '/') { *p = '\0'; }
	buf_puts ("/", 1, _buf, _bufsz);
	buf_puts (de->d_name, strlen (de->d_name), _buf, _bufsz);
	p = *_buf;
	if (is_dir (p)) {
	    if (!(newsd = sdlist_add (sdlist, p))) { goto ERROR; }
	    sdlist = newsd; continue;
	}
	if (unlink (p)) { goto ERROR; }
    }
    closedir (dfp); dfp = NULL;
    for (newsd = sdlist; newsd; newsd = sdlist) {
	sdlist = newsd->next;
	if (remove_tree (newsd->path, _buf, _bufsz)) { goto ERROR; }
	cfree (newsd);
    }
    return rmdir (dir);
ERROR:
    rc = errno;
    if (dfp) { closedir (dfp); dfp = NULL; }
    list_free (sdlist);
    errno = rc;
    return -1;
}
/*#### end of helper function for removing a file tree ####*/

/*#### remove_packdir ####*/
static int remove_packdir (const char *packdir)
{
    int rc, ec;
    char *pbuf = NULL;
    size_t pbufsz = 0;
    buf_clear (&pbuf, &pbufsz);
    rc = remove_tree (packdir, &pbuf, &pbufsz);
    ec = errno;
    buf_delete (&pbuf, &pbufsz);
    errno = ec;
    return rc;
}
/*#### end remove_packdir ####*/

/*#### gen_srcdist ####*/

/* Generate a source-archive using all files in the current source-tree which
** match none of the regular expressions in `exclude_pats´; the commands for
** generating the archive are generated from the templates `cleanupcmd´
** (issuing a cleanup in the temporary directory created for generating the
** archive) and `packcmd´ (the commands which really generate the requested
** archive). The parameter `newdir´ is currently not used, but may be (in a
** later version). On success, `gen_srcdist()´ returns 0, on failure -1.
** `gen_srcdist()´ may return the name of the generated archive (on success!),
** but only if the `packcmd´-template has the format "package_name\tpackcmd"
** ...
*/
static int gen_srcdist (rxlist_t exclude_pats, char *cleanupcmd, char *packcmd,
			char *suffix, char *newdir, char **_package)
{
    int rc;
    char *packdir, *buf = NULL, *package = NULL;
    size_t bufsz = 0;
    const char *cluptpl = NULL, *packtpl = NULL, *t;
    rxlist_t last_pat = 0;
    cluptpl = get_template ('c', cleanupcmd, ".cleanupcmds",
			    "admin/cleanupcmds", def_cluptpls);
    if (!cluptpl) {
	fprintf (stderr, "%s: no template for cleaning up found", prog);
	return -1;
    }
    packtpl = get_template ('p', packcmd, ".packcmds", "admin/packcmds",
			    def_packtpls);
    if (!packtpl) {
	fprintf (stderr, "%s: no packing-template found", prog);
	return -1;
    }
    packdir = get_packdir (newdir);
    if (mkdir (packdir, 0755)) {
	fprintf (stderr, "%s: %s - %s\n", prog, packdir, strerror (errno));
	exit (1);
    }
    if ((last_pat = exclude_pats)) {
	while (last_pat->next) { last_pat = last_pat->next; }
    }
    if (add_pattern (packdir, &exclude_pats, &last_pat, &buf, &bufsz) < 0) {
	fprintf (stderr, "%s: adding '%s' to exclude-list failed - %s\n",
			 prog, packdir, strerror (errno));
	exit (1);
    }
    t = "~^(\\./(.*/)?(\\.svn|CVS))$";
    if (add_pattern (t, &exclude_pats, &last_pat, &buf, &bufsz) < 0) {
	fprintf (stderr, "%s: adding '%s' to exclude-list failed - %s\n",
			 prog, t+1, strerror (errno));
	exit (1);
    }
    /* "Intelligentes" Kopieren der Daten aus dem aktuellen Verzeichnis in
    ** das zu packende Zielverzeichnis. Die Dateien werden nach Möglichkeit
    ** nur referentiell kopiert (`link()'). Nur wenn das nicht funktioniert
    ** werden die Dateien physisch kopiert ...
    */
    rc = -1;
    rc = copy_tree (".", packdir, exclude_pats);
    if (!rc) {
	/* Nun wird im Zielverzeichnis aufgeräumt ... */
	rc = cleanup (packdir, cluptpl);
	/* Anschließend wird das Archiv generiert ... */
	if (!rc) {
	    rc = gen_package (packtpl, packdir, suffix, newdir, &package);
	}
    }
    /* Das Zielverzeichnis wird nun noch weggeräumt ... */
    if (remove_packdir (packdir)) {
	fprintf (stderr, "%s: attempt to remove '%s' failed - %s\n",
			 prog, packdir, strerror (errno));
    }
    /* Zum Schluß wird der Name des erzeugten Archivs an den Ausgabe-parameter
    ** zugewiesen ...
    */
    *_package = package;
    return rc;
}
/*#### end gen_srcdist ####*/

/*#### gen_bindist ####*/

static const char *cwd (void)
{
    static size_t wdsz = 0;
    static char *wd = NULL;
    char *wd1;
    if (wdsz == 0) {
	wdsz = 1024;
	if (!(wd = t_allocv (char, wdsz))) { return wd; }
    }
    for (;;) {
	if (getcwd (wd, wdsz)) { return wd; }
	wdsz += 1024;
	if (!(wd1 = t_realloc (char, wd, wdsz))) { return wd1; }
	wd = wd1;
    }
}

typedef struct flist_s *flist_t;
struct flist_s {
    flist_t next;
    int is_dir;
    char path[1];
};

static flist_t flist_add (flist_t fl, const char *path, int is_dir)
{
    flist_t newfl = t_allocp (struct flist_s, strlen (path) + 1);
    if (newfl) {
	newfl->next = fl;
	newfl->is_dir = is_dir;
	strcpy (newfl->path, path);
    }
    return newfl;
}

static int collect_excludes (const char *dir, rxlist_t excl, flist_t *_xl,
			     char **_buf, size_t *_bufsz)
{
    int ec;
    char *p;
    struct dirent *de = NULL;
    regmatch_t m_dummy[1];
    int do_exclude = 0;
    rxlist_t rx;
    flist_t nxl;
    sdlist_t sdlist = NULL, newsd;
    DIR *dfp = opendir (dir);
    if (!dfp) {
	fprintf (stderr, "%s: attempt to read directory '%s' failed - %s\n",
			 prog, dir, strerror (errno));
	return -1;
    }
    while ((de = readdir (dfp))) {
	if (*de->d_name == '\0') { continue; }
	if (*de->d_name == '.') {
	    if (de->d_name[1] == '\0'
	    ||  (de->d_name[1] == '.' && de->d_name[2] == '\0')) {
		continue;
	    }
	}
	do_exclude = 0;
	buf_clear (_buf, _bufsz);
	buf_puts (dir, strlen (dir), _buf, _bufsz);
	p = *_buf + strlen (*_buf);
	while (--p != *_buf && *p == '/') { *p = '\0'; }
	if (*p == '/') { *p = '\0'; }
	buf_puts ("/", 1, _buf, _bufsz);
	buf_puts (de->d_name, strlen (de->d_name), _buf, _bufsz);
	p = *_buf;
	for (rx = excl; rx; rx = rx->next) {
	    if (regexec (&rx->rx, p, 1, m_dummy, 0) == 0) {
		do_exclude = 1; break;
	    }
	}
	if (do_exclude) {
	    if (!(nxl = flist_add (*_xl, p, is_dir (p)))) { goto ERROR; }
	    *_xl = nxl;
	    continue;
	}
	if (is_dir (p)) {
	    if (!(newsd = sdlist_add (sdlist, p))) { goto ERROR; }
	    sdlist = newsd; continue;
	}
    }
    closedir (dfp); dfp = NULL;
    /* create the sub-directories and call copy_tree() with each of them
    ** recursively ...
    */
    for (newsd = sdlist; newsd; newsd = sdlist) {
	sdlist = newsd->next;
	if (collect_excludes (newsd->path, excl, _xl, _buf, _bufsz)) {
	    goto ERROR;
	}
	cfree (newsd);
    }
    return 0;
ERROR:
    ec = errno;
    if (dfp) { closedir (dfp); dfp = NULL; }
    list_free (sdlist);
    errno = ec;
    return -1;
}

static int exclude_binaries (const char *packdir, rxlist_t exclude_pats)
{
    int rc, ec;
    const char *oldwd;
    char *pbuf = NULL;
    size_t pbufsz = 0;
    flist_t fl = NULL, lh;
    if (!(oldwd = cwd ())) { return -1; }
    if (chdir (packdir)) { return -1; }
    buf_clear (&pbuf, &pbufsz);
    rc = collect_excludes (".", exclude_pats, &fl, &pbuf, &pbufsz);
    if (rc) { goto ERROR; }
    while ((lh = fl)) {
	fl = lh->next;
	if (lh->is_dir) {
	    rc = remove_tree (lh->path, &pbuf, &pbufsz);
	} else {
	    rc = unlink (lh->path);
	}
	free (lh); if (rc) { goto ERROR; }
    }
    buf_delete (&pbuf, &pbufsz);
    return chdir (oldwd);
ERROR:
    ec = errno;
    list_free (fl);
    buf_delete (&pbuf, &pbufsz);
    chdir (oldwd);
    errno = ec;
    return -1;
}

static int gen_bindist (rxlist_t exclude_pats,
			char *instcmd,
			char *packcmd,
			char *instpfx,
			char *suffix,
			char *newdir,
			char **_package)
{
    int rc;
    const char *packtpl = NULL, *insttpl = NULL;
    char *packdir = NULL, *cmd = NULL, *package = NULL;
    size_t cmdsz = 0;
    struct rplc_struct r1[5];

    insttpl = get_template ('c', instcmd, ".installcmds",
			    "admin/installcmds",
			    def_insttpls);
    if (!insttpl) {
	fprintf (stderr, "%s: no template for generating binaries up found",
			 prog);
	return -1;
    }

    packtpl = get_template ('p', packcmd, ".packcmds", "admin/packcmds",
			    def_packtpls);
    if (!packtpl) {
	fprintf (stderr, "%s: no packing-template found", prog);
	return -1;
    }

    packdir = get_packdir (newdir);

    if (mkdir (packdir, 0755)) {
	fprintf (stderr, "%s: %s - %s\n", prog, packdir, strerror (errno));
	exit (1);
    }

    r1[0].c = 'd'; r1[0].s = packdir;
    r1[1].c = 'p'; r1[1].s = instpfx;
    r1[2].c = 0; r1[2].s = NULL;

    buf_clear (&cmd, &cmdsz);
    pf_subst (r1, insttpl, &cmd, &cmdsz);
    rc = system (cmd);

    if (!rc) {
	/* The cleanup-process which uses `exclude_pats´ is not ready yet ... */
	rc = exclude_binaries (packdir, exclude_pats);
    }

    if (!rc) {
	/* Nun wird das Archiv generiert ... */
	rc = gen_package (packtpl, packdir, suffix, newdir, &package);
    }

    buf_delete (&cmd, &cmdsz);

    /* Das Zielverzeichnis wird nun noch weggeräumt ... */
    if (remove_packdir (packdir)) {
	fprintf (stderr, "%s: attempt to remove '%s' failed - %s\n",
			 prog, packdir, strerror (errno));
    }
    /* Zum Schluß wird der Name des erzeugten Archivs an den Ausgabe-parameter
    ** zugewiesen ...
    */
    *_package = package;
    return rc;
}
/*#### end gen_bindist ####*/

/*#### main program ####*/
#define MODE_SRCDIST 0
#define MODE_BINDIST 1

static char *subst_version (const char *path)
{
    int rc;
    char *buf = NULL, *version = get_version ();
    char *res = NULL;
    size_t bufsz = 0;
    struct rplc_struct r1[2];

    r1[0].c = 'v'; r1[0].s = version;
    r1[1].c = 0; r1[1].s = NULL;

    buf_clear (&buf, &bufsz);
    rc = pf_subst (r1, path, &buf, &bufsz);
    res = x_strdup ((rc > 0 ? buf : path));
    buf_delete (&buf, &bufsz);
    cfree (version);
    return res;
}

int main (int argc, char *argv[])
{
    int mode = -1, opt;
    const char *exclude_file = 0;
    char *mname, *instcmd = NULL, *packcmd = NULL, *newdir = NULL;
    char *pkgname = NULL, *clupcmd = NULL, *ipfx = NULL, *psfx = NULL;
    rxlist_t exclude_pats = NULL;
    store_prog (argv);
    if (argc < 2) { usage (NULL); }
    /* get the `-c', `-h', `-i', `-p', `-V' and `-x' options */
    while ((opt = getopt (argc, argv, "+c:hi:p:Vx:")) != -1) {
	switch (opt) {
	    case 'c':	/* -c 'cleancmd-template' (e.g. -c 'make cleanall') */
		if (clupcmd) { usage ("ambiguous '-c'-option"); }
		clupcmd = x_strdup (optarg);
		break;
	    case 'h':	/* -h */
		usage (0);
	    case 'i':	/* -i 'installcmd-template' (e.g. -i 'make install') */
		if (instcmd) { usage ("ambiguous '-i'-option"); }
		instcmd = x_strdup (optarg);
		break;
	    case 'p':	/* -p 'packcmd-template' (e.g. -p 'zip -r %p.zip %p') */
		if (packcmd) { usage ("ambiguous '-p'-option"); }
		packcmd = x_strdup (optarg);
		break;
	    case 'V':	/* -V (version) */
		printf ("%s %s\n", prog, VERSION); exit (0);
	    case 'x':	/* -x exclude-file (e.g. -x .srcdist-exclude) */
		if (exclude_file) { usage ("ambiguous '-x'-option"); }
		exclude_file = x_strdup (optarg);
		break;
	    default:
		usage ("invalid option '-%c' found", opt);
	}
    }
    if (optind >= argc) {
	usage ("missing argument(s); Please call `%s -h' for help!", prog);
    }
    mname = argv[optind++];
    if (is_prefix (mname, "srcdist")) {
	mode = MODE_SRCDIST;
	if (!exclude_file) { exclude_file = src_excludes; }
    } else if (is_prefix (mname, "bindist")) {
	mode = MODE_BINDIST;
	if (!exclude_file) { exclude_file = bin_excludes; }
    } else {
	usage ("invalid mode; use a (prefix of) 'srcdist' or 'bindist'");
    }
    if (load_pattern_list (exclude_file, &exclude_pats)) {
	fprintf (stderr, "%s: errors found in '%s'\n", prog, argv[1]);
	exit (1);
    }
    /* ... */
    switch (mode) {
	case MODE_SRCDIST:
	    /* Two optional arguments (a package-suffix and then the directory
	    ** where the package is generated ...
	    */
	    if (optind < argc) { psfx = argv[optind++]; }
	    if (optind < argc) { newdir = subst_version (argv[optind++]); }
	    gen_srcdist (exclude_pats, clupcmd, packcmd,
			 psfx, newdir, &pkgname);
	    break;
	case MODE_BINDIST:
	    /* Three optional arguments (the install-prefix and then a package
	    ** suffix and then the directory where the package is generated) ...
	    */
	    if (optind < argc) { ipfx = argv[optind++]; }
	    psfx = "-bin"; if (optind < argc) { psfx = argv[optind++]; }
	    if (optind < argc) { newdir = subst_version (argv[optind++]); }
	    gen_bindist (exclude_pats, instcmd, packcmd, ipfx,
			 psfx, newdir, &pkgname);
	    break;
    }
    /* Der (Pfad-)Name des erzeugten Archivs muß nun noch in die Standard-
    ** ausgabe geschrieben werden ...
    */
    printf ("%s\n", pkgname);
    return 0;
}
/*#### end of main program ####*/
