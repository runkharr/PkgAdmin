/* hgen.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Generate a header file from a template and a list of input header files.
** Only parts of the between lines consisting entirely of '##EXPORT##' and
** '##END##' (both enclosed in comment brackets with no blanks between the
** brackets and the tags) are included into the generated file.
**
** Synopsis:
**    hgen -o outfile template [headerfile...]
**
** If '-o outfile' is not supplied, the result is written to stdout ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

#define PROG "hgen"

#include "lib/set_prog.c"

#include "lib/isws.c"

#include "lib/haseol.c"

static int isateol (const char *s)
{
    while (isws (*s)) { ++s; }
    if (*s == '\r') {
	if (!*++s) { return 2; }
	if (*s == '\n') { return 3; }
    } else if (*s == '\n') {
	return 1;
    }
    return 0;
}

#define BEGINTAG "/*##EXPORT##*/"
#define BEGINTAG_LEN (sizeof(BEGINTAG) - 1)

#define ENDTAG "/*##END##*/"
#define ENDTAG_LEN (sizeof(ENDTAG) - 1)

static int copy_header_parts (const char *infile, const char *tfname, int tlc,
			      FILE *out)
{
    FILE *in;
    int tf = 0, lc = 0;
    char line[1024];
    if (!(in = fopen (infile, "r"))) {
	fprintf (stderr, "%s: in %s(line %d): %s - %s\n",
			 prog, tfname, tlc, infile, strerror (errno));
	fprintf (out, "#error \"%s\" - %s\n", infile, strerror (errno));
	return -1;
    }
    while (fgets (line, sizeof(line), in)) {
	++lc;
	if (haseol (line)) {
	    if (!strncmp (line, BEGINTAG, BEGINTAG_LEN)
	    &&  isateol (line + BEGINTAG_LEN)) {
		if (!tf) {
		    fprintf (out, "/* From: %s (%d) */\n", infile, lc + 1);
		    fprintf (out, "#line %d \"%s\"\n", lc + 1, infile);
		}
		tf = 1; continue;
	    }
	    if (!strncmp (line, ENDTAG, ENDTAG_LEN)
	    &&  isateol (line + ENDTAG_LEN)) {
		if (tf) {
		    fprintf (out, "/* End %s (%d) */\n", infile, lc - 1);
		    fprintf (out, "#line %d \"%s\"\n", tlc, tfname);
		}
		tf = 0; continue;
	    }
	    if (tf) { fputs (line, out); }
	    continue;
	}
	if (tf) {
	    fputs (line, out);
	    while (fgets (line, sizeof(line), in)) {
		fputs (line, out);
		if (haseol (line)) { break; }
	    }
	}
    }
    fclose (in);
    if (tf) {
	fprintf (out, "/* End %s (%d) */\n", infile, lc - 1);
	fprintf (out, "#line %d \"%s\"\n", tlc, tfname);
    }
    return 0;
}

#define INCLTAG "#include"
#define INCLTAG_LEN (sizeof(INCLTAG) - 1)

#if NAME_MAX < 1024
#define LMAX 1024
#else
#define LMAX NAME_MAX
#endif

#include "lib/bn.c"

static char *find_file (const char *file, int filesc, char *files[])
{
    int ix;
    if (bn (file) == file) { 
	for (ix = 0; ix < filesc; ++ix) {
	    if (!strcmp (file, bn (files[ix]))) { return files[ix]; }
	}
    }
    return NULL;
}

