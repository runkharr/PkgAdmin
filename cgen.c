/* cgen.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2011, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Code-Generation (Compiler/Linker) helper program.
**
** Synopsis:
**
**    cgen clean [-s|-v] [-C <directory>] <clean-args>
**    cgen compile[=<compiler-program>] [-c <rcfile>] [-v] [-s] <target> <compiler-args>
**    cgen help [<topic>]
**    cgen link[=<linker-program>] [-c <rcfile>] [-v] [-s] <target> <linker-args>
**
** Arguments/Options:
**
**    -c <rcfile>
**       load compiler/linker and extra options from the configuration file <rcfile>
**
**    -C <directory> (alt: --cd, --chdir)
**       change into <directory> before performing the clean-action.
**
**    -s (alt: --split-prog)
**       Assume <compiler-program> or <linker-program> being a command line prefix
**       instead of a path-name and split it shell-alike.
**
**    -v
**       display the complete command to be executed, together with all of it's output;
**       otherwise, only a short message concerning the action, the <target> and the
**       action's success-status is displayed.
**
**    clean
**       Remove any files and (recursively) directories specified in <clean-args>; errors
**       during the removal process are ignored; if `-v´ is specified, display each file
**       (directory) to be removed before it's removal and display the success-status
**       thereafter; otherwise, only a shore text-line `Cleaning up in <cwd>´ is
**       (<cwd> is the current working directory) displayed either ` done´ or ` failed´
**       depending on whether all specified files/directories could be removed or not.
**
**    compile[=<compiler-program>]
**       Execute the program <compiler-program> (default: cc) with <compiler-args> as
**       it's arguments; if `-v´ is specified, display the complete command to be
**       executed and also this program's output (stdout/stderr); otherwise, display
**       only a text line `Compiling <target> ...´,  followed by either ` done´ or
**       ` failed´ - depending on the program's termination code. Any prefix of the
**       word `compile´ can be specified here, such as `c´ `co´, `comp´ and the like.
**
**    link=<linker-program>
**       Execute the program <linker-program> (default: cc) with <linker-args> as it's
**       arguments; the option `-v´ has the same effect as described above, with the
**       exception that `Linking <target> ...´ is displayed. Again, instead of the word
**       `link´, each of it's prefixes can be used.
**
**    help
**       display a short synopsis of cgen's arguments and terminate; any prefix of the
**       word `help´ can be used.
**
**    <target>
**       the name of the target to be displayed if `-v´ was not specified; any argument
**       of the form `%t´ in the <compiler-args> and <linker-args> will be replaced with
**       this (file-)name
**
**    <topic>
**       One of `clean´, `compile´, `help´ or `link`
**
**    <compiler-args>
**       The arguments which (together with the name of the compiler program) form the
**       command to be executed
**
**    <linker-args>
**       The arguments which (together with the name of the linker program) form the
**       command to be executed
**
**    <clean-args>
**       the names of files and directories to be removed
**
** Environment variables:
**    `cgen´ recognizes the the environment variables COMPILER and LINKER and uses the
**    stored in these variables instead of the defaults for the `compile´- and `link´-
**    actions. Additionally the variables COPTS (or CFLAGS) and LOPTS (LFLAGS) are
**    recognized; the corresponding value is (shell-splitted) inserted before
**    <compiler-args> (respectively <linker-args>) ...
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define OPT_CLEAN 1
#define OPT_COMPILE 2
#define OPT_HELP 3
#define OPT_LINK 4

#ifndef DEFAULT_COMPILER
# define DEFAULT_COMPILER "cc"
#endif

#ifndef DEFAULT_LINKER
# define DEFAULT_LINKER "cc"
#endif

typedef struct action_s action_t;

static int do_clean (action_t *act, const char *prog, int argc, char **argv);
static int do_generate (action_t *act, const char *prog, int argc, char **argv);
static int do_help (action_t *act, const char *prog, int argc, char **argv);

typedef struct {
    const char *acname;
    char *prog, *popts;
} pdesc_t;

typedef pdesc_t *cdesc_t;

typedef int (*actionproc_t) (action_t *act, const char *prog, int argc, char **argv);

struct action_s {
    const char *pfx_name, *eq_name;
    actionproc_t proc;
    int min_args, max_args;
    bool display_topics;
    const char *env_cmd, *default_cmd, *env_opts, *env_flags;
    const char *synopsis, *prog_desc, *prog_args, *short_msg, *prog_help;
} actions[] = {
    { "clean", NULL, do_clean, 1, 0, false, NULL, NULL, NULL, NULL,
      "%s%s [-s|-v] [-C <new-directory>] %s",
      "", "<clean-args>",
      "Cleaning up in %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  clean"
      "\n    Remove any files and (recursively) directories specified in <clean-args>;"
      "\n    errors during the removal process are ignored; if `-v´ is specified, display"
      "\n    each file (directory) to be removed before it's removal and display the"
      "\n    success-status thereafter; if `-s´ is specified, display nothing; otherwise,"
      "\n    display only a shore text-line `Cleaning up in <cwd>´ is (<cwd> is the"
      "\n    current working directory) displayed followed by either ` done´ or ` failed´"
      "\n    depending on whether all specified files and/or directories could be removed"
      "\n    or not."
      "\n"
      "\n  -C <new-directory> (alt, --cd, --chdir)"
      "\n    Chdir into <new-directory> before performing the removal."
      "\n"
      "\n  -s (alt: --silent)"
      "\n    Suppress the `Cleaning up in ...´ message"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    Display each file/directory to be removed (in a shell-alike manner) and it's"
      "\n    the success status thereafter (` done´ or `failed´)."
      "\n"
      "\n  <clean-args>"
      "\n    The list of files and directories to be removed."
    },
    { "compile", "cc", do_generate, 0, 0, false,
      "COMPILER", DEFAULT_COMPILER, "COPTS", "CFLAGS",
      "%s=[%s] [-c <rcfile>] [-v] [-s] <target> %s",
      "<compiler-program>", "<compiler-args>",
      "Compiling %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  compile[=<compiler-program>] (alt: cc[=<compiler-program>])"
      "\n    Execute the program <compiler-program> (default: cc) with <compiler-args> as"
      "\n    it's arguments; if `-v´ is specified, display the complete command to be"
      "\n    executed and also this program's output (stdout/stderr); otherwise, display"
      "\n    only a text line `Compiling <target> ...´,  followed by either ` done´ or"
      "\n    ` failed´ - depending on whether the programm succeeded or terminated"
      "\n    abnormally. Any prefix of the word `compile´ can be specified here, such as"
      "\n    `co´, `comp´ and the like."
      "\n"
      "\n  <target>"
      "\n    The name to be displayed in the short (non-verbose) message"
      "\n    `Compiling <target> ...´; additionally it can be inserted anywhere into"
      "\n    <compiler-args> (by using the place-holder `%t´ as argument"
      "\n"
      "\n  <compiler-args>"
      "\n    The additional arguments to be supplied to the command being executed."
      "\n    Any argument `%t´ is replaced by <target>"
      "\n"
      "\n  -c <rcfile>"
      "\n    load compiler and additional options from a configuration file"
      "\n"
      "\n  -s (alt: --split-prog)"
      "\n    Assume <compiler-command> being an incomplete command line (split it)"
      "\n    according to shell-rules"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    Display each command line generated before it's executed instead of the"
      "\n    short message `Compiling target ...´; also, allow the executed command"
      "\n    to display it's messages (stdout/stderr)."
    },
    { "help", NULL, do_help, 0, 1, true, NULL, NULL, NULL, NULL,
      "%s%s [%s]", "", "<topic>", "",
      "\n\nDisplay usage information on the different topics"
      "\n\nValid topics are:"
    },
    { "link", "ld", do_generate, 0, 0, false,
      "LINKER", DEFAULT_LINKER, "LOPTS", "LFLAGS",
      "%s=[%s] [-c <rcfile>] [-v] [-s] <target> %s",
      "<linker-program>", "<linker-args>",
      "Linking %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  link=<linker-program> (alt: ld[=<linker-program>])"
      "\n    Execute the program <linker-program> (default: cc) with <linker-args> as"
      "\n    it's arguments; if `-v´ is specified, display the complete command to be"
      "\n    executed and also this program's output (stdout/stderr); otherwise, display"
      "\n    only a text line `Linking <target> ...´, followed by either ` done´ or"
      "\n    ` failed´ - depending on whether the program succeeded or terminated"
      "\n    abnormally. Any prefix of the word `link´ can be specified here, such as"
      "\n    `l´, `li´ and the like."
      "\n"
      "\n  <target>"
      "\n    The name to be displayed in the short (non-verbose) message"
      "\n    `Linking <target> ...´; additionally it can be inserted anywhere into"
      "\n    <linker-args> (by using the place-holder `%t´ as argument"
      "\n"
      "\n  <linker-args>"
      "\n    The additional arguments to be supplied to the command being executed."
      "\n    Any argument `%t´ is replaced by <target>"
      "\n"
      "\n  -c <rcfile>"
      "\n    load linker and additional options from a configuration file"
      "\n"
      "\n  -s (alt: --split-prog)"
      "\n    Assume <linker-command> being an incomplete command line (split it)"
      "\n    according to shell-rules"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    Display each command line generated before it's executed instead of the"
      "\n    short message `Linking target ...´; also, allow the executed command"
      "\n    to display it's messages (stdout/stderr)."
    },
    { NULL, NULL, 0, 0, 0, false, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/* Module-internal global variable holding the name of the program.
*/
static char *progname = NULL;

