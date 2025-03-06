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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

#include "lib/bn.c"
#include "lib/fmt.c"
#include "lib/haseol.c"
#include "lib/int2str.c"
#include "lib/isws.c"
#include "lib/printarg.c"

#define PROG "hgen"

/* Some convenience macros: String comparison.
*/
#define streq(x, y) (strcmp ((x), (y)) == 0)
#define streqn(x, y, n) (strncmp ((x), (y), (n)) == 0)
#define strnen(x, y, n) (strncmp ((x), (y), (n)) != 0)

/* Convenience macro for replacing the annoying `strerror (errno))` in many
** places.
*/
#define ERRSTR (strerror (errno))

/* Because i didn't want to handle dynamically allocated lines, i set a
** maximum line length here. This means that the maximum length of all lines
** in a header template file cannot exceed the value of `LMAX` (defined here).
*/
#if NAME_MAX < 1024
#define LMAX 1024
#else
#define LMAX NAME_MAX
#endif

/* Marker for the beginning of a section to be exported from the local header
** file to output header file.
*/
#define BEGINTAG "/*##EXPORT##*/"
#define BEGINTAG_LEN (sizeof(BEGINTAG) - 1)

/* Marker for the end of a section to be exported from the local header
** file to output header file.
*/
#define ENDTAG "/*##END##*/"
#define ENDTAG_LEN (sizeof(ENDTAG) - 1)

/* Marker of the insert point for a specific header file.
** OLDER SCHEME: Used for template files consisting of a set of `#include`
** preprocessor statements. The name in the `#include` file will be directly
** used to identify the corresponding header file which replaces the whole
** `#include` statement in the result.
*/
#define INCLTAG "#include"
#define INCLTAG_LEN (sizeof(INCLTAG) - 1)

/* Marker of the insert point for the content exported from the header files.
** NEWER SCHEME: Here all header files specified in the command line of `hgen`
** are inserted into the result, replacing this marker.
*/
#define IMPORTTAG1 "/*##IMPORT##*/"
#define IMPORTTAG1_LEN (sizeof(IMPORTTAG1) - 1)

#define IMPORTTAG2 "//##IMPORT##"
#define IMPORTTAG2_LEN (sizeof(IMPORTTAG2) - 1)

/* Global variable holding the program name.
*/
static const char *prog;

/* Small function for testing the given line ending with an EOL marker
** (OS-agnostic).
*/
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

static const char *line2str (int lc, char *out, size_t outsz)
{
    if (lc < 0) { errno = EINVAL; return NULL; }
    return int2str (lc, out, outsz, false);
}

/* Insert a '#line' preprocessor command into the outfile ... */
static void line_to (const char *ofname, int lc, FILE *out)
{
    char numbuf[32];
    line2str (lc, numbuf, sizeof(numbuf));
    fmt_print (out, "# line $1 \"$2\"\n", numbuf, ofname);
}

/* Copy each part of 'infile' which is enclosed into BEGINTAG and ENDTAG into
** the output file. For the definitions of BEGINTAG and ENDTAG, see above,
** please!
**
** The header file being processed can have any number of esport sections
** (even zero). The only thing important here is that each of these sections
** must begin with the export marker and end with the end marker.
**
** The line size in the template file is limited to `LMAX` bytes, but this
** doesn't mean that parts of the header files inserted are in any way limited.
** 
*/
static int copy_header_parts (const char *infile, const char *tfname,
			      int *_tlc, FILE *out)
{
    FILE *in;
    bool inserting = false;
    int lc = 0, tlc = *_tlc, waseol;
    char line[1024], numbuf[32];
    if (!(in = fopen (infile, "r"))) {
	line2str (tlc, numbuf, sizeof(numbuf));
	fmt_print (stderr, "$1: in $2(line $3): $4, $5\n",
			    prog, tfname, numbuf, infile, ERRSTR);
	fmt_print (out, "#error \"$1\" - $2\n", infile, ERRSTR);
	*_tlc = tlc;
	return -1;
    }
    waseol = 1;
    while (fgets (line, sizeof(line), in)) {
	++lc;
	if (waseol) {
	    ++tlc;
	    if (streqn (line, BEGINTAG, BEGINTAG_LEN) &&
		isateol (line + BEGINTAG_LEN)) {
		if (! inserting) {
		    line2str (lc + 1, numbuf, sizeof(numbuf));
		    fmt_print (out, "/* From: $1 ($2) */\n", infile, numbuf);
		    line_to (infile, lc + 1, out);
		    ++tlc;
		}
		inserting = true;
	    } else if (streqn (line, ENDTAG, ENDTAG_LEN) &&
		       isateol (line + ENDTAG_LEN)) {
		if (inserting) {
		    line2str (lc - 1, numbuf, sizeof(numbuf));
		    fmt_print (out, "/* End $1 (S2) */\n", infile, numbuf);
		    ++tlc;
		}
		inserting = false;
	    } else {
		if (inserting) { fputs (line, out); }
		//if (tf) { fputs (line, out); }
	    }
	    waseol = haseol (line);
	} else {
	    if (inserting) { fputs (line, out); }
	    //if (tf) { fputs (line, out); }
	    waseol = haseol (line);
	}
    }
    fclose (in);
    if (inserting) {
	line2str (lc, numbuf, sizeof(numbuf));
	fmt_print (out, "/* End $1 ($2) <EOF> */\n", infile, numbuf);
	++tlc; *_tlc = tlc; errno = EOVERFLOW; return -1;
    }
    *_tlc = tlc;
    return 0;
}

