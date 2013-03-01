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
** Only parts of the between lines consisting entirely of `##EXPORT##´ and
** `##END##´ (both enclosed in comment brackets with no blanks between the
** brackets and the tags) are included into the generated file.
**
** Synopsis:
**    hgen -o outfile template [headerfile...]
**
** If `-o outfile´ is not supplied, the result is written to stdout ...
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

static const char *prog = NULL;

static int isws (int c)
{
    return (c == '\t') || (c == ' ');
}

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

static int haseol (const char *s)
{
    const char *p = s + strlen (s);
    if (p > s && *--p == '\r') { return 2; }
    if (*p == '\n') {
	if (p > s && *--p == '\r') { return 3; }
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

static const char *bn (const char *filename)
{
    const char *res = strrchr (filename, '/');
    if (res) { ++res; } else { res = filename; }
    return res;
}

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
    if (!(tf = fopen (tfname, "r"))) { return -1; }
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

static void set_prog (int argc, char *argv[])
{
    if (argc < 1) {
	prog = "hgen";
    } else if (!argv || !argv[0]) {
	prog = "hgen";
    } else if ((prog = strrchr (argv[0], '/'))) {
	++prog;
    } else {
	prog = argv[0];
    }
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
    printf ("Usage: %s [-o out-header] header-template [header-file...]\n"
	    "       %s -h\n"
	    "\nOptions:"
	    "\n  -h (alt: -help, --help)"
	    "\n    write this text to stdout and terminate"
	    "\n  -o out-header (alt: --output=out-header)"
	    "\n    write result to 'out-header' (instead of stdout)\n",
	    prog, prog);
    exit (0);
}

int main (int argc, char *argv[])
{
    FILE *out;
    int optc = 1, errs, filesc;
    char *outfile = NULL, *tfname, **files;

    set_prog (argc, argv);
    while (optc < argc && *argv[optc] == '-') {
	char *opt = argv[optc++];
	if (!strcmp (opt, "--")) { break; }
	if (!strcmp (opt, "-h") || !strcmp (opt, "-help")
	||  !strcmp (opt, "--help")) {
	    usage (NULL);
	}
	if (!strncmp (opt, "-o", 2)) {
	    if (outfile) { usage ("ambiguous option '-o'"); }
	    if (opt[2]) {
		outfile = &opt[2];
	    } else {
		if (optc >= argc) {
		    usage ("missing argument for option '-o'");
		}
		outfile = argv[optc++];
	    }
	    continue;
	}
	if (!strncmp (opt, "--outfile", sizeof("--outfile") - 1)) {
	    if (outfile) { usage ("ambiguous option '--outfile'"); }
	    opt += sizeof ("--outfile") - 1;
	    if (*opt == '=') {
		++opt; if (!*opt) {
		    usage ("missing argument for option '--outfile'");
		}
		outfile = opt; continue;
	    }
	    if (optc >= argc) {
		usage ("missing argument for option '--output'");
	    }
	    outfile = argv[optc++]; continue;
	}
	usage ("invalid option '%s'", opt);
    }

    if (argc - optc < 1) { usage ("missing argument(s)"); }

    tfname = argv[optc++];

    if (optc >= argc) {
	fprintf (stderr, "%s: WARNING! Header files determined by names"
			 " extracted from the\n"
			 "    template may sometimes not be found\n", prog);
    }

    if (outfile) {
	if (!(out = fopen (outfile, "w"))) {
	    fprintf (stderr, "%s: %s - %s\n", prog, outfile, strerror (errno));
	    exit (1);
	}
    } else {
	out = stdout;
    }

    filesc = argc - optc; files = &argv[optc];
    errs = write_header_file (tfname, outfile, filesc, files, out);

    if (outfile) { fclose (out); out = NULL; }

    return (errs > 0 ? 1 : 0);
}