/* Return true if the first argument (string) is a prefix of the second one (or equals
** the second argument) and false otherwise.
*/
static
bool is_prefix (const char *p, const char *s)
{
    int c;
    while ((c = *p++) == *s++ && c);
    return !c;
}

#if 0
static
bool is_lcprefix (const char *p, const char *s)
{
    int c;
    while ((c = tolower (*p++)) == tolower (*s++) && c);
    return !c;
}
#endif

/* Display either the usage message and terminate (exit-code = 0)
** or an error message concerning the usage and abort the program (exit-code = 64).
*/
static
void usage (const char *fmt, ...)
{
    int ix;
    action_t *act;
    if (fmt) {
	va_list av;
	if (!strcmp (fmt, "help")) {
	    char *acname;
	    va_start (av, fmt);
	    acname = va_arg (av, char *);
	    va_end (av);
	    if (!acname) { goto GENERAL_DESC; }
	    for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
		if (is_prefix (acname, act->pfx_name)) { break; }
		if (act->eq_name && !strcmp (acname, act->eq_name)) { break; }
	    }
	    if (!act->pfx_name) {
		fprintf (stderr, "%s: no topic for `%s´ available\n", progname, acname);
		exit (64);
	    }
	    printf ("Usage: %s ", progname);
	    printf (act->synopsis, act->pfx_name, act->prog_desc, act->prog_args);
	    printf ("%s", act->prog_help);
	    if (act->display_topics) {
		for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
		    printf ("\n    %s", act->pfx_name);
		    if (act->eq_name) {
			printf (" (alt: %s)", act->eq_name);
		    }
		}
	    }
	    puts ("\n");
	    exit (0);
	}
	fprintf (stderr, "%s: ", progname);
	va_start (av, fmt); vfprintf (stderr, fmt, av); va_end (av);
	fputs ("\n", stderr);
	exit (64);
    }
GENERAL_DESC:
    act = &actions[0];
    printf ("\nUsage: %s ", progname);
    printf (act->synopsis, act->pfx_name, act->prog_desc, act->prog_args);
    for (ix = 1; (act = &actions[ix])->pfx_name; ++ix) {
	printf ("\n       %s ", progname);
	printf (act->synopsis, act->pfx_name, act->prog_desc, act->prog_args);
    }
    printf ("\n\nFor further help issue `%s help <topic>´, where topic is one of\n(",
	    progname);
    for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
	if (ix > 0) { printf (", "); }
	printf ("%s", act->pfx_name);
    }
    printf (")\n\n");
    exit (0);
