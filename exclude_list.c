/* escape_list.cc
**
** $Id$
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

#define PROG "exclude_list"

#include "lib/mrmacs.c"
#include "lib/store_prog.c"
#include "lib/isws.c"
#include "lib/cuteol.c"
#include "lib/bgetline.c"

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
    while (bgetline (file, line, linesz) >= 0) {
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
