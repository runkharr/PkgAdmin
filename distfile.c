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
**                    [-p 'packcmd-template' ] srcdist [dir]
**           distfile [-x exclude-file] [-i 'installcmd-template'] \
**                    [-p 'packcmd-template' ] bindist [dir]
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

#define t_alloc(t, n) ((t *) malloc ((n) * sizeof(t)))
#define allocplus(t, n) ((t *) malloc (sizeof(t) + (n)))
#define t_realloc(t, p, n) ((t *) realloc ((p), (n) * sizeof(t)))
#define cfree(p) do { \
    void **q = (void **)&(p); \
    if (*q) { free (*q); *q = 0; } \
} while (0)

static char *prog = 0, *progpath = 0;

static const char *src_excludes = ".srcdist-excludes";
static const char *bin_excludes = ".bindist-excludes";

static const char *def_insttpls[] = {
    "make && make DESTDIR=%p install",
    "make && make DESTDIR=%p install install-man",
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
    "tar cf '%p.tar' '%p'; gzip -9f '%p.tar'",
    "tar cf '%p.tar' '%p'; bzip2 -9f '%p.tar'",
    "zip -9r '%p.zip' '%p'",
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
	linesz = LSZ_INITIAL; line = t_alloc (char, linesz);
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
    char *res = t_alloc (char, strlen (s) + 1);
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
    if (!(prog = t_alloc (char, strlen (p) + 1))) {
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
    printf ("\nUsage: %s [-f 'packcmd' ] [-c 'cleancmd' ] [-x 'excludes']"
	    " srcdist [dir]"
	    "\n       %s [-f 'packcmd' ] [-i 'installcmd' ] [-x 'excludes']"
	    " bindist [dir]"
	    "\n       %s -h"
	    "\n       %s -V"
	    "\n"
	    "\nOptions:"
	    "\n  -f 'packcmd'"
	    "\n     Specify a template for the packing-command. A '%%p' is"
	    " replaced with the"
	    "\n     name of the directory to be packed."
	    "\n     (Default: '%s')"
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
	    "\n  -x 'excludes'"
	    "\n     A file which contains pathname-patterns to be excluded"
	    " from the"
	    "\n     'srcdist'/'bindist' process."
	    "\n     (Default: a compiled builtin)"
	    "\n  dir"
	    "\n     Specify the target directory for the 'srcdist'/'bindist'"
	    "process."
	    "\n     (Default: The basename of the current directory plus a '-'"
	    " plus the"
	    "\n     content of (the first line of) the file 'VERSION' which"
	    " resides in this"
	    "\n     directory.)",
	    prog, prog, prog, prog,
	    def_packtpls[0],
	    def_cluptpls[0],
	    def_insttpls[0]
	    );
    exit (0);
}

static void buf_clear (char **_buf, size_t *_bufsz)
{
    if (!*_buf) {
	char *buf = 0;
	size_t bufsz = 1024;
	if (!(buf = t_alloc (char, bufsz))) {
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
    rxlist_t el = allocplus (struct rxlist_s, strlen (regex));
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
    buf_clear (_buf, _bufsz); /*##was: if (*_buf) { **_buf = '\0'; } */
    if (*p == '~') {
	/* Regular expression follows ... */
	/*buf_puts ("^(", 2, _buf, _bufsz);*/
	unquote_rx (++p, _buf, _bufsz);
	/*buf_puts (")$", 2, _buf, _bufsz);*/
    } else if (*p == '!') {
	/* A (sub-)string match */
	++p; if (*p == '!') {
	    /* The supplied string must match exactly */
	    ++p; buf_puts ("^(", 2, _buf, _bufsz);
	    quote_rx (p, _buf, _bufsz);
	    buf_puts (")$", 2, _buf, _bufsz);
	} else {
	    quote_rx (p, _buf, _bufsz);
	}
    } else {
	/* Pathname wildcard ... */
	if (*p == ':') { ++p; }
	buf_puts ("^(", 2, _buf, _bufsz);
	if (*p != '/' && strncmp (p, "./", 2) != 0
	&&  strncmp (p, "../", 3) != 0) {
	    conv_path ("./", 2, _buf, _bufsz);
	}
#if 0
	if (is_dir (p)) {
	    /* truncate any trailing '/' from the directory name ... */
	    const char *q = p + (strlen (p) - 1);
	    while (q != p && *q == '/') { --q; }
	    /* one step back (towards the end of the pathname), because q
	    ** must point to the first position behind the last char which
	    ** was *no* '/' ...
	    */
	    if (q != p) { ++q; }
	    conv_path (p, (size_t) (q - p), _buf, _bufsz);
	    buf_puts ("(/.*)?", 8, _buf, _bufsz);
	} else {
	    conv_path (p, strlen (p), _buf, _bufsz);
	}
#else
	conv_path(p, strlen (p), _buf, _bufsz);
#endif
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

#if 0
    /* Add the filename of the exclude-list itself ... */
    buf_clear (&buf, &bufsz);
    buf_puts ("!", 1, &buf, &bufsz);
    buf_puts (filename, strlen (filename), &buf, &bufsz);
    p = x_strdup (buf);
    if (add_pattern (p, &first, &last, &buf, &bufsz) < 0) { ++errcnt; }
    cfree (p);
#endif

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

#define MODE_SRCDIST 0
#define MODE_BINDIST 1

static int gen_srcdist (rxlist_t exclude_pats,
			char *gencmd,
			char *packcmd,
			char *newdir,
			char **_package);

static int gen_bindist (rxlist_t exclude_pats,
			char *gencmd,
			char *packcmd,
			char *newdir,
			char **_package);

int main (int argc, char *argv[])
{
    int mode = -1, opt;
    const char *exclude_file = 0;
    char *mname, *instcmd = 0, *packcmd = 0, *newdir = 0, *pkgname = 0;
    char *clupcmd = 0;
    rxlist_t exclude_pats = 0;
    store_prog (argv);
    if (argc < 2) { usage (0); }
    /* get the `-c', `-h', `-i', `-p', `-V' and `-x' options */
    while ((opt = getopt (argc, argv, "c:hi:p:Vx:")) != -1) {
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
    if (optind < argc) { newdir = argv[optind++]; }
    switch (mode) {
	case MODE_SRCDIST:
	    gen_srcdist (exclude_pats, clupcmd, packcmd, newdir, &pkgname);
	    break;
	case MODE_BINDIST:
	    gen_bindist (exclude_pats, instcmd, packcmd, newdir, &pkgname);
	    break;
    }
    /* Der (Pfad-)Name des erzeugten Archivs muß nun noch in die Standard-
    ** ausgabe geschrieben werden ...
    */
    printf ("%s\n", pkgname);
    return 0;
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
	if (!(tpl = t_alloc (char, strlen (p)))) {
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
    return 0;
}

static const char *get_template (const char *tplcmd,
				 const char *tplfname,
				 const char *tpl_altfname,
				 const char **def_list)
{
    long lv = 0;
    int rc, ix, jx;
    const char *res = 0;
    char *p, **tpls = 0;
    if (tplcmd) {
	lv = (long) strtoul (tplcmd, &p, 10);
	if (p && *p == '\0' && lv < 0) {
	    fprintf (stderr,
		     "%s: argument of option `-g' out of range\n", prog);
	    return res;
	}
	/* if `tplcmd' was not a number, it is assumed that it is already a
	** valid template, so it is returned directly in this case ...
	*/
	if (p && *p != '\0') { return tplcmd; }
    }
    rc = read_tplfile (tplfname, &tpls);
    if (rc == -1) { rc = read_tplfile (tpl_altfname, &tpls); }
    ix = 0;
    if (!tpls) {
	if (rc == -2) {
	    /* error: The file couldn't be opened ... */
	    fprintf (stderr,
		     "%s: %s - %s\n", prog, tplfname, strerror (errno));
	    return res;
	}
	if (rc == 0) {
	    /* error? The file didn't contain a valid template-line ... */
	    ;
	}
	/* template file not found, but this is no problem - as the compiled
	** default may be used ...
	*/
	while (lv > 0 && def_list[ix]) { --lv; ++ix; }
	res = def_list[ix];
    } else {
	while (lv > 0 && tpls[ix]) { --lv; ++ix; }
	for (jx = 0; tpls[jx]; ++jx) {
	    if (jx == ix) { continue; }
	    cfree (tpls[jx]);
	}
	cfree (tpls);
	res = tpls[ix];
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
		if (!(res = t_alloc (char, strlen (line)))) {
		    fprintf (stderr, "%s: %s\n", prog, strerror (errno));
		    exit (1);
		}
		strcpy (res, line);
	    }
	}
	fclose (versionfile);
    }
    if (!res) {
	if (!(res = t_alloc (char, strlen (novers) + 1))) {
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
    if (!(res = t_alloc (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }
    strcpy (res, p);
    return res;
}

static char *get_packdir (const char *packdir)
{
    char *res = 0, *wd, *vers;
    if (packdir) {
	if (!(res = t_alloc (char, strlen (packdir)))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	strcpy (res, packdir);
    } else {
	wd = get_thisdir (); vers = get_version ();
	if (!(res = t_alloc (char, strlen (wd) + strlen (vers) + 2))) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	strcpy (res, wd); strcat (res, "-"); strcat (res, vers);
	cfree (wd); cfree (vers);
    }
    return res;
}

static int copy_tree (const char *srcdir, const char *dstdir, rxlist_t excl);
static int cleanup (const char *packdir, const char *cluptpl);
static int gen_package (const char *packcmd, const char *packdir,
			const char *targetdir, char **_package);
static int remove_packdir (const char *packdir);

static int gen_srcdist (rxlist_t exclude_pats, char *cleanupcmd, char *packcmd,
			char *newdir, char **_package)
{
    int rc;
    char *packdir, *buf = 0, *package = 0;
    size_t bufsz = 0;
    const char *cluptpl = 0, *packtpl = 0, *t;
    rxlist_t last_pat = 0;
    cluptpl = get_template (cleanupcmd, ".cleanupcmds", "admin/cleanupcmds",
			    def_cluptpls);
    if (!cluptpl) {
	fprintf (stderr, "%s: no template for cleaning up found", prog);
	return -1;
    }
    packtpl = get_template (packcmd, ".packcmds", "admin/packcmds",
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
    if (copy_tree (".", packdir, exclude_pats) == 0) {
	/* Nun wird im Zielverzeichnis aufgeräumt ... */
	cleanup (packdir, cluptpl);
	/* Anschließend wird das Archiv generiert ... */
	gen_package (packcmd, packdir, "..", &package);
	rc = 0;
    }
    /* Das Zielverzeichnis wird nun noch weggeräumt ... */
    remove_packdir (packdir);
    /* Zum Schluß wird der Name des erzeugten Archivs an den Ausgabe-parameter
    ** zugewiesen ...
    */
    *_package = package;
    return 0;
}

typedef struct sdlist_s *sdlist_t;
struct sdlist_s {
    sdlist_t next;
    char *path;
};

static sdlist_t sdlist_add (sdlist_t sdlist, const char *sdpath)
{
    sdlist_t newit = t_alloc (struct sdlist_s, sizeof(struct sdlist_s) +
					strlen (sdpath) + 1);
    if (newit) {
	newit->next = sdlist;
	newit->path = (char *) newit + sizeof(struct sdlist_s);
	strcpy (newit->path, sdpath);
    }
    return newit;
}

static void sdlist_free (sdlist_t *_sdlist)
{
    sdlist_t lh;
    while ((lh = *_sdlist)) {
	*_sdlist = lh->next; lh->next = 0; free (lh);
    }
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
	if (!(linkpath = t_alloc (char, sb.st_size + 1))) { return -1; }
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
    char *spath = 0, *dpath = 0, *p;
    size_t spathsz = 0, dpathsz = 0;
    struct dirent *de = 0;
    rxlist_t rx = 0;
    regmatch_t m_dummy[1];
    int skip_entry = 0;
    sdlist_t sdlist = 0, newsd;
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
    closedir (dfp); dfp = 0;
    /* create the sub-directories and call copy_tree() with each of them
    ** recursively ...
    */
    for (newsd = sdlist; newsd; newsd = newsd->next) {
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
    }
    sdlist_free (&sdlist);
    cfree (spath); cfree (dpath);
    return 0;
ERROR:
    closedir (dfp);
    sdlist_free (&sdlist); cfree (spath); cfree (dpath);
    return -1;
}

static int gen_bindist (rxlist_t exclude_pats,
			char *gencmd,
			char *packcmd,
			char *newdir,
			char **_package)
{
#if 0
#error 'gen_bindist()' is not yet implemented ...
#endif
    errno = EINVAL; return -1;
}

static int cleanup (const char *packdir, const char *cluptpl)
{
#if 0
#error 'cleanup()' is not yet implemented ...
#endif
    errno = EINVAL; return -1;
}

static int gen_package (const char *packcmd, const char *packdir,
			const char *targetdir, char **_package)
{
#if 0
#error 'gen_package()' is not yet implemented ...
#endif
    errno = EINVAL; return -1;
}

static int remove_packdir (const char *packdir)
{
#if 0
#error 'remove_packdir()' is not yet implemented ...
#endif
    errno = EINVAL; return -1;
}