#if 0
    printf ("\nUsage: %s [-v] [-s] compile[=<compiler-program>] <target> <compiler-args>"
	    "\n       %s [-v] [-s] link[=<linker-program>] <target> <linker-args>"
	    "\n       %s [-v] [-c <directory>] clean <clean-args>"
	    "\n       %s help"
	    "\n"
	    "\nArguments/Options:"
	    "\n"
	    "\n  -c <directory> (alt: --chdir, --cd)"
	    "\n    change into <directory> before performing the clean-action."
	    "\n  -s (alt: --split-prog)"
	    "\n    split <compiler-program> or <linker-program> shell-alike instead of"
	    " using them as"
	    "\n    a single program name"
	    "\n"
	    "\n  -v (alt: --verbose)"
	    "\n    display the complete command to be executed, together with all of"
	    " it's output;"
	    "\n    otherwise, only a short message concerning the action, the <target>"
	    " and the"
	    "\n    action's success-status is displayed."
	    "\n"
	    "\n  compile[=<compiler-program>] (alt: cc[=<compiler-program>])"
	    "\n    Execute the program <compiler-program> (default: cc) with <compiler-"
	    "args> as"
	    "\n    it's arguments; if `-v´ is specified, display the complete command to"
	    " be"
	    "\n    executed and also this program's output (stdout/stderr); otherwise,"
	    " display"
	    "\n    only a text line `Compiling <target> ...´,  followed by either"
	    " ` done´ or"
	    "\n    ` failed´ - depending on the program's termination code. Any prefix"
	    " of the"
	    "\n    word `compile´ can be specified here, such as `c´ `co´, `comp´ and"
	    " the like."
	    "\n"
	    "\n  help"
	    "\n    display a short synopsis of cgen's arguments and terminate; any"
	    " prefix of the"
	    "\n    word `help´ can be used."
	    "\n"
	    "\n  link=<linker-program> (alt: ld[=<linker-program>])"
	    "\n    Execute the program <linker-program> (default: cc) with <linker-args>"
	    " as it's"
	    "\n    arguments; the option `-v´ has the same effect as described above,"
	    " with the"
	    "\n    exception that `Linking <target> ...´ is displayed. Again, instead of"
	    " the word"
	    "\n    `link´, each of it's prefixes can be used."
	    "\n"
	    "\n  clean"
	    "\n    Remove any files and (recursively) directories specified in <clean-"
	    "args>; errors"
	    "\n    during the removal process are ignored; if `-v´ is specified, display"
	    " each file"
	    "\n    (directory) to be removed before it's removal and display the success"
	    "-status"
	    "\n    thereafter; otherwise, only a shore text-line `Cleaning up in <cwd>´"
	    " is"
	    "\n    (<cwd> is the current working directory) displayed either ` done´ or"
	    " ` failed´"
	    "\n    depending on whether all specified files/directories could be removed"
	    " or not."
	    "\n"
	    "\n  <target>"
	    "\n    the name of the target to be displayed if `-v´ was not specified; any"
	    " argument"
	    "\n    of the form `%%t´ in the <compiler-args> and <linker-args> will be"
	    " replaced with"
	    "\n    this (file-)name"
	    "\n"
	    "\n  <compiler-args>"
	    "\n    The arguments which (together with the name of the compiler program)"
	    " form the"
	    "\n    command to be executed"
	    "\n"
	    "\n  <linker-args>"
	    "\n    The arguments which (together with the name of the linker program)"
	    " form the"
	    "\n    command to be executed"
	    "\n"
	    "\n  <clean-args>"
	    "\n    the names of files and directories to be removed"
	    "\n"
	    "\nEnvironment variables:"
	    "\n  `%s´ recognizes the the environment variables COMPILER and LINKER and"
	    " uses the"
	    "\n  stored in these variables instead of the defaults for the `compile´-"
	    " and `link´-"
	    "\n  actions. Additionally the variables COPTS (or CFLAGS) and LOPTS"
	    " (LFLAGS) are"
	    "\n  recognized; the corresponding value is (shell-splitted) inserted before"
	    "\n  <compiler-args> (respectively <linker-args>) ...\n\n",
	    progname, progname, progname, progname, progname);
    exit (0);
#endif
}

/* Duplicate a string
*/
static
char *sdup (const char *s)
{
    char *res = malloc (strlen (s) + 1);
    if (res) { strcpy (res, s); }
    return res;
}

#if 0
static
char *conc (const char *s, ...)
{
    const char *el;
    char *res = NULL, *p;
    size_t ell, resl;
    va_list rargs;
    if (s) {
	resl = strlen (s) + 1;
	va_start (rargs, s);
	while ((el = va_arg (rargs, const char *))) { resl += strlen (el) + 1; }
	va_end (rargs);
	if ((res = (char *) malloc (resl))) {
	    p = res; --p; el = s;
	    while ((*++p = *el++));
	    va_start (rargs, s);
	    while ((el = va_arg (rargs, const char *))) {
		while ((*++p = *el++));
	    }
	    va_end (rargs);
	}
    }
    return res;
}
#endif

#if 0
/* duplicate a string but catch the error, write the corresponding message to stderr and
** return the (copy of) the string.
*/
static
char *x_sdup (const char *s)
{
    char *res = sdup (s);
    if (!res) { fprintf (stderr, "%s: %s\n", progname, strerror (errno)); exit (1); }
    return res;
}
#endif

/* Return true if the argument is either a blank or a TAB-character and false otherwise.
*/
static
__inline__ bool isws (char c)
{
    return (c == ' ' || c == '\t');
}

/* return true if the argument is neither '\0' nor a blank nor a TAB-character and false
** otherwise ...
*/
static
__inline__ bool nows (char c)
{
    return (c != '\0' && c != ' ' && c != '\t');
}

/* Split a string into a vector of strings using shell-alike word-detection.
*/
static
int shsplit (const char *s, char ***_out, int *_outlen)
{
    const char *p;
    char *buf, *r, **rv, quote = 0, c;
    int wc = 0, ix;
    bool word_open = false;

    if (!(buf = malloc (strlen (s) + 1))) { return -1; }
    r = buf; p = s;
    while ((c = *p++)) {
	if (isws (c)) {
	    if (quote) {
		*r++ = c;
	    } else if (word_open) {
		*r++ = '\0'; ++wc; word_open = false;
	    }
	} else if (c == '\\') {
	    word_open = true;
	    if (quote == '\'') {
		*r++ = c;
	    } else {
		*r = (*p ? *p++ : c);
	    }
	} else if (c == '\'' || c == '"') {
	    if (!quote) {
		word_open = true;
	    } else if (c == quote) {
		*r++ = '\0'; ++wc; word_open = false;
	    } else {
		*r++ = c;
	    }
	} else {
	    *r++ = c; word_open = true;
	}
    }
    if (word_open) { *r++ = '\0'; ++wc; word_open = false; }
    if ((rv = malloc ((wc + 1) * sizeof(char *)))) {
	ix = 0; r = buf;
	while (wc-- > 0) {
	    if (!(rv[ix++] = sdup (r))) {
		int jx; for (jx = 0; jx < ix; ++jx) { free (rv[jx]); }
		free (rv); rv = NULL; break;
	    }
	    r += strlen (r) + 1;
	}
	if (rv) { rv[ix] = NULL; }
    }
    free (buf);
    if (rv) { *_out = rv; *_outlen = ix; return 0; }
    return -1;
}