static int write_header_file (const char *tfname, const char *ofname,
			      int filesc, char *files[], FILE *out)
{
    FILE *tf;
    size_t len;
    int lc = 0, ix, errs = 0;
    char ifname[LMAX + 1], line[LMAX + 20], *ifn, *p;
    if (!(tf = fopen (tfname, "r"))) {
	fprintf (stderr, "%s: Attempt to open the template file failed - %s\n",
			 prog, strerror (errno));
	return -1;
    }
    while (fgets (line, sizeof(line), tf)) {
	++lc;
	if (!strncmp (line, INCLTAG, INCLTAG_LEN)) {
	    p = &line[INCLTAG_LEN]; while (isws (*p)) { ++p; }
	    if (*p == '"') {
		++p; ix = 0;
		while (*p && *p != '"') {
		    if (ix < sizeof(ifname) - 1) { ifname[ix++] = *p; }
		    ++p;
		}
		if (*p != '"' || ix >= sizeof(ifname) - 1) {
		    fputs ("#error - invalid include directive\n", out);
		    while (!haseol (line)) {
			if (!fgets (line, sizeof(line), tf)) { break; }
		    }
		    ++errs; continue;
		}
		ifname[ix] = '\0';
		if (filesc > 0) {
		    /* Get the filename from the files list ... */
		    ifn = find_file (ifname, filesc, files);
		} else {
		    /* Try the extracted filename directly ... */
		    ifn = ifname;
		}
		if (!ifn) {
		    fputs (line, out);
		} else {
		    if (copy_header_parts (ifn, ofname, lc, out)) { ++errs; }
		}
	    } else {
		fputs (line, out);
	    }
	} else if (lc == 1 && !strncmp (line, "/* ", sizeof("/* ") - 1)) {
	    p = &line[sizeof("/* ") - 1];
	    if (!strncmp (p, tfname, (len = strlen (tfname)))
	    &&  isateol (p + len)) {
		fputs ("/* ", out); fputs (ofname, out);
		p += len; fputs (p, out);
	    } else {
		fputs (line, out);
	    }
	} else if (lc == 1 && !strncmp (line, "// ", sizeof("// ") - 1)) {
	    p = &line[sizeof("// ") - 1];
	    if (!strncmp (p, tfname, (len = strlen (tfname)))
	    &&  isateol (p + len)) {
		fputs ("// ", out); fputs (ofname, out);
		p += strlen (tfname); fputs (p, out);
	    } else {
		fputs (line, out);
	    }
	} else {
	    fputs (line, out);
	}
	/* Write the remaining part of the line to the output file ... */
	while (!haseol (line)) {
	    if (!fgets (line, sizeof(line), tf)) { break; }
	    fputs (line, out);
	}
    }
    fclose(tf);
    return errs;
}

