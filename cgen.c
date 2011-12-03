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
**    cgen [-v] compile[=<compiler-program>] <target> <compiler-args>
**    cgen [-v] link[=<linker-program>] <target> <linker-args>
**    cgen [-v] [-c <directory>] clean <clean-args>
**    cgen help
**
** Arguments/Options:
**
**    -c <directory>
**       change into <directory> before performing the clean-action.
**
**    -v
**       display the complete command to be executed, together with all of it's output;
**       otherwise, only a short message concerning the action, the <target> and the
**       action's success-status is displayed.
**
**    compile[=<compiler-program>]
**       Execute the program <compiler-program> (default: cc) with <compiler-args> as
**       it's arguments; if `-v´ is specified, display the complete command to be
**       executed and also this program's output (stdout/stderr); otherwise, display
**       only a text line `Compiling <target> ...´,  followed by either ` done´ or
**       ` failed´ - depending on the program's termination code. Any prefix of the
**       word `compile´ can be specified here, such as `c´ `co´, `comp´ and the like.
**
**    help
**       display a short synopsis of cgen's arguments and terminate; any prefix of the
**       word `help´ can be used.
**
**    link=<linker-program>
**       Execute the program <linker-program> (default: cc) with <linker-args> as it's
**       arguments; the option `-v´ has the same effect as described above, with the
**       exception that `Linking <target> ...´ is displayed. Again, instead of the word
**       `link´, each of it's prefixes can be used.
**
**    clean
**       Remove any files and (recursively) directories specified in <clean-args>; errors
**       during the removal process are ignored; if `-v´ is specified, display each file
**       (directory) to be removed before it's removal and display the success-status
**       thereafter; otherwise, only a shore text-line `Cleaning up in <cwd>´ is
**       (<cwd> is the current working directory) displayed either ` done´ or ` failed´
**       depending on whether all specified files/directories could be removed or not.
**
**    <target>
**       the name of the target to be displayed if `-v´ was not specified; any argument
**       of the form `%t´ in the <compiler-args> and <linker-args> will be replaced with
**       this (file-)name
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

#define MODE_CLEAN 1
#define MODE_COMPILE 2
#define MODE_LINK 4

#ifndef DEFAULT_COMPILER
# define DEFAULT_COMPILER "cc"
#endif

#ifndef DEFAULT_LINKER
# define DEFAULT_LINKER "cc"
#endif

/* Module-internal global variable holding the name of the program.
*/
static char *progname = NULL;

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

/* Return true if the first argument (string) is a prefix of the second one (or equals
** the second argument) and false otherwise.
*/
static
bool is_prefix (const char *s1, const char *s2)
{
    size_t l1 = strlen (s1), l2 = strlen (s2);
    return (l1 > 0 && l1 <= l2 && strncmp (s1, s2, l1) == 0);
}

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

/* Write the short (non-verbose) message corresponding the requested action to the
** specified output channel.
*/
static
void print_mode (FILE *out, int mode, const char *target)
{
    switch (mode) {
	case MODE_CLEAN:
	    fputs ("Cleaning up in", out); break;
	case MODE_COMPILE:
	    fputs ("Compiling", out); break;
	case MODE_LINK:
	    fputs ("Linking", out); break;
	default:
	    return;
    }
    fputc (' ', out); fputs (target, out); fputs (" ...", out);
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
int do_cleanup (FILE *out, bool verbose, int nfiles, char **files)
{
    int rc, errs = 0, ix = 0;
    char *workdir = NULL;
    if (!verbose) {
	if (!(workdir = mycwd ())) { return -1; }
	print_mode (out, MODE_CLEAN, workdir);
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

/* Determine the program to be executed by using either the first argument (probably the
** value specified on the command line) of the value of the corresponding environment
** variable or the (hard-coded) default (in this order).
*/
static
const char *get_command (const char *cmdarg, const char *envvar, const char *defcmd)
{
    char *envval;
    if (cmdarg) { return cmdarg; }
    if ((envval = getenv (envvar))) { return envval; }
    return defcmd;
}

/* Generate the command from the program (cmd), the value of the environment variable
** (envvar) and a list of arguments.
*/
char **gen_cmd (const char *cmd, const char *target,
		const char *envvar, int argc, char **argv)
{
    int cmdc = 0, optc = 0, ix, jx;
    char *envval, **optv = NULL, **cmdv = NULL;
    if ((envval = getenv (envvar))) {
	if (!(optv = shsplit (envval))) { return NULL; }
    }
    if (optv) {
	while (optv[optc]) { ++optc; }
    }
    cmdc = optc + argc + 1;
    if ((cmdv = (char **) malloc ((cmdc + 1) * sizeof(char *)))) {
	ix = 0; if (!(cmdv[ix++] = sdup (cmd))) { goto ERREXIT; }
	for (jx = 0; jx < optc; ++jx) { cmdv[ix++] = optv[jx]; optv[jx] = NULL; }
	for (jx = 0; jx < argc; ++jx) {
	    if (!strcmp (argv[jx], "%t")) {
		if (!(cmdv[ix++] = sdup (target))) { goto ERREXIT; }
	    } else {
		if (!(cmdv[ix++] = sdup (argv[jx]))) { goto ERREXIT; }
	    }
	}
	cmdv[ix] = NULL;
    }
    if (optv) { free (optv); }
    return cmdv;
ERREXIT:
    if (optv) {
	for (jx = 0; jx < optc; ++jx) {
	    if (optv[jx]) { free (optv[jx]); optv[jx] = NULL; }
	}
	free (optv); optv = NULL;
    }
    for (jx = 0; jx < ix; ++jx) { free (cmdv[jx]); }
    free (cmdv);
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

/* Return the name of an environment variable (depending on the requested action)
** either for the program to be executed or one of the two possible option-vars
*/
static
const char *mode_envvar (int mode, int opts)
{
    switch (mode) {
	case MODE_COMPILE:
	    return (opts == 0 ? "COMPILER" : (opts == 1 ? "CFLAGS" : "COMPILER"));
	case MODE_LINK:
	    return (opts == 0 ? "LINKER" : (opts == 1 ? "LFLAGS" : "LOPTS"));
	default:
	    return NULL;
    }
}

/* Return the default program to be executed depending on the requested action
** (operation-mode).
*/
static
const char *mode_cmddefault (int mode)
{
    switch (mode) {
	case MODE_COMPILE: return DEFAULT_COMPILER;
	case MODE_LINK:    return DEFAULT_LINKER;
	default:           return NULL;
    }
}

/* Perform the requested action (`compile´ or `link´) by executing the corresponding
** command in a sub-process. Display the output depending on the `verbose´ argument.
*/
int spawn (FILE *out, bool verbose,
	   int mode, const char *prog,
	   const char *target, int argc, char **argv)
{
    extern char **environ;
    char **cmdv, *envval;
    const char *cmd, *env_cmd, *env_opts;
    pid_t child;
    int out_fd, waitstat, excode;
    env_cmd = mode_envvar (mode, 0);
    env_opts = mode_envvar (mode, 2);
    if (!(envval = getenv (env_opts)) || *envval == '\0') {
	env_opts = mode_envvar (mode, 1);
    }
    cmd = get_command (prog, env_cmd, mode_cmddefault (mode));
    if (!(cmdv = gen_cmd (cmd, target, env_opts, argc, argv))) { return -1; }
    if (!(cmd = which (cmdv[0]))) { return -1; }
    if (verbose) { print_command (out, cmdv); } else { print_mode (out, mode, target); }
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

/* Display either the usage message and terminate (exit-code = 0)
** or an error message concerning the usage and abort the program (exit-code = 64).
*/
static
void usage (const char *fmt, ...)
{
    if (fmt) {
	va_list av;
	fprintf (stderr, "%s: ", progname);
	va_start (av, fmt); vfprintf (stderr, fmt, av); va_end (av);
	fputs ("\n", stderr);
	exit (64);
    }
    printf ("\nUsage: %s [-v] compile[=<compiler-program>] <target> <compiler-args>"
	    "\n       %s [-v] link[=<linker-program>] <target> <linker-args>"
	    "\n       %s [-v] [-c <directory>] clean <clean-args>"
	    "\n       %s help"
	    "\n"
	    "\nArguments/Options:"
	    "\n"
	    "\n  -c <directory>"
	    "\n    change into <directory> before performing the clean-action."
	    "\n"
	    "\n  -v"
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
}


/* Main program
**
*/
int main (int argc, char *argv[])
{
    bool verbose = false;
    int mode, ix, rc;
    char *newdir = NULL, *p, *prog, *target;

    if ((progname = strrchr (argv[0], '/'))) { ++progname; } else { progname = argv[0]; }

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
    if (ix >= argc) {
	usage ("missing argument(s); see `%s help´ for more", progname);
    }
    if (is_prefix (argv[ix], "help")) { usage (NULL); }
    if (newdir && chdir (newdir)) {
	fprintf (stderr, "%s: chdir() - %s\n", progname, strerror (errno)); exit (1);
    }
    if (is_prefix (argv[ix], "clean")) {
	mode = MODE_CLEAN;
	if (++ix >= argc) {
	    usage ("missing argument(s); see `%s help´ for more", progname);
	}
	rc = do_cleanup (stdout, verbose, argc - ix, &argv[ix]);
	return 0;
    }
    if (ix + 3 >= argc) {
	usage ("missing argument(s); see `%s help´ for more", progname);
    }
    p = argv[ix++];
    if ((prog = strchr (p, '='))) {
	*prog++ = '\0'; if (!*prog) { prog = NULL; }
    }
    if (is_prefix (p, "compile") || !strcmp (p, "cc")) {
	mode = MODE_COMPILE;
    } else if (is_prefix (p, "link") || !strcmp (p, "ld")) {
	mode = MODE_LINK;
    } else {
	usage ("invalid action `%s´", p);
    }
    target = argv[ix++];
    rc = spawn (stdout, verbose, mode, prog, target, argc - ix, &argv[ix]);
    return (rc ? 1 : 0);
}
