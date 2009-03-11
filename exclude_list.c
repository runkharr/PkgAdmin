/* escape_list.cc
**
** $Id: exclude_list.c,v 1.3 2009-03-11 17:26:53 bj Exp $
**
** Author: Boris Jakubith
** E-Mail: fbj@blinx.de
** Copyright: (c) 2009, Boris Jakubith <fbj@blinx.de>
** License: GPL (version 2)
**
** Small C-program which reads a file line by line, treats
** all lines which are not empty and don't start with a '#'
** as pathnames of files and converts them into a single regular
** expression suitable for the `-regex'-option of a `find'-command
** ...
** 
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/*#include <stdarg.h>*/
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

static void add_rx (const char *p, char **_buf, size_t *_bufsz)
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

int main (int argc, char *argv[])
{
    char *line = 0, *rx = 0, *p, *q;
    size_t linesz = 0, rxsz = 0;
    FILE *file;
    store_prog (argv);
    if (argc < 2) { usage (); }
    p = argv[1];
    if (!(file = fopen (p, "rb"))) {
	fprintf (stderr, "%s: %s - %s\n", prog, p, strerror (errno));
	exit (1);
    }
    buf_puts ("^\\(", 3, &rx, &rxsz);
    if (*p != '/' && strncmp (p, "./", 2) != 0 &&  strncmp (p, "../", 3) != 0) {
	add_rx ("./", &rx, &rxsz);
    }
    add_rx (p, &rx, &rxsz);
    while (my_getline (file, &line, &linesz) >= 0) {
	p = line; while (isws (*p)) { ++p; }
	if (*p == '\0' || *p == '#') { continue; }
	buf_puts ("\\|", 2, &rx, &rxsz);
	if (*p != '/' && strncmp (p, "./", 2) != 0
	&&  strncmp (p, "../", 3) != 0) {
	    add_rx ("./", &rx, &rxsz);
	}
	if (is_dir (p)) {
	    q = p + (strlen (p) - 1);
	    while (q != p && *q == '/') { *q-- = '\0'; }
	    add_rx (p, &rx, &rxsz);
	    buf_puts ("\\(/.*\\)?", 8, &rx, &rxsz);
	} else {
	    add_rx (p, &rx, &rxsz);
	}
    }
    buf_puts ("\\)$", 3, &rx, &rxsz);
    fclose (file);
    printf ("%s\n", rx);
    cfree (rx); cfree (line);
    return 0;
}