static
void _argv_free (char ***_argv)
{
    int ix;
    char **argv = *_argv;
    for (ix = 0; argv[ix]; ++ix) {
	/*memset (argv[ix], 0, strlen (argv[ix]));*/
	argv[ix] = NULL;
    }
    free (argv); argv = NULL;
    *_argv = argv;
}

#define argv_free(argv) (_argv_free (&(argv)))

#if 0
static
char **path_split (const char *path)
{
    int ix;
    char **res = NULL;
    size_t vecsz = 1, ressz = 0;

    const char *p, *q;
    char *r;

    p = path;
    if (*p == '/') { ++vecsz; ++p; }
    while ((q = strchr (p, '/'))) {
	if (q > p) {
	    ++vecsz; ressz += (size_t) (q - p) + 1;
	}
	p = q + 1;
    }
    if (*p) { ++vecsz; ressz += strlen (p); }

    ressz += (vecsz) * sizeof(char *);
    if (!(res = (char **) malloc (ressz))) { return res; }

    ix = 0; r = (char *) res + (vecsz) * sizeof(char *);
    p = path; if (*p == '/') { res[ix++] = r; *r++ = '\0'; ++p; }
    while ((q = strchr (p, '/'))) {
	if (q > p) {
	    res[ix++] = r; memcpy (r, p, (size_t) (q - p)); r[q - p] = '\0';
	    r += (q - p) + 1;
	}
	p = q + 1;
    }
    if (*p) { res[ix++] = r; strcpy (r, p); }
    res[ix] = NULL;
    return res;
}
#endif

#if 1
static
int lccmp (const char *l, const char *r)
{
    int lc, rc;
    while ((lc = tolower (*l++)) == (rc = tolower (*r++)) && lc && rc);
    return lc - rc;
#if 0
    for (;;) {
	lc = tolower (*l++); rc = tolower (*r++);
	if (lc != rc) { return lc - rc; }
    }
    return 0;
#endif
}
#endif

typedef struct { const char *acname, *cfname; int isopt, len; } rcdef_t;

static
rcdef_t rcdefs[] = {
    { "compile", "compiler", 0, 8 }, { "compile", "cc", 0, 2 },
    { "link", "linker", 0, 6 }, { "link", "ld", 0, 2 },
    { "compile", "compiler_options", 1, 16 }, { "compile", "ccopts", 1, 6 },
    { "compile", "copts", 1, 5 },
    { "link", "linker_options", 1, 14 }, { "link", "ldopts", 1, 6 },
    { "link", "lopts", 1, 5 },
    { NULL, NULL, -1, 0 }
};

static
int cuteol (char *buf)
{
    char *p = &buf[strlen (buf)];
    if (p > buf) {
	if (*--p == '\r') { *p = '\0'; return 1; }
	if (*p == '\n') {
	    *p = '\0'; if (p > buf && *--p == '\r') { *p = '\0'; return 3; }
	    return 2;
	}
    }
    return 0;
}

static
int get_ident (char *p, char *q, char **_r)
{
    if (!isalpha (*p) && *p != '_') { return -1; }
    while (p < q && (isalnum (*++p) || *p == '_'));
    if (p < q && nows (*p)) { return -1; }
    *p = '\0'; *_r = ++q;
    return 0;
}

/* Parse a configuration file (returning the names and options for a compiler/linker via
** the last argument). A return value of -1 indicates an opening failure, 0 a success and
** a positive value the number of errors found during the parsing process ...
*/
static
int read_cgenrc (const char *cgenrc, cdesc_t *_out, int *_outlen)
{
    int lc = 0, errc = 0, ix, ndesc = 0;
    FILE *fp;
    rcdef_t *rcdef;
    cdesc_t out = NULL, o1 = NULL;
    char buf[1024], rem[1024], *p, *q, *v;
    if (!(fp = fopen (cgenrc, "r"))) { return -1; }
    while (fgets (buf, sizeof(buf), fp)) {
	++lc;
	if (!cuteol (buf)) {
	    while (fgets (rem, sizeof(rem), fp) && !cuteol (rem));
	}
	p = buf; while (isws (*p)) { ++p; }
	if (!*p || *p == '#') { continue; }
	if (*p == '=') {
	    ++errc; fprintf (stderr, "%s(line %d): expecting a cfname\n", cgenrc, lc);
	    continue;
	}
	if (!(q = strchr (p, '='))) {
	    ++errc; fprintf (stderr, "%s(line %d): expecting `=´\n", cgenrc, lc);
	    continue;
	}
	if (get_ident (p, q, &q)) {
	    ++errc; fprintf (stderr, "%s(line %d): invalid identifier\n", cgenrc, lc);
	    continue;
	}

	/* Try to find the identifier in the list of valid cfnames ... */
	for (ix = 0; (rcdef = &rcdefs[ix])->cfname; ++ix) {
	    if (!lccmp (rcdef->cfname, p)) { break; }
	}

	/* It is an error if the identifier is not in this list ... */
	if (!rcdef->cfname) {
	    ++errc; fprintf (stderr, "%s(line %d): invalid variable\n", cgenrc, lc);
	    continue;
	}

	/* Set p to point behind the position of the `=´ ... */
	p = q;

	/* Search for `acname´ in the list of the already loaded values ... */
	for (ix = 0; ix < ndesc; ++ix) {
	    if (!strcmp (rcdef->acname, out[ix].acname)) { break; }
	}

	/* If the `acname´-configuration was not already loaded, then generate a
	** new one ...
	*/
	if (ix >= ndesc) {
	    if (!(o1 = (cdesc_t) realloc (out, ++ndesc * sizeof(*out)))) {
		fprintf (stderr, "%s: %s\n", progname, strerror (errno)); exit (1);
	    }
	    out = o1;
	    out[ix].acname = rcdef->acname;
	}

	/* It is an error if this configuration was already loaded ... */
	if ((rcdef->isopt && out[ix].popts) || (!rcdef->isopt && out[ix].prog)) {
	    ++errc;
	    fprintf (stderr, "%s(line %d): ambiguous `%s´\n", cgenrc, lc, rcdef->cfname);
	    continue;
	}

	/* Extract the configuration value ... */
	while (isws (*p)) { ++p; }
	q = p + strlen (p); while (q > p && isws (*--q));
	++q; if (isws (*q)) { *q = '\0'; }
	if (!(v = sdup (p))) {
	    fprintf (stderr, "%s: %s\n", progname, strerror (errno)); exit (1);
	}

	/* Insert the configuration value ... */
	if (rcdef->isopt) { out[ix].popts = v; } else { out[ix].prog = v; }
    }
    fclose (fp);

    /* If the configuration file was correct, the configuration list (and it's length)
    ** are returned (through the last two parameters); otherwise, release all items
    ** allocated to this point ...
    */
    if (errc == 0) {
	*_out = out; *_outlen = ndesc;
    } else {
	for (ix = 0; ix < ndesc; ++ix) {
	    out[ix].acname = NULL;
	    free (out[ix].prog); out[ix].prog = NULL;
	    free (out[ix].popts); out[ix].popts = NULL;
	}
	free (out);
    }
    return errc;
}

