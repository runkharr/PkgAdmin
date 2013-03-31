/* genflist.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2011, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Scan a complete directory tree for files and write the names of these files
** to stdout.
**
** Synopsis:
**
**    genflist [-f|-d] [-n] path
**
** Options:
**   -f (full-path)
**     write the pathnames including the 'path'-parameter
**   -d
**     prepend a '.' to each pathname printed
**   -n (no directiroes)
**     don't write the names of directories
**
** vim: set tabstop=8 shiftwidth=4 noexpandtab:
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>

#define PROG "genflist"

#include "lib/set_prog.c"
#include "lib/mrmacs.c"
#include "lib/sdup.c"
#include "lib/slist.c"
#include "lib/cwd.c"
#include "lib/pbCopy.c"
#include "lib/trans_path.c"
#include "lib/travdir-types.c"
#include "lib/travdirne.c"
#include "lib/travdir.c"

static
int get_filetype (const char *path, int *_filetype)
{
    struct stat sb;
    /* I don't want to follows symbolic links (using 'lstat()') ... */
    if (lstat (path, &sb)) { return -1; }
    if (S_ISDIR (sb.st_mode)) {
	*_filetype = FT_DIRECTORY;
    } else {
	/* The remaining file types are of rather little concern, so they are
	** all treated as regular files here (returning FT_FILE) ...
	*/
	*_filetype = FT_FILE;
    }
    return 0;
}

static
int is_pprefix (const char *p, const char *s)
{
    int c;
    while ((c = *p++) == *s++ && c);
    if (c) { return 0; }
    --s;
    return (!*s || *s == '/');
}

struct pe_data {
    int cut_prefix, add_dot;
    char *prefix;
    slist_t flf, flp;
};

static
int process_entry (const char *path, int filetype, void *data)
{
    struct pe_data *pe = (struct pe_data *) data;
    if (pe->cut_prefix && is_pprefix (pe->prefix, path)) {
	path += strlen (pe->prefix);
    }
    if (!*path) {
	if (slist_append (pe->flp, (pe->add_dot ? "." : "/"))) {
	    return -1;
	}
	if (!pe->flf) { pe->flf = pe->flp; }
	return 0;
    }
    if (pe->add_dot) {
	char *npath = t_allocv (char, strlen (path) + 2);
	if (!npath) { return -1; }
	*npath = '.'; pbCopy (npath + 1, path);
	if (slist_append (pe->flp, npath)) { free (npath); return -1; }
	free (npath);
    } else {
	if (slist_append (pe->flp, path)) { return -1; }
    }
    if (!pe->flf) { pe->flf = pe->flp; }
    return 0;
}

typedef int (*trav_t) (char **_buf, size_t *_bufsz,
		       const char *dirname, getfiletype_t get_filetype,
		       travop_t travop, void *travdata);

static
void usage (const char *format, ...)
{
    if (format) {
	va_list ual;
	fprintf (stderr, "%s: ", prog);
	va_start (ual, format); vfprintf (stderr, format, ual); va_end (ual);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("Usage: %s [-d|-f] [-n] path\n"
	    "       %s -h\n"
	    "\nOptions:"
	    "\n  -d (dot-add)"
	    "\n    prepend a '.' to each path printed"
	    "\n  -f (full-path)"
	    "\n    expand each entry's pathname to an absolute pathname"
	    "\n  -h (help)"
	    "\n    display this text and terminate"
	    "\n  -n (no empty dirs)"
	    "\n    do not process (print) the pathnames of empty directories\n",
	    prog, prog);
    exit (0);
}

int main (int argc, char *argv[])
{
    char *buf = NULL, *path = NULL;
    size_t bufsz = 0;
    int rc, optx, opt_f = 0, opt_d = 0, opt_n = 0;
    struct pe_data pe;
    slist_t lp;
    trav_t trav = travdir;

    set_prog (argc, argv);
    pe.cut_prefix = 1; pe.add_dot = 0;
    pe.prefix = NULL; pe.flf = pe.flp = NULL;

    if (argc < 2) { usage (NULL); }
    for (optx = 1; optx < argc; ++optx) {
	if (*argv[optx] != '-') { break; }
	if (!strcmp (argv[optx], "-")) { ++optx; break; }
	if (!strcmp (argv[optx], "-d")) {
	    if (opt_f) { usage ("can't use '-d' and '-f' together"); }
	    opt_d = 1; continue;
	}
	if (!strcmp (argv[optx], "-f")) {
	    if (opt_d) { usage ("can't use '-f' and '-d' together"); }
	    opt_f = 1; continue;
	}
	if (!strcmp (argv[optx], "-h")) { usage (NULL); }
	if (!strcmp (argv[optx], "-n")) { opt_n = 1; continue; }
	usage ("invalid option '%s'", argv[optx]);
    }

    /* Establish the configuration settings ... */
    if (opt_d) { pe.add_dot = 1; }
    if (opt_f) { pe.cut_prefix = 0; }
    if (opt_n) { trav = travdirne; }

    /* Check for one argument (ignore any remaining ones after the first one)
    * ...
    */
    if (optx >= argc) { usage ("missing argument"); }

    /* Make the argument an absolute pathname ... */
    if (*argv[optx] != '/') {
	char *p;
	const char *wd = cwd ();
	ifnull (path = t_allocv (char, strlen (wd) + strlen (argv[optx]) + 2)) {
	    fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
	}
	p = pbCopy (path, wd); p = pbCopy (p, "/"); pbCopy (p, argv[optx]);
    } else ifnull (path = sdup (argv[optx])) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
    }

    /* Normalize the pathname ... */
    if (trans_path (path, path)) {
	usage ("\%s\" - invalid pathname", argv[optx]);
    }

    /* Establish the pathname in the configuration ... */
    pe.prefix = path;

    /* Traverse through the directory tree specified by the pathname ... */
    rc = trav (&buf, &bufsz, path, get_filetype, process_entry, (void *) &pe);
    if (rc) { fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1); }

    /* Write the (resulting) list of pathnames to stdout ... */
    for (lp = pe.flf; lp; lp = lp->next) { printf ("%s\n", lp->sval); }
    fflush (stdout);

    /* Free the allocated memory ... */
    slist_free (pe.flf);
    free (pe.prefix); pe.prefix = NULL;

    /* Return (indicating success) ... */
    return 0;
}
