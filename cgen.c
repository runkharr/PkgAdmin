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
**    cgen clean [-v] [-c <directory>] <clean-args>
**    cgen compile[=<compiler-program>] [-v] [-s] <target> <compiler-args>
**    cgen help [<topic>]
**    cgen link[=<linker-program>] [-v] [-s] <target> <linker-args>
**
** Arguments/Options:
**
**    -c <directory> (alt: --cd, --chdir)
**       change into <directory> before performing the clean-action.
**
**    -s (alt: --split-prog)
**       Assume <compiler-program> or <linker-program> being a command line prefix
**       instead of a path-name and split it shell-alike.

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
      "%s%s [-v] [-c <new-directory>] %s",
      "", "<clean-args>",
      "Cleaning up in %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  clean"
      "\n    Remove any files and (recursively) directories specified in <clean-args>;"
      "\n    errors during the removal process are ignored; if `-v´ is specified, display"
      "\n    each file (directory) to be removed before it's removal and display the"
      "\n    success-status thereafter; otherwise, only a shore text-line"
      "\n    `Cleaning up in <cwd>´ is (<cwd> is the current working directory) displayed"
      "\n    either ` done´ or ` failed´ depending on whether all specified files and/or"
      "\n    directories could be removed or not."
      "\n"
      "\n  -c <new-directory> (alt, --cd, --chdir)"
      "\n    Chdir into <new-directory> before performing the removal."
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    Display each file/directory to be removed (in a shell-alike manner) and it's"
      "\n    the success status thereafter (` done´ or `failed´)."
      "\n"
      "\n  <clean-args>"
      "\n    The list of files and directories to be removed."
    },
    { "compile", "cc", do_generate, 1, 0, false,
      "COMPILER", DEFAULT_COMPILER, "COPTS", "CFLAGS",
      "%s=%s [-v] [-s] <target> %s",
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
    { "link", "ld", do_generate, 1, 0, false,
      "LINKER", DEFAULT_LINKER, "LOPTS", "LFLAGS",
      "%s=%s [-v] [-s] <target> %s",
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
bool is_prefix (const char *s1, const char *s2)
{
    size_t l1 = strlen (s1), l2 = strlen (s2);
    return (l1 > 0 && l1 <= l2 && strncmp (s1, s2, l1) == 0);
}

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

/* Split a string into a vector of strings using shell-alike word-detection.
*/
static
char **shsplit (const char *s)
{
    const char *p;
    char *buf, *r, **rv, quote = 0, c;
    int wc = 0, ix;
    bool word_open = false;

    if (!(buf = malloc (strlen (s) + 1))) { return NULL; }
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
    return rv;
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
int cleanup (FILE *out, bool verbose, int nfiles, char **files)
{
    int rc, errs = 0, ix = 0;
    action_t *act;
    char *workdir = NULL;
    if (!verbose) {
	if (!(workdir = mycwd ())) { return -1; }
	for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
	    if (!strcmp (act->pfx_name, "clean")) { break; }
	}
	if (act->pfx_name) {
	    fprintf (out, act->short_msg, workdir);
	}
	free (workdir); workdir = NULL;
	for (ix = 0; ix < nfiles; ++ix) {
	    rc = rmfsentry (files[ix], NULL);
	    if (rc) { ++errs; }
	}
	print_exitstate (out, (errs ? 1 : 0));
    } else {
	for (ix = 0; ix < nfiles; ++ix) {
	    rc = rmfsentry (files[ix], out);
	    if (rc) { ++errs; }
	}
    }
    return (errs ? 1 : 0);
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
	    ressz += sl; w += pl;
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
		memcpy (r, subst, sl); r += sl; w += pl;
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
char **gen_cmd (const char *prog, bool split_prog, action_t *act,
		const char *target, int argc, char **argv)
{
    int ix, jx;
    char *envval;
    char **progv = NULL; int progc = 0;
    char **optv = NULL; int optc = 0;
    char **cmdv = NULL; int cmdc = 0;
    if (prog) {
	if (split_prog) {
	    if (!(progv = shsplit (prog))) { return NULL; }
	    progc = 0; while (progv[progc]) { ++progc; }
	} else {
	    progc = 1;
	    if (!(progv = (char **) malloc (2 * sizeof(char *)))) { return NULL; }
	    *progv = progv[1] = NULL; if (!(*progv = sdup (prog))) { goto ERREXIT; }
	}
    } else {
	if (act->env_cmd) { prog = getenv (act->env_cmd); }
	if (!prog || !*prog) { prog = act->default_cmd; }
	if (prog && !*prog) { prog = NULL; }
	if (!prog) {
	    usage ("no valid program specified; see `%s help´ for help, please!",
	    progname);
	}
	progc = 1;
	if (!(progv = (char **) malloc (2 * sizeof(char *)))) { return NULL; }
	*progv = progv[1] = NULL; if (!(*progv = sdup (prog))) { goto ERREXIT; }
    }
    if (!(envval = getenv (act->env_opts)) || !*envval) {
	envval = getenv (act->env_flags);
    }
    if (envval && *envval) {
	if (!(optv = shsplit (envval))) { goto ERREXIT; }
	optc = 0; while (optv[optc]) { ++optc; }
    }
    cmdc = optc + argc + 1;
    if (!(cmdv = (char **) malloc ((cmdc + 1) * sizeof(char *)))) { goto ERREXIT; }
    ix = 0;
    for (jx = 0; jx < progc; ++jx) { cmdv[ix++] = progv[jx]; progv[jx] = NULL; }
    for (jx = 0; jx < optc; ++jx) { cmdv[ix++] = optv[jx]; optv[jx] = NULL; }
    for (jx = 0; jx < argc; ++jx) {
	if (!(cmdv[ix++] = rplc ("%t", argv[jx], target))) { goto ERREXIT; }
    }
    cmdv[ix] = NULL;
    if (progv) { free (progv); progv = NULL; }
    if (optv) { free (optv); optv = NULL; }
    return cmdv;
ERREXIT:
    if (progv) {
	for (jx = 0; jx < progc; ++jx) {
	    if (progv[jx]) { free (progv[jx]); progv[jx] = NULL; }
	}
	free (progv); progv = NULL;
    }
    if (optv) {
	for (jx = 0; jx < optc; ++jx) {
	    if (optv[jx]) { free (optv[jx]); optv[jx] = NULL; }
	}
	free (optv); optv = NULL;
    }
    if (cmdv) {
	for (jx = 0; jx < ix; ++jx) { free (cmdv[jx]); cmdv[jx] = 0; }
	free (cmdv); cmdv = NULL;
    }
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
	   action_t *act, const char *prog,
	   const char *target, int argc, char **argv)
{
    extern char **environ;
    char **cmdv;
    const char *cmd;
    pid_t child;
    int out_fd, waitstat, excode;
    if (!(cmdv = gen_cmd (prog, split_prog, act, target, argc, argv))) { return -1; }
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
    if (argc - 1 < act->min_args) {
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
    bool verbose = false;
    char *newdir = NULL;

    if (prog) { usage ("%s=<program> not allowed here", act->pfx_name); }

    for (ix = 1; ix < argc; ++ix) {
	if (!strcmp (argv[ix], "-v") || !strcmp (argv[ix], "--verbose")) {
	    verbose = true; continue;
	}
	if (!strncmp (argv[ix], "-c", 2)) {
	    if (newdir) { usage ("ambiguous option `-c´"); }
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
    cleanup (stdout, verbose, argc - ix, &argv[ix]);
    return 0;
}

static
int do_generate (action_t *act, const char *prog, int argc, char **argv)
{
    int ix, rc;
    bool verbose = false, split_prog = false;
    char *target = NULL;

    for (ix = 1; ix < argc; ++ix) {
	if (!strcmp (argv[ix], "-v") || !strcmp (argv[ix], "--verbose")) {
	    verbose = true; continue;
	}
	if (!strcmp (argv[ix], "-s") || !strcmp (argv[ix], "--split-prog")) {
	    split_prog = true; continue;
	}
	if (*argv[ix] != '-') { break; }
	usage ("invalid option `%s´", argv[ix]);
    }

    check_args (act, argc - ix);
    target = argv[ix++];
    rc = spawn (stdout, verbose, split_prog, act, prog, target, argc - ix, &argv[ix]);
    return (rc ? 1 : 0);
}

/* Main program
**
*/
int main (int argc, char *argv[])
{
    int mode;
    char *p, *prog;
    action_t *act;

    if ((progname = strrchr (argv[0], '/'))) { ++progname; } else { progname = argv[0]; }

    if (argc < 2) {
	usage ("missing argument(s); see `%s help´ for more, please!", progname);
    }
    p = argv[1];
    if ((prog = strchr (p, '='))) {
	*prog++ = '\0'; if (!*prog) { prog = NULL; }
    }
    for (mode = 0; (act = &actions[mode], act->pfx_name); ++mode) {
	if (is_prefix (p, act->pfx_name)) { break; }
	if (act->eq_name && !strcmp (p, act->eq_name)) { break; }
    }
    if (!act->pfx_name) { usage ("invalid action `%s´", p); }

    return act->proc (act, prog, argc - 1, &argv[1]);
}