/* Print a command in shell-format to the specified output channel.
*/
static
void print_command (FILE *out, char **cmd)
{
    const char *w, *p;
    int ix;

    for (ix = 0; (w = cmd[ix]); ++ix) {
	if (ix > 0) { fputc (' ', out); }
	if (!*w) {
	    fputs ("''", out);
	} else if (!strpbrk (w, " \t'\"\\`$")) {
	    /* Keiner der üblichen Verdächtigen ==> Wort kann direkt ausgegeben werden */
	    fputs (w, out);
	} else if (!strpbrk (w, "\"\\`$")) {
	    /* Kein ", \, ` oder $ ==> Wort kann in " eingeschlossen ausgegeben werden */
	    fputc ('"', out); fputs (w, out); fputc ('"', out);
	} else {
	    /* ", \, ` oder $ gefunden ==> Wort muß in ' eingeschlossen werden */
	    for (p = w; *p; ++p) {
		if (*p == '\'') {
		    if (p != w) { fputc ('\'', out); }
		    fputs ("\"'\"", out);
		    if (p[1] != '\0') { fputc ('\'', out); }
		} else {
		    if (p == w) { fputc ('\'', out); }
		    fputc (*p, out);
		    if (p[1] == '\0') { fputc ('\'', out); }
		}
	    }
	}
    }
    fputs ("\n", out);
}

/* Write either " failed\n" or " done\n" (depending on the value of `exitstate´) to the
** specified output channel.
*/ 
static
void print_exitstate (FILE *out, int exitstate)
{
    fputs ((exitstate ? " failed\n" : " done\n"), out);
}

/* Remove a single file.
*/
static
int rmfile (const char *path, FILE *out)
{
    int rc;
    if (out) { fputs ("rm ", out); fputs (path, out); fputs (" ...", out); }
    rc = unlink (path);
    if (out) { fputc (' ', out); fputs ((rc ? "failed\n" : "done\n"), out); }
    return rc;
}

/* Helper structure for collection sub-directories during the recurvive removal of a
** directory.
*/
struct _dlist;
typedef struct _dlist dlist_s, *dlist_t;
struct _dlist {
    dlist_t next;
    char path[1];
};

/* Remove a directory - including (recursively) all of it' entries.
*/
static
int rmrec (const char *path, FILE *out)
{
    int rc;
    DIR *dp;
    struct dirent de, *dep;
    size_t bsz = 0, pl; char *buf = NULL, *p;
    dlist_t subdirs = NULL, ne;
    struct stat st;

    if (!(dp = opendir (path))) { return -1; }
    while ((rc = readdir_r (dp, &de, &dep)) == 0 && dep != NULL) {
	if (*de.d_name == '.'
	&&  (de.d_name[1] == '\0' || (de.d_name[1] == '.' && de.d_name[2] == '\0'))) {
	    continue;
	}
	pl = strlen (path) + strlen (de.d_name) + 2;
	if (pl > bsz) {
	    bsz = pl + 1023; bsz -= bsz % 1024;
	    if (!(p = realloc (buf, bsz))) { goto ERREXIT; }
	    buf = p; 
	}
	snprintf (buf, bsz, "%s/%s", path, de.d_name);
	if (lstat (buf, &st)) {
	    if (errno == ENOENT) { continue; }
	    goto ERREXIT;
	}
	if (S_ISDIR (st.st_mode)) {
	    if (!(ne = malloc (sizeof(dlist_s) + strlen (buf)))) { goto ERREXIT; }
	    ne->next = subdirs; subdirs = ne;
	    strcpy (ne->path, buf);
	} else if (rmfile (buf, out)) {
	    goto ERREXIT;
	}
    }
    closedir (dp);
    if (rc) { goto ERREXIT; }
    while (subdirs) {
	ne = subdirs; subdirs = ne->next; ne->next = NULL;
	if (rmrec (ne->path, out)) { goto ERREXIT; }
	free (ne); ne = NULL;
    }
    if (buf) { free (buf); }
    if (out) {
	fputs ("rmdir ", out); fputs (path, out); fputs (" ...", out);
    }
    rc = rmdir (path);
    if (out) { fputs ((rc ? " failed\n" : " done\n"), out); }
    return rc;
ERREXIT:
    while (subdirs) {
	ne = subdirs; subdirs = ne->next;
	ne->next = NULL; free (ne); ne = NULL;
    }
    if (buf) { free (buf); }
    return -1;
}

static
int rmfsentry (const char *path, FILE *out)
{
    struct stat st;
    if (lstat (path, &st)) { return (errno == ENOENT ? 0 : -1); }
    if (S_ISDIR (st.st_mode)) { return rmrec (path, out); }
    return rmfile (path, out);
}

