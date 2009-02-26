/* distfile.c
**
** $Id: distfile.c,v 1.1 2009-02-26 09:34:33 bj Exp $
**
** Author: Boris Jakubith
** E-Mail: fbj@blinx.de
** Copyright: (c) 2009, Boris Jakubith <fbj@blinx.de>
** License: GPL (version 2)
**
** C-implementation of my small `admin/distfile' utility.
**
** Synopsis: distfile [-x exclude-file] [-g 'cleancmd-template'] \
**                    [-f 'packcmd-template' ] srcdist [dir]
**           distfile [-x exclude-file] [-g 'gencmd-template'] \
**                    [-f 'packcmd-template' ] bindist [dir]
**
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/*#include <stdarg.h>*/
#include <regex.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LSZ_INITIAL 1024
#define LSZ_INCREASE 1024

#define t_alloc(t, n) ((t *) malloc ((n) * sizeof(t)))
#define t_realloc(t, p, n) ((t *) realloc ((p), (n) * sizeof(t)))
#define cfree(p) do { \
    void **q = &(p); \
    if (*q) { free (*q); *q = 0; } \
} while (0)

static char *prog = 0;

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

static void store_prog (char *argv[])
{
    char *p = strrchr (argv[0], '/');
    if (p) { ++p; } else { p = argv[0]; }
    if (prog) { cfree (prog); }
    if (!(prog = t_alloc (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", p, strerror (errno)); exit (1);
    }
    strcpy (prog, p);
}

static void usage (void)
{
    printf ("Usage: %s filename\n", prog);
    exit (0);
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

static void conv_path (const char *p, char **_buf, size_t *_bufsz)
{
    char cc;
    int brctx = 0;
    while ((cc = *p++)) {
	switch ((int) cc & 0xFF) {
	    case '\\':
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		if (*p) {
		    buf_puts (p++, 1, _buf, _bufsz);
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
    char cc, *res;
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
		put_special ((int) cc & 0xFF, qmode);
		break;
	    default:
		buf_puts (ccharp (cc), 1, _buf, _bufsz);
		break;
	}
    }
}

typedef struct rxlist_s *rxlist_t;
struct rxlist_s {
    rxlist_t next;
    regex_t rx;
};

static int append_regex (const char *regex, rxlist_t *_first, rxlist_t *_last)
{
    int rc;
    char *buf = 0, errbuf[1024];
    size_t bufsz = 0;
    rxlist_t el = t_alloc (rx_list_t, 1);
    if (!el) {
	fprintf (stderr, "%s: %s - %s\n", prog, regex, sterror (errno));
	exit (1);
    }
    if ((rc = regcomp (&el->rx, regex, REG_EXTENDED|REG_NOSUB))) {
	regerror (rc, &el->rx, errbuf, sizeof (errbuf));
	fprintf (stderr, "%s: %s - %s\n", prog, regex, errbuf);
	rc = -1;
    } else {
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
    if (*_buf) { **_buf = '\0'; }
    if (*p == '*') {
	/* Regular expression follows ... */
	/*buf_puts ("^(", 2, &buf, &bufsz);*/
	unquote_rx (++p, &buf, &bufsz);
	/*buf_puts (")$", 2, &buf, &bufsz);*/
    } else {
	/* Pathname wildcard ... */
	if (*p == ':') { ++p; }
	buf_puts ("^(", 2, _buf, _bufsz);
	if (*p != '/' && strncmp (p, "./", 2) != 0
	&&  strncmp (p, "../", 3) != 0) {
	    conv_path ("./", _buf, _bufsz);
	}
	if (is_dir (p)) {
	    q = p + (strlen (p) - 1);
	    while (q != p && *q == '/') { *q-- = '/'; }
	    conv_path (p, &rx, &rxsz);
	    buf_puts ("\\(/.*\\)?", 8, &rx, &rxsz);
	} else {
	    conv_path (p, &rx, &rxsz);
	}
	buf_puts (")$", 2, &buf, &bufsz);
    }
    return append_regex (buf, _first, _last);
}

static int load_pattern_list (const char *filename, rxlist_t *_out)
{
    int errcnt = 0;
    FILE *file;
    char *line = 0, *buf = 0, *p;
    size_t linesz = 0, bufsz = 0;
    rxlist_t first = 0, last = 0;
    if (!(file = fopen (filename, "rb"))) {
	fprintf (stderr, "%s: %s - %s\n", prog, p, strerror (errno));
	exit (1);
    }
    /* Add the filename itself to the exclude-list ... */
    if (add_pattern (filename, &first, &last, &buf, &bufsz) < 0) { ++errcnt; }
    while (my_getline (file, &line, &linesz) >= 0) {
	p = line; while (isws (*p)) { ++p; }
	if (*p == '\0' || *p == '#') { continue; }
	if (add_pattern (p, &first, &last, &buf, &bufsz) < 0) { ++errcnt; }
    }
    fclose (file);
    cfree (line); cfree (buf);
    *_out = first;
    return (errcnt > 0 ? -1 : 0);
}

#define MODE_SRCDIST 0
#define MODE_BINDIST 1

static int gen_srcdist (rxlist_t exclude_pats,
			char *gencmd,
			char *packcmd,
			char *newdir);

static int gen_bindist (rxlist_t exclude_pats,
			char *gencmd,
			char *packcmd,
			char *newdir);

int main (int argc, char *argv[])
{
    int mode;
    char *mname, *exclude_file = 0, *gencmd = 0, *packcmd = 0, *newdir = 0;
    char *pkgname = 0;
    rxlist_t *exclude_pats = 0;
    store_prog (argv);
    if (argc < 2) { usage (); }
    /* get the `-f', `-g' and `-x' options */
    if (load_pattern_list (exclude_file, &exclude_pats)) {
	fprintf (stderr, "%s: errors found in '%s'\n", argv[1]);
	exit (1);
    }
    /* ... */
    if (optind >= argc) {
	usage ("missing argument(s); Please call `%s -h' for help!", prog);
    }
    mname = argv[optind++];
    if (strcmp (mname, "srcdist") != 0 && strcmp (mname, "bindist") != 0) {
	usage ("invalid mode (only 'srcdist' or 'bindist' allowed)");
    }
    mode = MODE_SRCDIST;
    if (strcmp (mname, "bindist") == 0) { mode = MODE_BINDIST; }
    if (optind < argc) { newdir = argv[optind++]; }
    switch (mode) {
	case MODE_SRCDIST:
	    gen_srcdist (exclude_pats, gencmd, packcmd, newdir, &pkgname);
	    break;
	case MODE_BINDIST:
	    gen_bindist (exclude_pats, gencmd, packcmd, newdir, &pkgname);
	    break;
    }
    /* Der (Pfad-)Name des erzeugten Archivs muß nun noch in die Standard-
    ** ausgabe geschrieben werden ...
    */
    printf ("%s\n", pkgname);
    return 0;
}

static const char *def_gentpls[] = {
    "make && make DESTDIR=%s install",
    "make && make DESTDIR=%s install install-man",
    NULL
};

static const char *def_cluptpls[] = {
    "make cleanall",
    NULL
};

static const char *def_packtpls[] = {
    "tar cf '%s.tar' '%s'; gzip -9f '%s.tar'",
    "tar cf '%s.tar' '%s'; bzip2 -9f '%s.tar'",
    "zip -9r '%s.zip' '%s'",
    NULL
};

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
    int from_file = 1, rc, ix, jx;
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
    if (!tpls) {
	if (rc == -2) {
	    /* error: The file couldn't be opened ... */
	    fprintf (stderr,
		     "%s: %s - %s\n", prog, tplfname, strerror (errno));
	    return -1;
	}
	if (rc == 0) {
	    /* error? The file didn't contain a valid template-line ... */
	    ;
	}
	/* template file not found, but this is no problem - as the compiled
	** default may be used ...
	*/
	tpls = def_list; from_file = 0;
    }
    ix = 0;
    while (lv > 0 && tpls[ix]) { --lv; ++ix; }
    if (from_file) {
	for (jx = 0; tpls[jx]; ++jx) {
	    if (jx == ix) { continue; }
	    cfree (tpls[jx]);
	}
	cfree (tpls);
    }
    if (!(res = tpls[ix])) { errno = EINVAL; }
    return res;
}

static char *get_version (void)
{
    const char *novers = "unknown";
    char *line = 0, *p, *q, *res = 0;
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
	    fprintf (stderr, "%s: getcwd() failed - %s\n,
			     prog, strerror (errno));
	    exit (1);
	}
    }
    if ((p = strrchr (pwdbuf, '/'))) { +p; }
    if (!p || *p == '\0') {
	fprintf (stderr, "%s: invalid path\n", prog); exit (1);
    }
    if (!(res = t_alloc (char, strlen (p) + 1))) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }
    strcpy (res, p);
    return (const char *) res;
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

static int gen_srcdist (rxlist_t exclude_pats, char *cleanupcmd, char *packcmd,
			char *newdir, **_package)
{
    long lv = -1;
    int from_file = 0, need_list = 1, rc;
    char *p, *packdir, *buf = 0, *package = 0;
    size_t bufsz = 0;
    const char *cluptpl = 0, packtpl = 0, *t;
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
    if (!mkdir (packdir, 0755)) {
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
    t = "*^(\./.*/CVS(/.*)?)$";
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
    copy_tree (".", packdir, exclude_pats);
    /* Nun wird im Zielverzeichnis aufgeräumt ... */
    cleanup (packdir, cluptpl);
    /* Anschließend wird das Archiv generiert ... */
    gen_package (packdir, "..", &package);
    /* Das Zielverzeichnis wird nun noch weggeräumt ... */
    remove_packdir (packdir);
    /* Zum Schluß wird der Name des erzeugten Archivs an den Ausgabe-parameter
    ** zugewiesen ...
    */
    *_package = package;
    return 0;
}
