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

#include "lib/printarg.c"

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

/* Insert a '#line' preprocessor command into the outfile ... */
static void line_to (const char *ofname, int lc, FILE *out)
{
    fprintf (out, "# line %d \"%s\"\n", lc, ofname);
}

/* Copy each part of 'infile' which is enclosed into BEGINTAG and ENDTAG into
** the output file. For the definitions of BEGINTAG and ENDTAG, see above,
** please!
*/
static int copy_header_parts (const char *infile, const char *tfname, int tlc,
			      FILE *out)
{
    FILE *in;
    int tf = 0, lc = 0, waseol;
    char line[1024];
    if (!(in = fopen (infile, "r"))) {
	fprintf (stderr, "%s: in %s(line %d): %s - %s\n",
			 prog, tfname, tlc, infile, strerror (errno));
	fprintf (out, "#error \"%s\" - %s\n", infile, strerror (errno));
	return -1;
    }
    waseol = 1;
    while (fgets (line, sizeof(line), in)) {
	++lc;
	if (waseol) {
	    if (!strncmp (line, BEGINTAG, BEGINTAG_LEN) &&
		isateol (line + BEGINTAG_LEN)) {
		if (!tf) {
		    fprintf (out, "/* From: %s (%d) */\n", infile, lc + 1);
		    line_to (infile, lc + 1, out);
		}
		tf = 1;
	    } else if (!strncmp (line, ENDTAG, ENDTAG_LEN) &&
		       isateol (line + ENDTAG_LEN)) {
		if (tf) {
		    fprintf (out, "/* End %s (%d) */\n", infile, lc - 1);
		}
		tf = 0;
	    } else {
		if (tf) { fputs (line, out); }
	    }
	    waseol = haseol (line);
	} else {
	    if (tf) { fputs (line, out); }
	    waseol = haseol (line);
	}
    }
    fclose (in);
    if (tf) { fprintf (out, "/* End %s (%d) <EOF> */\n", infile, lc); }
    return 0;
}

#define INCLTAG "#include"
#define INCLTAG_LEN (sizeof(INCLTAG) - 1)

#define IMPORTTAG1 "/*##IMPORT##*/"
#define IMPORTTAG1_LEN (sizeof(IMPORTTAG1) - 1)

#define IMPORTTAG2 "//##IMPORT##"
#define IMPORTTAG2_LEN (sizeof(IMPORTTAG2) - 1)

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

typedef struct lines *lines_t;
struct lines {
    lines_t next;
    int continued;
    char text[1];
};

static void add_line (lines_t *_first, lines_t *_last, const char *text, int cl)
{
    lines_t res = malloc (sizeof (struct lines) + strlen (text) + 1);
    if (!res) {
	fprintf (stderr, "%s: %s\n", prog, strerror (errno));
	exit (71);
    }
    res->next = (lines_t) NULL;
    res->continued = cl;
    strcpy (res->text, text);
    if (*_first) { (*_last)->next = res; } else { (*_first) = res; }
    (*_last) = res;
}

static void free_lines (lines_t *_first)
{
    int ec = errno;
    while (*_first) {
	lines_t next = (*_first)->next;
	(*_first)->next = (lines_t) NULL;
	memset ((*_first)->text, 0, strlen ((*_first)->text));
	free ((*_first));
	*_first = next;
    }
    errno = ec;
}

static int read_template (const char *tfname, lines_t *_out)
{
    FILE *tf;
    lines_t first = (lines_t) NULL, last = (lines_t) NULL;
    char line[LMAX + 20]; int lc;
    if (!(tf = fopen (tfname, "r"))) {
	fprintf (stderr, "%s: Attempt to open the template file failed - %s\n",
			 prog, strerror (errno));
	return -1;
    }
    lc = 0;
    while (fgets (line, sizeof(line), tf)) {
	++lc;
	add_line (&first, &last, line, 0);
	if (!haseol (line)) {
	    fprintf (stderr, "%s(%d): WARNING! Line too long ...\n",
			     tfname, lc);
	    while (fgets (line, sizeof(line), tf)) {
		add_line (&first, &last, line, 1);
		if (haseol (line)) { break; }
	    }
	}
    }
    fclose (tf);
    *_out = first;
    return 0;
}

static lines_t process_remaining (lines_t cline, int skip_text, FILE *out)
{
    if (cline) {
	cline = cline->next;
	if (skip_text) {
	    while (cline && cline->continued) { cline = cline->next; }
	} else {
	    while (cline && cline->continued) {
		fputs (cline->text, out);
		cline = cline->next;
	    }
	}
    }
    return cline;
}