/* Retrieve the current working diretory.
*/
static
char *mycwd()
{
    size_t bsz = 1024;
    char *buf = malloc (bsz), *b, *wd;
    if (!buf) { return buf; }
    while (!(wd = getcwd (buf, bsz)) && errno == ERANGE) {
	bsz += 1024;
	if (!(b = realloc (buf, bsz))) { free (buf); return b; }
	buf = b;
    }
    if (!wd) { return NULL; }
    wd = sdup (buf); free (buf);
    return wd;
}

/* Perform the `clean´-action (directly - not calling an external program for this
** reason).
*/
static
int cleanup (FILE *out, int verbose, int nfiles, char **files)
{
    int rc, errs = 0, ix = 0;
    action_t *act;
    char *workdir = NULL;
    if (verbose < 2) {
	if (!(workdir = mycwd ())) { return -1; }
	for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
	    if (!strcmp (act->pfx_name, "clean")) { break; }
	}
	if (act->pfx_name && verbose) {
	    fprintf (out, act->short_msg, workdir);
	}
	free (workdir); workdir = NULL;
	for (ix = 0; ix < nfiles; ++ix) {
	    rc = rmfsentry (files[ix], NULL);
	    if (rc) { ++errs; }
	}
	if (verbose) { print_exitstate (out, (errs ? 1 : 0)); }
    } else {
	for (ix = 0; ix < nfiles; ++ix) {
	    rc = rmfsentry (files[ix], out);
	    if (rc) { ++errs; }
	}
    }
    return (errs ? 1 : 0);
}

static
size_t sfx_subst (const char *w, const char *subst, size_t sl,
		  const char **_w, char **_out)
{
    size_t xl;
    const char *sx, *w1;
    if (*w != '[') { goto NOSFXSUBST; }
    w1 = strchr (w, ']');
    if (!w1) { goto NOSFXSUBST; }
    sx = w; while (sx < w1 && *sx != '/') { ++sx; }
    if (sx == w1) { goto NOSFXSUBST; }
#if 1
    xl = (size_t) (w1 - ++sx);
    if (sl <= xl || strncmp (subst + sl - xl, sx, xl)) {
	/* %t[Y/X] found bust subst is too short ... */
	w = ++w1; goto NOSFXSUBST;
    }
    sl -= xl; if (_out) { memcpy (*_out, subst, sl); *_out += sl; }
    ++w; xl = (size_t) (--sx - w);
    sl += xl; if (_out) { memcpy (*_out, w, xl); *_out += xl; }
#else
    ++w; xl = (size_t) (sx - w);
    if (sl <= xl || strncmp (subst + sl - xl, w, xl)) {
	/* %t[X/Y] found but subst is too short ... */
	w = ++w1; goto NOSFXSUBST;
    }
    sl -= xl; if (_out) { memcpy (*_out, subst, sl); *_out += sl; }
    ++sx; xl = (size_t) (w1 - sx); sl += xl;
    if (_out) { memcpy (*_out, sx, xl); *_out += xl; }
#endif
    *_w = ++w1;
    return sl;
NOSFXSUBST:
    if (_out) { memcpy (*_out, subst, sl); *_out += sl; }
    *_w = w;
    return sl;
}

/* Replace each occurrence of `pat´ (except when prefixed with `\´) in `where´
** with `subst´.
*/
static
char *rplc (const char *pat, const char *where, const char *subst)
{
    const char *w; size_t pl = strlen (pat);
    char *res, *r; size_t ressz = 1;
    size_t sl = strlen (subst);

    /* 1. Calculate the size of the result */
    w = where;
    while (*w) {
	if (*w == '\\' && !strncmp (&w[1], pat, pl)) {
	    ressz += pl; w += pl + 1;
	} else if (!strncmp (w, pat, pl)) {
	    w += pl; ressz += sfx_subst (w, subst, sl, &w, NULL);
	} else {
	    ++ressz; ++w; 
	}
    }

    /* 2. Allocate enough memory for the result and fill it with `where´ (with each
    **    occurrence of `pat´ replaced with `subst´) ...
    */
    if ((res = malloc (ressz))) {
	w = where; r = res;
	while (*w) {
	    if (*w == '\\' && !strncmp (&w[1], pat, pl)) {
		memcpy (r, &w[1], pl); r += pl; w += pl + 1;
	    } else if (!strncmp (w, pat, pl)) {
		w += pl; sfx_subst (w, subst, sl, &w, &r);
	    } else {
		*r++ = *w++;
	    }
	}
	*r = '\0';
    }
    return res;
}