static void usage (const char *fmt, ...)
{
    if (fmt) {
	va_list pfa;
	fprintf (stderr, "%s: ", prog);
	va_start (pfa, fmt); vfprintf (stderr, fmt, pfa); va_end (pfa);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("Usage: %s [-c directory] [-o out-header] header-template"
	    " header-file...\n"
	    "       %s -h\n"
	    "\nOptions/Arguments:"
	    "\n  -h (alt: -help, --help)"
	    "\n    Write this text to stdout and terminate."
	    "\n  -c directory (alt: --chdir=directory)"
	    "\n    Change into 'directory' before performing any action."
	    "\n  -o out-header (alt: --output=out-header)"
	    "\n    Write result to 'out-header' (instead of stdout)."
	    "\n  header-template"
	    "\n    The template file which is used as a boilerplate for"
	    " generating the"
	    "\n    combined header file."
	    "\n  header-file..."
	    "\n    The header files who are used to create the combined header"
	    " file." 
	    "\n",
	    prog, prog);
    exit (0);
}

/* Get a single short option argument with an argument. Allowed are
** '-<option><argument>' (one-argument form) and '-<option>' '<argument>'
** (two-argument form).
** Returns the option-argument if the given option matched, increasing the
** index '*_optx' or NULL if the option didn't match, leaving '*_optx'
** untouched in this case.
*/
static char *soptarg (const char *opt, int argc, char **argv, int *_optx)
{
    size_t optlen = strlen (opt);
    char *ov = argv[*_optx];
    if (*ov != '-' || strncmp (ov + 1, opt, optlen) != 0) { return NULL; }
    ++optlen;
    if (ov[optlen] != '\0') {
	ov += optlen;
    } else {
	if (*_optx  + 1 >= argc) {
	    usage ("missing argument for option '-%s'", opt);
	}
	ov = argv[++(*_optx)];
    }
    return ov;
}

/* Get a single long option argument with an argument. Allowed are
** '--<option>=<argument>' (one-argument form) and '--<option>' '<argument>'
** (two-argument form).
** Returns the option-argument if the given option matched, increasing the
** index '*_optx' or NULL if the option didn't match, leaving '*_optx'
** untouched in this case.
*/
static char *loptarg (const char *opt, int argc, char **argv, int *_optx)
{
    size_t optlen = strlen (opt);
    char *ov = argv[*_optx];
    if (strncmp (ov, "--", 2) != 0 ||  strncmp (ov + 2, opt, optlen) != 0) {
	return NULL;
    }
    optlen += 2;
    if (ov[optlen] == '=') {
	++optlen;
	if (ov[optlen] == '\0') {
	    usage ("invalid empty argument of option '--%s'", opt);
	}
	ov += optlen;
    } else {
	if (ov[optlen] != '\0') { return NULL; }
	if (*_optx + 1 >= argc) {
	    usage ("missing argument for option '--%s'", opt);
	}
	ov = argv[++(*_optx)];
    }
    return ov;
}

int main (int argc, char *argv[])
{
    FILE *out;
    int optc = 1, errs, filesc;
    char *outfile = NULL, *tfname, **files, *dir = NULL, *v;
    char **non_optv = NULL;
    int non_optc = 0, nox;

    set_prog (argc, argv);
    if (!(non_optv = malloc ((argc + 1) * sizeof(char *)))) {
	fprintf (stderr, "%s: Failed to allocate memory\n", prog);
	exit (1);
    }
    memset (non_optv, 0, (argc + 1) * sizeof(char *));

    for (optc = 1; optc < argc; ++optc) {
	char *opt = argv[optc];
	if (!strcmp (opt, "--")) { ++optc; break; }
	if (!strcmp (opt, "-h") || !strcmp (opt, "-help")
	||  !strcmp (opt, "--help")) {
	    usage (NULL);
	}
	if ((v = soptarg ("c", argc, argv, &optc))) {
	    if (dir) { usage ("ambiguous option '-c'"); }
	    dir = v; continue;
	}
	if ((v = loptarg ("chdir", argc, argv, &optc))) {
	    if (dir) { usage ("ambiguous option '--chdir'"); }
	    dir = v; continue;
	}
	if ((v = soptarg ("o", argc, argv, &optc))) {
	    if (outfile) { usage ("ambiguous option '-o'"); }
	    outfile = v; continue;
	}
	if ((v = loptarg ("outfile", argc, argv, &optc))) {
	    if (outfile) { usage ("ambiguous option '--outfile'"); }
	    outfile = v; continue;
	}
	if (*opt == '-') { usage ("invalid option '%s'", opt); }
	/* Push any non-option argument to 'non_optv' ... */
	non_optv[non_optc++] = opt;
    }
    /* Push the remaining arguments (after a '--') to 'non_optv' ... */
    while (optc < argc) {
	non_optv[non_optc++] = argv[optc++];
    }
    non_optv[non_optc] = NULL;

    if (non_optc < 1) { usage ("missing argument(s)"); }

    nox = 0; tfname = non_optv[nox++];

    if (optc >= argc) {
	fprintf (stderr, "%s: WARNING! Header files determined by names"
			 " extracted from the\n"
			 "    template may sometimes not be found\n", prog);
    }

    if (dir && chdir (dir) != 0) {
	fprintf (stderr, "%s: Changing into directory '%s' failed - %s\n",
			 prog, dir, strerror (errno));
	exit (1);
    }

    if (outfile) {
	if (!(out = fopen (outfile, "w"))) {
	    fprintf (stderr, "%s: %s - %s\n", prog, outfile, strerror (errno));
	    exit (1);
	}
    } else {
	out = stdout;
    }

    filesc = non_optc - nox; files = &non_optv[nox];
    errs = write_header_file (tfname, outfile, filesc, files, out);

    if (outfile) { fclose (out); out = NULL; }

    return (errs > 0 ? 1 : 0);
}
