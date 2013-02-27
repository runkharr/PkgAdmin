/* uninstall.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2011, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Un-install helper
**
** Synopsis:
**
**     uninstall [-d|-D directory] [-q|-v] file-path...
**
** With `-d directory´ any element of `file-path´ which is a relative pathname
** (e.g. a pure file-name) is searched below `directory´ and deleted if found;
** additionally, if `directory´ is empty after removing all of the
** `file-path...´-elements, the directory is removed itself ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>

static int
dir_is_empty (const char *dir)
{
    int cc = 0;
    const char *p;
    struct dirent *de = NULL;
    DIR *dp = opendir (dir);
    if (!dp) { return -1; }
    while ((de = readdir (dp))) {
        p = de->d_name;
        /* Don't know if this is required: if (!*p) { continue; }*/
        if (*p == '.') {
            if (*++p == '\0') { continue; }
            if (*p == '.' && *++p == '\0') { continue; }
        }
        ++cc;
    }
    closedir (dp);
    return cc == 0;
}

static const char *prog = NULL;

static void
set_prog (const char *av0)
{
    char *b;
    const char *p;
    if (!av0) { p = "uninstall"; }
    else if ((p = strrchr (av0, '/'))) { ++p; } else { p = av0; }
    if (!(b = malloc (strlen (p) + 1))) {
        fprintf (stderr, "%s: %s\n", p, strerror (errno)); exit (1);
    }
    strcpy (b, p);
    prog = b;
}

static char *
pbCopy (char *d, const char *s)
{
    while ((*d++ = *s++));
    return --d;
}

static char *
conc (char **_res, size_t *_ressz, const char *s0, ...)
{
    va_list sX, sY;
    char *res = *_res, *p;
    const char *sx;
    size_t ressz = *_ressz;
    if (!s0) { return res; }
    ressz = 1 + strlen (s0);
    va_start (sX, s0);
    va_copy (sY, sX);
    while ((sx = va_arg (sX, const char *))) {
        ressz += strlen (sx);
    }
    va_end (sX);
    if (ressz > *_ressz) {
        if (!(res = realloc (*_res, ressz))) { return res; }
        *_res = res; *_ressz = ressz;
    }
    p = res;
    p = pbCopy (p, s0);
    while ((sx = va_arg (sY, const char *))) {
        p = pbCopy (p, sx);
    }
    return res;
}

static void
usage (const char *fmt, ...)
{
    va_list pfa;
    if (fmt) {
        fprintf (stderr, "%s: ", prog);
        va_start (pfa, fmt); vfprintf (stderr, fmt, pfa); va_end (pfa);
        fputs ("\n", stderr);
        exit (64);
    }
    printf ("Usage: %s [-d|-D dirname] [-q] file...\n"
            "       %s -h\n"
            "\nOptions:"
            "\n  -d dirname"
            "\n    treat relative pathnames in the list 'file...' as being"
            " relative to"
            "\n    'dirname'"
	    "\n  -D dirname"
	    "\n    same as '-d dirname', but additionally remove the directory"
	    " 'dirname' if"
	    "\n    it is empty after deleting the last of the specified files"
            "\n  -h"
            "\n    display this text and terminate\n"
            "\n  -q"
            "\n    don't display error messages. An exit-code != 0 (indicating"
            " errors) is"
            "\n    returned nonetheless"
	    "\n  -v"
	    "\n    display each file to be removed (including error"
	    " messages)\n",
            prog, prog);
    exit (0);
}

static void
print_state (const char *file, int verbosity, int rc)
{
    if (verbosity >= 2) {
	if (rc) {
	    printf (" failed (%s)\n", strerror (errno));
	} else {
	    fputs (" done\n", stdout);
	}
    } else if (verbosity > 0) {
	fprintf (stderr, "%s: %s - %s\n", prog, file, strerror (errno));
    }
}

static int
is_directory (const char *path)
{
    struct stat sb;

    if (stat (path, &sb) != 0) { return -1; }
    return (S_ISDIR (sb.st_mode) ? 1 : 0);
}

int
main (int argc, char *argv[])
{
    int opt, verbosity = 1, errcc = 0, remove_directory = 0, rc;
    char *dirname = NULL;
    char *path = NULL, *file;
    size_t pathsz = 0;

    set_prog (argv[0]);
    while ((opt = getopt (argc, argv, "D:d:hqv")) != -1) {
        switch (opt) {
            case 'd': case 'D':
                if (dirname) { usage ("ambiguous '-%c'-option", opt); }
                dirname = optarg;
		if (opt == 'D') { remove_directory = 1; }
		if ((rc = is_directory (dirname)) <= 0) {
		    if (rc < 0) {
			usage ("'%s' - %s", dirname, strerror (errno));
		    }
		    usage ("'%s' - no directory", dirname);
		}
                break;
            case 'h':
                usage (NULL);
            case 'q':
                verbosity = 0;
                break;
	    case 'v':
		++verbosity; if (verbosity > 2) { verbosity = 2; }
		break;
            default:
                usage ("invalid option '%s'", argv[optind]);
        }
    }

    if (optind >= argc) {
        usage ("missing argument(s); try '%s -h' for help, please!", prog);
    }

    for (; optind < argc; ++optind) {
        if (*(file = argv[optind]) == '/' || !dirname) {
            file = conc (&path, &pathsz, file, NULL);
        } else {
            file = conc (&path, &pathsz, dirname, "/", file, NULL);
        }
        if (!file) {
            fprintf (stderr, "%s: %s\n", prog, strerror (errno)); exit (1);
        }
	if (verbosity >= 2) {
	    printf ("Removing %s ...", file); fflush (stdout);
	}
	
	if ((rc = unlink (file)) != 0) { ++errcc; }
	print_state (file, verbosity, rc);
    }
    if (errcc == 0 && dirname && dir_is_empty (dirname) && remove_directory) {
	if (verbosity >= 2) {
	    printf ("Removing directory %s ...", dirname);
	}
	if ((rc = rmdir (dirname)) != 0) { ++errcc; }
	print_state (dirname, verbosity, rc);
    }
    if (path) { free (path); path = NULL; pathsz = 0; }
    return (errcc > 0 ? 1 : 0);
}