static void rplc_filename (const char *line, const char *old, const char *new,
			   FILE *out)
{
    const char *p, *q = line;
    while ((p = strstr (q, old))) {
	fwrite (q, 1, (size_t)(p - q), out);
	fputs (new, out); q = p + strlen (old);
    }
    fputs (q, out);
}

static int is_import_tag (const char *line)
{
    const char *p = line;
    while (isws (*p)) { ++p; }
    return strncmp (p, IMPORTTAG1, IMPORTTAG1_LEN) == 0 ||
	   strncmp (p, IMPORTTAG2, IMPORTTAG2_LEN) == 0;
}


static int is_include_line (const char *line, char *ifname, size_t ifnamesz)
{
    if (strncmp (line, INCLTAG, INCLTAG_LEN) == 0) {
	const char *p = line + INCLTAG_LEN, *q;
	while (isws (*p)) { ++p; } 
	if (*p != '"') { return 0; }
	++p; q = p; while (*q && *q != '"') { ++q; }
	if (*q != '"') { return 0; }
	if ((size_t) (q - p) >= ifnamesz) { errno = EINVAL; return -1; }
	memcpy (ifname, p, (size_t) (q - p)); ifname[q - p] = '\0';
	return 1;
    }
    return 0;
}

static void invalid_include (FILE *out)
{
    fputs ("#error Invalid '#include \"...'-line.\n", out);
}

static int write_header_file (const char *tfname, const char *ofname,
			      int filesc, char *files[], FILE *out)
{
    int lc = 0, ix, errs = 0, impmode = 0, need_skip = 0, isinc;
    char ifname[LMAX + 1], *ifn;
    lines_t template = (lines_t) NULL, cline;
    if (read_template (tfname, &template)) {
	free_lines (&template); return -1;
    }
    lc = 0; cline = template;
    while (cline) {
	if (is_import_tag (cline->text)) {
	    if (impmode > 0) {
		fprintf (stderr,
			 "%s(%d): Ignoring further occurrences of the"
			 " import tag.\n", tfname, lc);
	    }
	    ++impmode;
	}
	cline = process_remaining (cline, 1, out);
    }
    if (impmode > 0) {
	/* Create an output file where all include-files given as arguments
	** are inserted at the first occurrence of IMPORTTAG1 or IMPORTTAG2.
	** Each line beginning with '#include "`file`"' or '#include <`file`>',
	** where `file` is one of the files given as arguments is converted
	** into a comment in the output file. Additionally, each occurrence
	** of `tfname` in the template file is replaced with the name of the
	** output file ...
	*/
	need_skip = 0;
	impmode = 0; lc = 0; cline = template;
	while (cline) {
	    ++lc;
	    /* Deactivate each '#include "file"' or '#include <file>'-line
	    ** where 'file' is found in the list of files to be (partially)
	    ** included. Additionally, replace a line containing
	    ** '#include "`ofname`"' or '#include <`ofname`>' with an
	    ** error line ...
	    */
	    isinc = is_include_line (cline->text, ifname, sizeof(ifname));
	    if (isinc != 0) {
		if (isinc < 0) {
		    invalid_include (out);
		    need_skip = 1;
		} else {
		    if (filesc > 0 && find_file (ifname, filesc, files)) {
			fprintf (out,
				 "#warning Deactivated '#include \"%s\"'\n",
				 ifname);
			need_skip = 1;
		    } else if (strcmp (ifname, ofname) == 0) {
			fprintf (out,
				 "#error File \"%s\" must not #include"
				 " itself\n", ofname);
			fputs (ofname, out);
			need_skip = 1;
		    } else {
			fputs (cline->text, out);
			need_skip  = 0;
		    }
		}
	    } else if (is_import_tag (cline->text)) {
		/* Insert all files in the list at the position of the first
		** input file.
		*/
		if (impmode > 0) {
		    fputs (cline->text, out);
		    need_skip = 0;
		} else {
		    for (ix = 0; ix < filesc; ++ix) {
			const char *ifn = files[ix];
			if (copy_header_parts (ifn, ofname, lc, out)) {
			    ++errs;
			}
		    }
		    line_to (ofname, lc, out);
		    need_skip = 1;
		}
	    } else {
		/* Replace each occurrence of `tfname` in the first part of the
		** with `ofname` ...
		*/
		rplc_filename (cline->text, tfname, ofname, out);
		need_skip = 0;
	    }
	    cline = process_remaining (cline, need_skip, out);
	}
    } else {
	/* Revert to the original mode of action ... */
	lc = 0; cline = template;
	while (cline) {
	    ++lc;
	    isinc = is_include_line (cline->text, ifname, sizeof(ifname));
	    if (isinc != 0) {
		if (isinc < 0) {
		    invalid_include (out);
		    need_skip = 1;
		} else {
		    /* If there were the names of header-files supplied in
		    ** the invocation, the `ifname` is searched only in
		    ** this list. Otherwise, the `ifname` is used directly
		    ** as an input file.
		    */
		    if (filesc > 0) {
			/* Get the filename from the files list ... */
			ifn = find_file (ifname, filesc, files);
		    } else {
			/* Try the extracted filename directly ... */
			ifn = ifname;
		    }
		    if (!ifn) {
			fputs (cline->text, out);
			need_skip = 0;
		    } else {
			if (copy_header_parts (ifn, ofname, lc, out)) {
			    ++errs;
			}
			line_to (ofname, lc, out);
			need_skip = 1;
		    }
		}
	    } else {
		/* In any other case, write the line - with each occurrence of
		** the original file name of the template file replaced with
		** that of the output file (at least in the first LMAX columns
		** of the line) directly to the output file ...
		*/
		rplc_filename (cline->text, tfname, ofname, out);
		need_skip = 0;
	    }
	    cline = process_remaining (cline, need_skip, out);
	}
    }
    /* Free the "memory" version of the template file ... */
    free_lines (&template);
    /* Return the number of errors occured while inserting ... */
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
    printf ("Usage: %s [-v] [-c directory] [-o out-header] header-template"
	    " header-file...\n"
	    "       %s [-h]\n"
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
	    "\n  -v (alt: --verbose)"
	    "\n    Write the complete command (as invoked) to the standard"
	    " output. If this"
	    "\n    option is not specified, only a short message about the"
	    " creation of the"
	    "\n    output file is written."
	    "\n  header-file..."
	    "\n    The header files who are used to create the combined header"
	    " file." 
	    "\n",
	    prog, prog);
    exit (0);
}

