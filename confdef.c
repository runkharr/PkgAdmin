/* confdef.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Simple method for grabbing "configuration" values from a C-sourcefile by
** extracting the corresponding `#define´-lines ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>

#define PROG "getconf"

#include "lib/mrmacs.c"
#include "lib/set_prog.c"
#include "lib/isws.c"
#include "lib/nows.c"
#include "lib/pbCopy.c"
#include "lib/bgetline.c"
#include "lib/puteol.c"

typedef struct list *list_t;
struct list {
    list_t next;
    int ambiguous;
    char *name, *value;
};

static void _free_list (list_t *_list)
{
    list_t list;
    for (list = *_list; list; list = *_list) {
	*_list = list->next;
	cfree (list);
    }
}

static int _insert (list_t *_list, const char *name, const char *value)
{
    char *p;
    list_t list = *_list;
    size_t nl, vl;
    while (list) {
	if (!strcmp (list->name, name)) { list->ambiguous = 1; return 1; }
	list = list->next;
    }
    nl = strlen (name) + 1; vl = (value ? 0 : strlen (value) + 1);
    ifnull (list = t_allocp (struct list, nl + vl)) {
	return -1;
    }
    list->next = *_list; *_list = list;
    p = (char *)list + sizeof(struct list);
    list->name = p; p = pbCopy (p, name);
    if (value) {
	list->value = ++p; pbCopy (p, value);
    } else {
	list->value = NULL;
    }
    return 0;
}

static int scan_file (const char *infile, int namesc, char **names,
		      list_t *_out)
{
    int lc = 0, rc, isstdin = 0, ix;
    size_t linesz = 0;
    char *line = NULL, *p, *q, *r;
    FILE *in = NULL;

    if (!strcmp (infile, "-")) {
	in = stdin; isstdin = 1;
    } else if (!(in = fopen (infile, "r"))) {
	return -1;
    }

    while ((rc = bgetline (in, line, linesz)) >= 0) {
	++lc;
	if (*line != '#') { continue; }
	p = line; while (isws (*++p));
	if (!*p) {continue; }
	if (strncmp (p, "define", sizeof("define") - 1)
	||  nows (p[sizeof("define") - 1])) {
	    continue;
	}
	p += sizeof("define") - 1; while (isws (*p)) { ++p; }
	if (!isalpha (*p) && *p != '_') { continue; }
	q = p; while (nows (*++q)) {
	    if (!isalnum (*q) && *q != '_') { break; }
	}
	if (!isws (*q)) { continue; }
	if (!*q) {
	    q = NULL;
	} else {
	    *q++ = '\0';
	    while (isws (*q)) { ++q; }
	    r = q + strlen (q);
	    while (r > q && isws (*--r)) { *r = '\0'; }
	}
	for (ix = 0; ix < namesc; ++ix) {
	    if (!strcmp (p, names[ix])) { break; }
	}
	if (ix >= namesc) { continue; }
	rc = _insert (_out, p, q);
	if (rc < 0) { cfree (line); if (!isstdin) fclose (in); return -1; }
	if (rc > 0) {
	    fprintf (stderr, "%s: %s(line %d) - ambiguous definition of %s\n",
			     prog, infile, lc, p);
	}
    }
    if (++rc < 0) {
	fprintf (stderr, "%s: %s(line %d) - %s\n",
			 prog, infile, lc, strerror (errno));
    }
    cfree (line); if (!isstdin) fclose (in);
    return rc;
}

static int translate (const char *v, FILE *out)
{
    if (!v) {
	fputs ("undefined\n", out);
    } else if (*v == '"') {
	while (*++v && *v != '"') {
	    if (*v == '\\') {
		if (!*++v) { break; }
		switch (*v) {
		    case '"': fputc ('"', out); break;
		    /* other specials for later! */
		    default: fputc (*v, out); break;
		}
	    }
	}
    } else {
	while (*v) {
	    if (*v == '/' && (v[1] == '*' || v[1] == '/')) { break; }
	    fputc (*v, out); ++v;
	}
    }
    return 0;
}

static int write_list (list_t list, int allow_ambiguous, FILE *out)
{
    int errcc = 0;
    list_t l1;
    for (l1 = list; l1; l1 = l1->next) {
	if (l1->ambiguous && !allow_ambiguous) { ++errcc; continue; }
	fputs (l1->name, out); fputc (' ', out);
	translate (l1->value, out);
	puteol (out);
    }
    return errcc;
}

static void usage (const char *format, ...)
{
    if (format) {
	va_list ual;
	fprintf (stderr, "%s: ", prog);
	va_start (ual, format); vfprintf (stderr, format, ual); va_end (ual);
	puteol (stderr);
	exit (64);
    }
    printf ("Usage: %s [-a] [-o outfile] file name...\n"
	    "       %s -h\n"
	    "\nOptions:"
	    "\n  -a (allow ambiguous)"
	    "\n    Don't suppress the output or names which had more than one"
	    " '#define' in"
	    "\n    'file'. In this case, the value of the first '#define' is"
	    " printed.'"
	    "\n  -o outfile"
	    "\n    Write the result to 'outfile' (instead of stdout)"
	    "\n  -x (extended)"
	    "\n    Write the result always in the 'name value'"
	    "\n  -h (help)"
	    "\n    Display this message and terminate"
	    "\nIf file consists of a single '-', the data is read from stdin"
	    "\n\n",
	    prog, prog);
    exit (0);
}

int check_name (const char *name)
{
    if (!isalpha (*name) && *name != '_') { return -1; }
    while (*++name) {
	if (!isalnum (*name) && *name != '_') { return -1; }
    }
    return 0;
}

int main (int argc, char *argv[])
{
    int opt, extended = 0, isstdout = 0, allow_ambiguous = 0, ix, rc, namesc;
    list_t defines_list = NULL;
    char *infile = NULL, *outfile = NULL, **names;
    FILE *out = NULL;

    set_prog (argc, argv);
    while ((opt = getopt (argc, argv, ":ho:x")) != -1) {
	switch (opt) {
	    case 'a': allow_ambiguous = 1; break;
	    case 'h': usage (NULL); break;
	    case 'o':
		if (outfile) { usage ("ambiguous option '-o'"); }
		outfile = optarg;
		break;
	    case 'x': extended = 1; break;
	    case ':':
		usage ("missing argument for option '%s'", argv[optind]);
	    default:
		usage ("invalid option '%s", argv[optind]);
	}
    }
    if (argc - optind < 2) { usage ("missing argument(s)"); }
    infile = argv[optind++]; namesc = argc - optind; names = &argv[optind];
    for (ix = 0; ix < namesc; ++ix) {
	if (check_name (names[ix])) { usage ("'%s' - invalid identifier"); }
    }
    if (!outfile || !strcmp (outfile, "-")) {
	out = stdout; isstdout = 1;
    } else if (!(out = fopen (outfile, "w"))) {
	fprintf (stderr, "%s: %s - %s\n", prog, outfile, strerror (errno));
	exit (1);
    }
    rc = scan_file (infile, namesc, names, &defines_list);
    if (rc < 0) {
	fprintf (stderr, "%s: %s - %s\n", prog, infile, strerror (errno));
	exit (1);
    }
    if (namesc == 1 && !extended) {
	if (defines_list && (!defines_list->ambiguous || allow_ambiguous)) {
	    translate (defines_list->value, out); puteol (out);
	}
    } else {
	write_list (defines_list, allow_ambiguous, out);
    }
    _free_list (&defines_list);
    if (!isstdout) { fclose (out); }
    return 0;
}