static const char *find_file (const char *file,
			      int filesc, char *const *files)
{
    int ix;
    if (bn (file) == file) { 
	for (ix = 0; ix < filesc; ++ix) {
	    if (streq (file, bn (files[ix]))) { return files[ix]; }
	}
    }
    return NULL;
}

/* LIST TYPE for collecting the template lines.
*/
typedef struct lines *lines_t;
struct lines {
    lines_t next;
    bool continued;
    char text[1];
};

/* Add a line (or line continuation) to the given list of lines.
** Lines seemingly too long will be splitted into multiple entries with the
** `continued` flag of the first line and all continuations (save from the
** last one) being set.
*/
static void add_line (lines_t *_first, lines_t *_last, const char *text,
		      bool cl)
{
    lines_t res = (lines_t) malloc (sizeof (struct lines) + strlen (text) + 1);
    if (!res) {
	fmt_print (stderr, "$1: $2\n", prog, ERRSTR);
	exit (71);
    }
    res->next = (lines_t) NULL;
    res->continued = cl;
    strcpy (res->text, text);
    if (*_first) { (*_last)->next = res; } else { (*_first) = res; }
    (*_last) = res;
}

/* Deallocate the complete list of lines. The given parameter (if not `NULL`)
** must point to a pointer variable pointing to a line list. This parameter is
** an in/out parameter, meaning; the variable pointed to by it will be set to
** `NULL` at the end of this function.
** The error state (`errno`) is preserved.
*/
static void free_lines (lines_t *_first)
{
    if (_first) {
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
}

/* Read a header template from a given file and store it in a list whose
** starting element is returned through the parameter `out`.
** Any line which is too long is splitted, but a warning about this case
** is written to `stderr`. This functions returns either `-1` (if opening
** the template file failed), or the number of lines recognised as too long
** (which should be `0` in normal circumstances).
*/
static int read_template (const char *tfname, lines_t *_out)
{
    int ec, errc;	/* errno saver, error counter */
    FILE *tf;
    lines_t first = (lines_t) NULL, last = (lines_t) NULL;
    char line[LMAX + 20], numbuf[32]; int lc;
    if (!(tf = fopen (tfname, "r"))) {
	ec = errno;
	fmt_print (stderr,
		   "$1: Attempt to open the template file failed - $2\n",
		   prog, ERRSTR);
	errno = ec;
	return -1;
    }
    lc = 0; errc = 0;
    while (fgets (line, sizeof(line), tf)) {
	int eol = haseol (line);
	++lc; add_line (&first, &last, line, (eol == 0));
	if (! eol) {
	    line2str (lc, numbuf, sizeof(numbuf));
	    fmt_print (stderr, "$1($2): WARNING! Line too long ...\n",
			       tfname, numbuf);
	    ++errc;
	    while (fgets (line, sizeof(line), tf)) {
		eol = haseol (line);
		add_line (&first, &last, line, (eol == 0));
		if (eol > 0) { break; }
	    }
	}
    }
    fclose (tf);
    /* The `continued` flag of the (last part of the) last line should always
    ** be reset - evfen if the line doesn't contain a line terminator. In the
    ** M$ world, this seems to be the regular case.
    **
    ** ATTENTION! the lines list of the template may be empty, so last
    ** can have a `NULL` value. So, only if `last` is not `NULL`, the
    ** `continued` can be modified.
    */
    if (last) { last->continued = false; }
    *_out = first;
    return errc;	// Return the number 
}

/* Send the remaining lines of the template (after the last import marker was
** found) to the given file `out` or simply skip them.
** The schema is as follows:
**   During reading the line (`read_template()`) lines longer than the maximum
**   buffer length are split, so that for all parts without a line terminator
**   (EOL) the `continued` flag is set. If the (part of) the line containing
**   the line terminator is found, its `continued` flag is reset - indicating
**   that the line is now complete.
*/
static lines_t process_remaining (lines_t cline, int skip_text, FILE *out)
{
    if (cline) {
	if (skip_text) {
	    while (cline && cline->continued) { cline = cline->next; }
	} else {
	    while (cline && cline->continued) {
		fputs (cline->text, out);
		cline = cline->next;
	    }
	    // print the remaining part of the line
	    if (cline) { fputs (cline->text, out); }
	}
	// Skip the remaining part of the line
	if (cline) { cline = cline->next; }
    }
    return cline;
}

#if 0
/* Convert a filename in a line - replacing the string `old` with the string
** `new`. The converted name is directly written to the output file `out`.
*/
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
#endif

// Identify an import tag (`/*##IMPORT##*/` or `//##IMPORT##`) in the given
// string `line`
//
static bool is_import_tag (const char *line)
{
    const char *p = line;
    while (isws (*p)) { ++p; }
    return streqn (p, IMPORTTAG1, IMPORTTAG1_LEN) ||
	   streqn (p, IMPORTTAG2, IMPORTTAG2_LEN);
}

/* Identify an `#include` or `#import` preprocessor command. On success
** `true` is returned and the second and third paramaters of this function
** set to `true` or `false` (depending if an `#import` or `#include` command
** was found), and the position of the `#include`/`#import` string within the
** line.
*/
static bool is_include_cmd (const char *line, bool *_is_import, int *_pos)
{
    /* An `#include` (or at least the `#` of it always starts at the beginning
    ** of the line.
    */
    bool res = false, import = false;
    if (*line == '#') {
	const char *p = line;
	while (isws (*++p));
	if ((import = streqn (p, "import", 6)) || streqn (p, "include", 7)) {
	    p += 6 + (import ? 0 : 1);
	    while (isws (*p)) { ++p; }
	    if (*p == '<' || *p == '"') {
		*_is_import = import; *_pos = (int) (p - line); res = true;
	    }
	}
    }
    return res;
}

/* Identify an `#include` preprocessor statement in the given string `line`
** and return the included filename through the buffer `ifname` (with the
** maximum size of `ifnamesz`). The filename in the `#include` statement
** can be enclosed either in two `"`, or in `<` and `>`. Apart from the
** extraction of the filename, no further is performed, especially *no*
** syntax check. The return value is calculated as follows:
**   - If the line is an `#import` command, the fourth parameter is set to
**     `true`, otherwise to `false`,
**   - If a valid `#import` or `#include` statement was found, the filename
**     string of this command is written to the buffer given as second and
**     third parameters.
 * Thies output parameters are not modified if an error occurs or the line
 * is no `#import` or `#include` line.
 * The result value if `0` for a successful detection of an `#import` or
 * `#include` command, `1` if the examined line is no `#import` or `#include`
 * line and `-1` if an error occurred.
 * Possible errors generated by this function are:
 *   - `EINVAL` if an `#import` or `#include` line was found, but its string
 *     parameter is incomplete, or
 *   - `ENAMETOOLONG` if the string parameter didn't fit into the buffer.
*/
static int chk_include_stmt (const char *line,
			     char *ifname, size_t ifnamesz,
			     bool *_is_import)
{
    int pos, res = 1;
    bool is_import = false;
    if (is_include_cmd (line, &is_import, &pos)) {
	const char *p = line + pos, *q;
	int cltag = (*p++ == '<' ? '>' : '"');
	q = p; while (*q && *q != cltag) { ++q; }
	if (*q != cltag) {
	    res = -1; errno = EINVAL;
	} else if ((size_t) (q - p) >= ifnamesz) {
	    res = -1; errno = ENAMETOOLONG;
	} else {
	    memcpy (ifname, p, (size_t) (q - p)); ifname[q - p] = '\0';
	    *_is_import = is_import; res = 0;
	}
    }
    return res;
}

/* Write an `#error` directive about an invalid `#include` line to the
** output file `out`.
*/
static void invalid_include (FILE *out, int lc, bool is_import, int ec,
			     const char *ifname)
{
    const char *impincl = (is_import ? "#import" : "#include");
    if (! ifname) { ifname = "..."; }
    if (ec == 0) {
	fmt_print (out, "#warning Deactivated `$1 \"$2\".\n", impincl, ifname);
    } else if (ec == EINVAL) {
	fmt_print (out, "#error Invalid `$1 \"$2\"` line.\n", impincl, ifname);
    } else if (ec == ENAMETOOLONG) {
	fmt_print (out, "#error The `$1` filename is too long.\n", impincl);
    } else if (ec == EEXIST) {
	fmt_print (out, "#error File \"$1\" must not `$2` itself.\n",
			ifname, impincl);
    } else if (ec == EPERM) {
	fmt_print (out, "#error `$1 \"$2\"` is forbidden here.\n",
			impincl, ifname);
    } else {
	fmt_print (out, "#error `$1 \"$2\" - $3\n",
			impincl, ifname, strerror (ec));
    }
}


/* Create an output file where all include-files given as arguments
** are inserted at the first occurrence of IMPORTTAG1 or IMPORTTAG2.
** Each line beginning with '#include "`file`"' or '#include <`file`>',
** where `file` is one of the files given as arguments is converted
** into a comment in the output file. Additionally, each occurrence
** of `tfname` in the template file is replaced with the name of the
** output file ...
*/
static int import_via_tag (lines_t cline,
			   size_t filesc, char *const *files,
			   const char *ofname, FILE *out)
{
    int errs = 0, lc = 0, isinc;
    bool need_skip = false, tag_processed = false, is_import = false;
    char ifname[LMAX + 1];
    while (cline) {
	++lc;
	/* Deactivate each '#include "file"' or '#include <file>'-line
	** where 'file' is found in the list of files to be (partially)
	** included. Additionally, replace a line containing
	** '#include "`ofname`"' or '#include <`ofname`>' with an
	** error line ...
	*/
	need_skip = false;
	isinc = chk_include_stmt (cline->text, ifname, sizeof(ifname),
				  &is_import);
	if (isinc == 0) {
	    const char *ifn = ifname;
	    int ec = 0;
	    need_skip = true;
	    if (filesc > 0 && find_file (ifname, filesc, files)) {
		; // DO NOTHING HERE!
	    } else if (streq (ifname, ofname)) {
		ec = EEXIST;
	    } else if (is_import) {
		ec = EPERM;
	    }
	    invalid_include (out, lc, is_import, ec, ifn);
	    fflush (out);
	} else if (isinc < 0) {
	    const char *ifn = ifname;
	    need_skip = true;
	    if (errno == EINVAL) { ifn = NULL; }
	    invalid_include (out, lc, is_import, errno, ifn);
	    fflush (out);
	} else if (is_import_tag (cline->text)) {
	    need_skip = true;
	    if (! tag_processed) {
		for (int ix = 0; ix < filesc; ++ix) {
		    const char *ifn = files[ix];
		    if (copy_header_parts (ifn, ofname, &lc, out)) { ++errs; }
		}
		tag_processed = true;
	    }
	    line_to (ofname, lc, out);
	} else {
	    need_skip = false;
	}
	cline = process_remaining (cline, need_skip, out);
    }
    return errs;
}


static int import_via_include (lines_t cline,
			       size_t filesc, char *const *files,
			       const char *ofname, FILE *out)
{
    bool is_import = false, need_skip = false;;
    char ifname[LMAX + 1];
    int lc = 0, errs = 0, isinc;
    while (cline) {
	++lc;
	isinc = chk_include_stmt (cline->text, ifname, sizeof(ifname),
				  &is_import);
	if (isinc == 0) {
	    /* A valid `#import` or `#include` statement was found. */
	    const char *ifn = NULL;
	    if (filesc > 0) {
		ifn = find_file (ifname, filesc, files);
	    } else {
		ifn = ifname;
	    }
	    if (ifn) {
		if (copy_header_parts (ifn, ofname, &lc, out)) { ++errs; }
		line_to (ofname, lc, out);
		need_skip = true;	// Skip the remaining parts of the line.
	    } else {
		/* Converting an `#import` statement into an equivalent
		** `#include` statement, because `#import` was never really
		** supported by the C/C++ standards.
		*/
		int pos = 0;
		(void) is_include_cmd (cline->text, &is_import, &pos);
		if (is_import) {
		    int blanks = pos - 7;
		    fputc ('#', out);
		    while (blanks-- > 0) { fputc (' ', out); }
		    fputs ("include", out);
		    fputs (cline->text + pos, out);
		} else {
		    fputs (cline->text, out);
		}
		/* The (first part of) the line is now processed. Any
		** remaining parts are processed with `process_remaining()`.
		*/
		cline = cline->next;
		need_skip = false;
	    }
	} else if (isinc < 0) {
	    /* An error occurred. In this case, an `#error` pre-processor
	    ** command should be inserted instead of the broken line.
	    */
	    invalid_include (out, lc, is_import, errno, ifname);
	    need_skip = true;	// Skip the remaining parts of the line.
	} else if (isinc > 0) {
	    /* No error, but a normal line of the template (no `#import` or
	    ** `#include` preprocessor statement. This should be handled
	    ** through `process_remaining()`, so `need_skip` must be set to
	    ** false, here.
	    */
	    need_skip = false;
	}
	/* Process the remaining parts of a line - either by writing
	** them out, or by simply skipping them.
	*/
	cline = process_remaining (cline, need_skip, out);
    }
    return errs;
}

/* Create the header file from the given template and the list of input
** header files.
*/
static int write_header_file (const char *tfname, const char *ofname,
			      int filesc, char *const *files, FILE *out)
{
    int lc = 0, errs = 0, impmode = 0;
    char numbuf[32];
    lines_t template = (lines_t) NULL, cline;
    if (read_template (tfname, &template)) {
	free_lines (&template); return -1;
    }
    lc = 0; cline = template;
    /* Searching for an import tag in the template. If such a tag is found,
    ** all other import tags are ignored and the `#include` lines are inserted
    ** verbosely into the output file.
    ** In the first step, the number of import tags within the template is
    ** counted.
    */
    while (cline) {
	if (is_import_tag (cline->text)) {
	    if (impmode > 0) {
		line2str (lc, numbuf, sizeof(numbuf));
		fmt_print (stderr,
		    "$1($2): Ignoring further occurrences of the import tag.\n",
		    tfname, numbuf);
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
	errs = import_via_tag (template, filesc, files, ofname, out);
    } else {
	/* Revert to the original mode of action ... */
	errs = import_via_include (template, filesc, files, ofname, out);
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
	fmt_print (stderr, "$1: ", prog);
	va_start (pfa, fmt); vfprintf (stderr, fmt, pfa); va_end (pfa);
	fputs ("\n", stderr);
	exit (64);
    }
    fmt_print (stdout,
	       "Usage: $1 [-v] [-c directory] [-o out-header] header-template"
	       " header-file...\n"
	       "       $1 [-h]\n"
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
	       "\n    The header files who are used to create the combined"
	       " header file." 
	       "\n",
	       prog);
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
	return ovlen <= optlen && streqn (ov, lopt, ovlen);
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
    if (*ov != '-' || strnen (ov + 1, opt, optlen)) { return NULL; }
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
    if (strnen (ov, "--", 2) || strnen (ov + 2, opt, optlen)) {
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

    prog = bn (*argv); if (! prog) { prog = PROG; }

    if (!(non_optv = malloc ((argc + 1) * sizeof(char *)))) {
	fmt_print (stderr, "$1: Failed to allocate memory\n", prog);
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
	fmt_print (stderr, "$1: WARNING! Header files determined by names"
			   " extracted from the\n"
			   "    template may sometimes not be found\n", prog);
    }

    if (dir && chdir (dir) != 0) {
	fmt_print (stderr, "$1: Changing into directory '$1' failed - $1\n",
			   prog, dir, ERRSTR);
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
	    fmt_print (stderr, "$1: $2 - $3\n", prog, outfile, ERRSTR);
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