/* Generate the command from the program (cmd), the value of the environment variable
** (envvar) and a list of arguments.
*/
char **gen_cmd (const char *prog, const char *popts, bool split_prog, action_t *act,
		const char *target, int argc, char **argv)
{
    int ix, jx, optc, cmdc = 0, aviix;
    const char *sv, *envval;
    char **progv = NULL, **optv = NULL, **cmdv = NULL;

    /* If the compiler program was explicitely set ... */
    if (prog) {
	/* ... and the `-s´-option was one of the options to `compile´ or `link´ ... */
	if (split_prog) {
	    /* ... then split `prog´ into a vector of arguments (e.g. special calling
	    ** of gcc)
	    */
	    if (shsplit (prog, &progv, &optc)) { return NULL; }
	    /* Increase the number of arguments by the length of the `prog´-vector ... */
	    cmdc += optc;
	} else {
	    if (!(progv = (char **) malloc (2 * sizeof(char *)))) { return NULL; }
	    *progv = progv[1] = NULL; if (!(*progv = sdup (prog))) { goto ERREXIT; }
	    /* Increase the number of arguments by the length of the `prog´-vector ... */
	    ++cmdc;
	}
    } else {
	sv = prog;
	if (act->env_cmd) { prog = getenv (act->env_cmd); }
	if (!prog || !*prog) { prog = sv; }
	if (!prog || !*prog) { prog = act->default_cmd; }
	if (prog && !*prog) { prog = NULL; }
	if (!prog) {
	    usage ("no valid program specified; see `%s help´ for help, please!",
	    progname);
	}
	/* Increase the number of arguments by the length of the `prog´-vector ... */
	if (!(progv = (char **) malloc (2 * sizeof(char *)))) { return NULL; }
	*progv = progv[1] = NULL; if (!(*progv = sdup (prog))) { goto ERREXIT; }
	++cmdc;
    }
    if (!(envval = getenv (act->env_opts)) || !*envval) {
	envval = getenv (act->env_flags);
    }
    if (!envval || !*envval) { envval = popts; }
    if (envval && *envval) {
	if (shsplit (envval, &optv, &optc)) { goto ERREXIT; }
	/* Increase the number of arguments by the length of the options-vector ... */
	cmdc += optc;
    }
    /* Increase the number of arguments by the number of the remaining arguments ... */
    cmdc += argc;

    /* Allocate enough memory for the command vector ... */
    if (!(cmdv = (char **) malloc ((cmdc + 1) * sizeof(char *)))) { goto ERREXIT; }
    memset (cmdv, 0, (cmdc + 1) * sizeof(char *));

    /* Now fill the command vector piece by piece ... */
    ix = 0;

    /* 1. the program vector (splitted? `prog´-argument ... */
    for (jx = 0; progv[jx]; ++jx) { cmdv[ix++] = progv[jx]; progv[jx] = NULL; }

    /* 2. the options retrieved either from the environment or from the
    ** configuration file, until either the end of this list or until an argument
    ** @ARGV was found ...
    */
    aviix = -1;
    if (optv) {
	for (jx = 0; optv[jx]; ++jx) {
	    if (!strcmp (optv[jx], "@ARGV")) { aviix = jx + 1; break; }
	    cmdv[ix++] = optv[jx]; optv[jx] = NULL;
	}
    }

    /* 3. the remaining command line arguments ... */
    for (jx = 0; jx < argc; ++jx) {
	if (!(cmdv[ix++] = sdup (argv[jx]))) { goto ERREXIT; }
    }

    /* 4. if there are remaining options (after the `@ARGV´ placeholder) ... */
    if (aviix > 0) {
	/* ... then these options ... */
	while (optv[aviix]) { cmdv[ix++] = optv[aviix++]; }
    }

    /* End of the command vector */
    cmdv[ix] = NULL;

    /* After the command vector was filled, each argument is modifier by substituting
    ** each `%t´ with `target´ ...
    */
    for (jx = 0; jx < ix; ++jx) {
	char *sv = rplc ("%t", cmdv[jx], target);
	if (!sv) { goto ERREXIT; }
	free (cmdv[jx]); cmdv[jx] = sv;
    }

    /* Remove the (no longer used) `progv´ and `optv´ vectors ... */
    if (progv) { argv_free (progv); progv = NULL; }
    if (optv) { argv_free (optv); optv = NULL; }

    /* Return the command vector ... */
    return cmdv;

ERREXIT:
    if (progv) { argv_free (progv); }
    if (optv) { argv_free (optv); }
    if (cmdv) { argv_free (cmdv); }
    return NULL;
}

/* Check if the argument points to a regular file which is executable for the calling
** user.
*/
static
bool is_xfile (const char *path)
{
    struct stat st;
    if (stat (path, &st)) { return false; }
    if (!S_ISREG (st.st_mode)) { return false; }
    return (access (path, X_OK) ? false : true);
}

/* Return either (a copy of) the argument if this argument is a (relative or absolute)
** pathname or try to find the argument in the PATH and return it's absolute pathname.
*/
static
char *which (const char *cmd)
{
    size_t bsz = 0, pl, cl;
    char *PATH, *p, *q, *buf = NULL, *b, *res = NULL;
    if (strchr (cmd, '/')) { return (is_xfile (cmd) ? sdup (cmd) : NULL); }
    if (!(PATH = getenv ("PATH"))) { return NULL; }
    cl = strlen (cmd);
    for (p = PATH; (q = strchr (p, ':')); p = q + 1) {
	if (p + 1 == q) { continue; }
	pl = (size_t) (q - p) + cl + 2;
	if (bsz < pl) {
	    bsz = pl + 1023; bsz -= bsz % 1024;
	    if (!(b = realloc (buf, bsz))) {
		if (buf) { free (buf); buf = NULL; return NULL; }
	    }
	    buf = b;
	}
	memcpy (buf, p, (size_t)(q - p)); buf[q - p] = '/';
	strcpy (buf + (q - p) + 1, cmd);
	if (is_xfile (buf)) { res = sdup (buf); break; }
    }
    if (!res && *p) {
	/* Remaining path element after the last `:´ ... */
	q = p + strlen (p);
	pl = (size_t) (q - p) + cl + 2;
	if (bsz < pl) {
	    bsz = pl + 1023; bsz -= bsz % 1024;
	    if (!(b = realloc (buf, bsz))) {
		if (buf) { free (buf); buf = NULL; }
	    } else {
		buf = b;
	    }
	}
	memcpy (buf, p, (size_t)(q - p)); buf[q - p] = '/';
	strcpy (buf + (q - p) + 1, cmd);
	if (is_xfile (buf)) { res = sdup (buf); }
    }
    free (buf);
    return res;
}

/* Perform the requested action (`compile´ or `link´) by executing the corresponding
** command in a sub-process. Display the output depending on the `verbose´ argument.
*/
int spawn (FILE *out, bool verbose, bool split_prog,
	   action_t *act, const char *prog, const char *popts,
	   const char *target, int argc, char **argv)
{
    extern char **environ;
    char **cmdv;
    const char *cmd;
    pid_t child;
    int out_fd, waitstat, excode;
    cmdv = gen_cmd (prog, popts, split_prog, act, target, argc, argv);
    if (!cmdv) { return -1; }
    if (!(cmd = which (cmdv[0]))) { return -1; }
    if (verbose) {
	print_command (out, cmdv);
    } else {
	fprintf (out, act->short_msg, target);
    }
    if ((out_fd = open ("/dev/null", O_WRONLY|O_APPEND)) < 0) { return -1; }
    fflush (stdout); fflush (stderr);
    switch (child = fork ()) {
	case -1: /* ERROR (fork failed) */
	    close (out_fd);
	    return -1;
	case 0:  /* CHILD */
	    if (!verbose) { dup2 (out_fd, 1); dup2 (out_fd, 2); }
	    close (out_fd);
	    execve (cmd, cmdv, environ);
	    exit (1);
	default: /* PARENT */
	    close (out_fd);
	    waitpid (child, &waitstat, 0);
	    excode = 0;
	    if (WIFEXITED (waitstat)) {
		excode = WEXITSTATUS (waitstat);
	    } else if (WIFSIGNALED (waitstat)) {
		excode = WTERMSIG (waitstat);
	    }
	    if (!verbose) { print_exitstate (out, excode); }
	    return excode;
    }
    return -1;
}