/* Getting a short or long option without argument. In the case of the long
** option, any prefix of this option is valid. Returns 1 if the corresponding
** option was detected and 0, otherwise.
*/
static int nvopt (const char *sopt, const char *lopt,
		  int argc, char **argv,
		  int *_optx)
{
    size_t optlen = strlen (sopt);
    char *ov = argv[*_optx];
    if (sopt) {
	if (*ov != '-') { return 0; }
	if (ov[1] != '-') {
	    return strcmp (ov + 1, sopt) == 0;
	}
    }
    if (lopt) {
	size_t ovlen = strlen (ov);
	if (*ov != '-' || ov[1] != '-' || ovlen < 3) { return 0; }
	ov += 2; ovlen -= 2;
	return ovlen <= optlen && strncmp (ov, lopt, ovlen) == 0;
    }
    return 0;
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
    int verbose = 0;

    set_prog (argc, argv);
    if (!(non_optv = malloc ((argc + 1) * sizeof(char *)))) {
	fprintf (stderr, "%s: Failed to allocate memory\n", prog);
	exit (1);
    }
    memset (non_optv, 0, (argc + 1) * sizeof(char *));

    /* Allowing an empty argument list - issuing the usage message in this
    ** case.
    */
    if (argc < 2) { usage (NULL); }

    for (optc = 1; optc < argc; ++optc) {
	char *opt = argv[optc];
	if (!strcmp (opt, "--")) { ++optc; break; }
	if (nvopt ("h", "help", argc, argv, &optc)) { usage (NULL); }
	if (!strcmp (opt, "-help")) { usage (NULL); }
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
	if (nvopt ("v", "verbose", argc, argv, &optc)) {
	    verbose = 1; continue;
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

    if (non_optc < 1) {
	fprintf (stderr, "%s: WARNING! Header files determined by names"
			 " extracted from the\n"
			 "    template may sometimes not be found\n", prog);
    }

    if (dir && chdir (dir) != 0) {
	fprintf (stderr, "%s: Changing into directory '%s' failed - %s\n",
			 prog, dir, strerror (errno));
	exit (1);
    }

    if (verbose) {
	int ix;
	fputs (prog, stdout);
	for (ix = 1; ix < argc; ++ix) { print_arg (argv[ix], stdout); }
	fputs ("\n", stdout);
    } else {
	printf ("Creating %s ...", (outfile ? outfile : "in <stdout>"));
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
    if (!verbose) { fputs (" done.\n", stdout); }

    return (errs > 0 ? 1 : 0);
}