static void check_args (action_t *act, int argc)
{
    if (argc < act->min_args) {
	usage ("missing argument(s) for %s; see `%s help´ for more, please!",
	       act->pfx_name, progname);
    }
    if (act->max_args > act->min_args && argc - 1 > act->max_args) {
	usage ("too many arguments for %s; see `%s help´ for more, please!",
	       act->pfx_name, progname);
    }
}

static
int do_help (action_t *act, const char *prog, int argc, char **argv)
{
    char *topic = NULL;
    if (prog) { usage ("%s=<program> not allowed here", act->pfx_name); }
    check_args (act, argc);
    if (argc > 1) { topic = argv[1]; }
    usage ("help", topic);
    return 0;
}

static
int do_clean (action_t *act, const char *prog, int argc, char **argv)
{
    int ix;
    bool verbose = false, silent = false;
    char *newdir = NULL;

    if (prog) { usage ("%s=<program> not allowed here", act->pfx_name); }

    for (ix = 1; ix < argc; ++ix) {
	if (!strcmp (argv[ix], "-v") || !strcmp (argv[ix], "--verbose")) {
	    verbose = true; silent = false; continue;
	}
	if (!strcmp (argv[ix], "-s") || !strcmp (argv[ix], "--silent")) {
	    verbose = false; silent = true; continue;
	}
	if (!strncmp (argv[ix], "-C", 2)) {
	    if (newdir) { usage ("ambiguous option `-C´"); }
	    if (argv[ix][2]) {
		newdir = &argv[ix][2];
	    } else if (ix + 1 < argc) {
		newdir = argv[++ix];
	    } else {
		usage ("missing argument for option `%s´", argv[ix]);
	    }
	    continue;
	}
	if (!strncmp (argv[ix], "--cd=", 5)) {
	    if (newdir) { usage ("ambiguous option `--cd´"); }
	    if (!*(newdir = &argv[ix][5])) {
		usage ("invalid empty argument for option `--cd´");
	    }
	    continue;
	}
	if (!strncmp (argv[ix], "--chdir=", 8)) {
	    if (newdir) { usage ("ambiguous option `--chdir´"); }
	    if (!*(newdir = &argv[ix][8])) {
		usage ("invalid empty argument for option `--chdir´");
	    }
	    continue;
	}
	if (!strcmp (argv[ix], "--cd") || !strcmp (argv[ix], "--chdir")) {
	    if (newdir) { usage ("ambiguous option `%s´", argv[ix]); }
	    if (ix + 1 < argc) {
		usage ("missing argument for option `%s´", argv[ix]);
	    }
	    continue;
	}
	if (*argv[ix] != '-') { break; }
	usage ("invalid option `%s´", argv[ix]);
    }

    check_args (act, argc - ix);

    if (newdir && chdir (newdir)) {
	fprintf (stderr, "%s %s: failed to chdir into `%s´ - %s\n",
			 progname, act->pfx_name, newdir, strerror (errno));
	exit (1);
    }
    cleanup (stdout, (verbose ? 2 : (silent ? 0 : 1)), argc - ix, &argv[ix]);
    return 0;
}

static
int do_generate (action_t *act, const char *prog, int argc, char **argv)
{
    int optx, ix, rc, cdesclen = 0, ac;
    bool verbose = false, split_prog = false;
    char *target = NULL, *cf = NULL, *opt, **av;
    const char *popts = NULL;
    cdesc_t cdesc = NULL;

    for (optx = 1; optx < argc; ++optx) {
	opt = argv[optx]; if (*opt != '-' || !strcmp (opt, "--")) { break; }
	if (is_prefix ("-c", opt)) {
	    if (cf) { usage ("ambiguous `-c´-option"); }
	    if (opt[2]) {
		cf = &opt[2];
	    } else {
		if (optx >= argc - 1) { usage ("missing argument for option `-c´"); }
		cf = argv[++optx];
	    }
	    continue;
	}
	if (!strcmp (opt, "-s") || !strcmp (opt, "--split-prog")) {
	    split_prog = true; continue;
	}
	if (!strcmp (opt, "-v") || !strcmp (opt, "--verbose")) {
	    verbose = true; continue;
	}

	usage ("invalid option `%s´", opt);
    }

    rc = read_cgenrc (cf, &cdesc, &cdesclen);
    if (rc > 0) {
	fprintf (stderr, "%s: errors in configuration file\n", progname); exit (1);
    }
    if (cdesc) {
	for (ix = 0; ix < cdesclen; ++ix) {
	    if (!strcmp (act->pfx_name, cdesc[ix].acname)) {
		popts = cdesc[ix].popts; if (!prog) { prog = cdesc[ix].prog; }
		break;
	    }
	}
    }

    check_args (act, argc - optx);
    target = argv[optx++];
    ac = argc - optx; av = &argv[optx];
    rc = spawn (stdout, verbose, split_prog, act, prog, popts, target, ac, av);
    return (rc ? 1 : 0);
}

/* Main program
**
*/
int main (int argc, char *argv[])
{
    int mode;
    char *p, *prog = NULL;
    action_t *act;

    if ((progname = strrchr (argv[0], '/'))) { ++progname; } else { progname = argv[0]; }

    if (argc - 1 < 1) {
	usage ("missing argument(s); see `%s help´ for more, please!", progname);
    }

    /* Argument: compile=<prog> | link=<prog> | clean | help */
    p = argv[1];
    if ((prog = strchr (p, '='))) {
	/* Get the program name */
	*prog++ = '\0'; if (!*prog) { prog = NULL; }
    }

    /* Select the action routine from the command-parameter `p´ ...
    */
    for (mode = 0; (act = &actions[mode], act->pfx_name); ++mode) {
	if (is_prefix (p, act->pfx_name)) { break; }
	if (act->eq_name && !strcmp (p, act->eq_name)) { break; }
    }
    if (!act->pfx_name) { usage ("invalid command `%s´", p); }

    return act->proc (act, prog, argc - 1, &argv[1]);
}
