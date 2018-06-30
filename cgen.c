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
**    cgen compile[=<compiler-program>] [-c <rcfile>] [-v] [-s] <target> \
**         <compiler-args>
**    cgen help [<topic>]
**    cgen link[=<linker-program>] [-c <rcfile>] [-v] [-s] <target> \
**         <linker-args>
**    cgen libgen[=<libgen-commands>] [-c <rcfile>] <target> <object-files>
**    cgen sogen[=<sogen-commands>] [-c <rcfile>] [<target>] <object-files>
**    cgen rogen[=<rogen-commands>] [-c <rcfile>] [<target>] <object-files>
**
** Arguments/Options:
**
**    -c <rcfile> (alt: -f <rcfile>)
**       load compiler/linker and extra options from the configuration file
**       <rcfile>
**
**    -C <directory> (alt: --cd, --chdir)
**       change into <directory> before performing the clean-action.
**
**    -s (alt: --split-prog)
**       Assume <compiler-program> or <linker-program> being a command line
**       prefix instead of a path-name and split it shell-alike.
**
**    -v
**       display the complete command to be executed, together with all of it's
**       output; otherwise, only a short message concerning the action, the
**       <target> and the action's success-status is displayed.
**
**    clean
**       Remove any files and (recursively) directories specified in
**       <clean-args>; errors during the removal process are ignored; if '-v'
**       is specified, display each file (directory) to be removed before it's
**       removal and display the success-status thereafter; otherwise, only a
**       shore text-line 'Cleaning up in <cwd>' is (<cwd> is the current
**       working directory) displayed either ' done' or ' failed' depending on
**       whether all specified files/directories could be removed or not.
**
**    compile[=<compiler-program>]
**       Execute the program <compiler-program> (default: cc) with
**       <compiler-args> as it's arguments; if '-v' is specified, display the
**       complete command to be executed and also this program's output
**       (stdout/stderr); otherwise, display only a text line
**       'Compiling <target> ...',  followed by either ' done' or ' failed'
**       - depending on the program's termination code. Any prefix of the word
**       'compile' can be specified here, such as 'c' 'co', 'comp' and the
**       like.
**
**    link=<linker-program>
**       Execute the program <linker-program> (default: cc) with <linker-args>
**       as it's arguments; the option '-v' has the same effect as described
**       above, with the exception that 'Linking <target> ...' is displayed.
**       Again, instead of the word 'link', each of it's prefixes can be used.
**
**    help
**       display a short synopsis of cgen's arguments and terminate; any prefix
**       of the word 'help' can be used.
**
**    libgen
**       generate a static library using 'ar' and 'ranlib' ...
**
**    sogen
**       generate a dynamic shared library using the specified linker program
**       (default: 'cc'). This command expects <linker-program> to understand
**       the option '-shared'.
**
**    rogen
**       generate a relocatable object file from a list of (relocatable) object
**       files - using <linker-program> (default: 'ld'); This command expects
**       <linker-program> to understand the option '-r'.
**
**    <target>
**       the name of the target to be displayed if '-v' was not specified; any
**       argument of the form '%t' in the <compiler-args> and <linker-args>
**       will be replaced with this (file-)name
**
**    <topic>
**       One of 'clean', 'compile', 'help' or 'link'
**
**    <compiler-args>
**       The arguments which (together with the name of the compiler program)
**       form the command to be executed
**
**    <linker-args>
**       The arguments which (together with the name of the linker program)
**       form the command to be executed
**
**    <clean-args>
**       the names of files and directories to be removed
**
** Environment variables:
**    'cgen' recognizes the the environment variables COMPILER and LINKER and
**    uses the values stored in these variables instead of the defaults for the
**    'compile'- and 'link'-actions. Additionally the variables COPTS (or
**    CFLAGS) and LOPTS (LFLAGS) are recognized; the corresponding value is
**    (shell-splitted) inserted before <compiler-args> (respectively
**    <linker-args>) ...
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
#include <pwd.h>
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

#ifndef DEFAULT_LIBGENCMD
# define DEFAULT_LIBGENCMD "ar rc %t @ARGV; ranlib %t"
# define DEFAULT_LIBGENCMDTEXT "'ar rc %%t @ARGV; ranlib %%t'"
#endif

#ifndef DEFAULT_SOGENCMD
# define DEFAULT_SOGENCMD "ld -shared @ARGV -o %t"
# define DEFAULT_SOGENCMDTEXT "'ld -shared @ARGV -o %%t'"
#endif

#ifndef DEFAULT_ROGENCMD
# define DEFAULT_ROGENCMD "ld -r @ARGV -o %t"
# define DEFAULT_ROGENCMDTEXT "'ld -r @ARGV -o %t'"
#endif

typedef struct action_s action_t;

static int do_clean (action_t *act, const char *prog, int argc, char **argv);
static int do_generate (action_t *act, const char *prog, int argc, char **argv);
static int do_help (action_t *act, const char *prog, int argc, char **argv);
static int do_libgen (action_t *act, const char *prog, int argc, char *argv[]);

typedef struct {
    const char *acname;
    char *prog, *popts;
} pdesc_t;

typedef pdesc_t *cdesc_t;

#define tmalloc(s, t) ((t *) malloc ((s) * sizeof(t)))
#define tsmalloc(t, s) ((t *) malloc (sizeof(t) + (s)))

typedef int (*actionproc_t) (action_t *act, const char *prog,
			     int argc, char **argv);

struct action_s {
    const char *pfx_name, *eq_name;
    actionproc_t proc;
    int min_args, max_args;
    bool display_topics;
    const char *env_cmd, *default_cmd, *default_opts, *env_opts, *env_flags;
    const char *synopsis, *prog_desc, *prog_args, *short_msg, *prog_help;
} actions[] = {
    { "clean", NULL, do_clean, 1, 0, false, NULL, NULL, NULL, NULL, NULL,
      "%s%s [-s|-v] [-C <new-directory>] %s",
      "", "<clean-args>",
      "Cleaning up in %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  clean"
      "\n    Remove any files and (recursively) directories specified in"
      " <clean-args>;"
      "\n    errors during the removal process are ignored; if '-v' is"
      " specified,"
      "\n    display each file (directory) to be removed before it's removal"
      " and display"
      "\n    the success-status thereafter; if '-s' is specified, display"
      " nothing;"
      "\n    otherwise, display only a shore text-line 'Cleaning up in <cwd>'"
      " is (<cwd>"
      "\n    is the current working directory) displayed followed by either"
      " ' done' or"
      "\n    ' failed', depending on whether all specified files and/or"
      " directories"
      "\n    could be removed or not."
      "\n"
      "\n  -C <new-directory> (alt, --cd, --chdir)"
      "\n    Chdir into <new-directory> before performing the removal."
      "\n"
      "\n  -s (alt: --silent)"
      "\n    Suppress the 'Cleaning up in ...' message"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    Display each file/directory to be removed (in a shell-alike"
      " manner) and"
      "\n    the success status thereafter (' done' or ' failed')."
      "\n"
      "\n  <clean-args>"
      "\n    The list of files and directories to be removed."
    },
    { "compile", "cc", do_generate, 0, 0, false,
      "COMPILER", DEFAULT_COMPILER, NULL, "COPTS", "CFLAGS",
      "%s[=%s] [-c <rcfile>] [-v] [-s] <target> \\\n           %s",
      "<compiler-program>", "<compiler-args>",
      "Compiling %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  compile (alt: cc)"
      "\n    Execute a compiler command constructed from a linker program,"
      " some"
      "\n    (default-)options and <compiler-args>."
      "\n"
      "\n  <compiler-program>"
      "\n    the name of the program used to translate an source-file into a"
      " relocatable"
      "\n    object file."
      "\n"
      "\n  <target>"
      "\n    The name to be displayed in the short (non-verbose) message"
      "\n    'Compiling <target> ...'; additionally it can be inserted"
      " anywhere into"
      "\n    <compiler-args> (by using the place-holder '%t' as argument"
      "\n"
      "\n  <compiler-args>"
      "\n    the arguments (options and source-file) which are used to"
      " generate the"
      "\n    relocatable object-file."
      "\n"
      "\n  -c <rcfile> (alt: -f <rcfile>)"
      "\n    load <compiler-program> and additional options from a"
      " configuration file"
      "\n"
      "\n  -s (alt: --split-prog)"
      "\n    Assume <compiler-command> being an (incomplete) command line"
      " template, thus"
      "\n    splitting it according to shell-rules"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    Display each command line generated before it's executed instead"
      " of the"
      "\n    short message 'Generating target ...'; also, allow the executed"
      " command"
      "\n    to display it's messages (stdout/stderr)."
      "\n"
      "\nThe command-template constructed from <compiler-program>, default-"
      "options and"
      "\n<compiler-args> may contain constructs like '@ARGV' (a), '%t[<newsfx>"
      "/<oldsfx>]'"
      "\n(b) and '%t' (c). These constructs are replaced by\n"
      "\n  (a) <compiler-args> - instead of appending <compiler-args> to"
      "\n      <compiler-program> and default-options,"
      "\n  (b) <target> - but with the suffix <oldsfx> replaced with <newsfx>"
      " and"
      "\n  (c) <target> - unchanged."
      "\n"
      "\nOnly the first occurrence of '@ARGV' is replaced; each further"
      " occurrences"
      "\nremain unchanged. The announced \"default-options\" are supplied"
      " through an the"
      "\nenvironment variable (either COPTS or CFLAGS with a preference for"
      " COPTS)."
    },
    { "help", NULL, do_help, 0, 1, true, NULL, NULL, NULL, NULL, NULL,
      "%s%s [%s]", "", "<topic>", "",
      "\n\nDisplay usage information on the different topics"
      "\n\nValid topics are:"
    },
    { "link", "ld", do_generate, 0, 0, false,
      "LINKER", DEFAULT_LINKER, NULL, "LOPTS", "LFLAGS",
      "%s[=%s] [-c <rcfile>] [-v] [-s] <target> \\\n           %s",
      "<linker-program>", "<linker-args>",
      "Linking %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  link (alt: ld)"
      "\n    Execute a linker command constructed from a linker program, some"
      " (default-)"
      "\n    options and <linker-args>."
      "\n"
      "\n  <linker-program>"
      "\n    the name of the program used to combine object-files and"
      " libraries to an"
      "\n    executable object file."
      "\n"
      "\n  <linker-args>"
      "\n    the arguments (options/object-files/libraries) which are used to"
      " generate"
      "\n    an executable object-file."
      "\n"
      "\n  <target>"
      "\n    The name to be displayed in the short (non-verbose) message"
      "\n    'Linking <target> ...'; additionally it can be inserted anywhere"
      " into"
      "\n    <linker-args> (by using the place-holder '%t' each argument)"
      "\n"
      "\n  -c <rcfile> (alt: -f <rcfile>)"
      "\n    load <linker-program> and additional options from a configuration"
      " file"
      "\n"
      "\n  -s (alt: --split-prog)"
      "\n    Assume <linker-command> being an (incomplete) command line"
      " template, thus"
      "\n    splitting it according to shell-rules"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    write the command-text which generates <target> to stdout prior"
      " to the"
      "\n    execution of the command (instead of the message 'Generating"
      " <target> ... '"
      "\n    (followed by a status message of either 'done' or 'failed'))"
      "\n"
      "\nThe command-template constructed from <linker-program>, default-"
      "options and"
      "\n<linker-args> may contain constructs like '@ARGV' (a),"
      " '%t[<newsfx>/<oldsfx>]'"
      "\n(b) and '%t' (c). These constructs are replaced by"
      "\n"
      "\n  (a) <linker-args> - instead of appending <linker-args> to <linker-"
      "program>"
      "\n      and default-options,"
      "\n  (b) <target> - but with the suffix <oldsfx> replaced with <newsfx>"
      " and"
      "\n  (c) <target> - unchanged."
      "\n"
      "\nOnly the first occurrence of '@ARGV' is replaced; each further"
      " occurrences"
      "\nremain unchanged. The announced \"default options\" are supplied"
      " through an"
      "\nenvironment variable (either LOPTS or LFLAGS with a preference for"
      " LOPTS)."
    },
    { "libgen", NULL, do_libgen, 1, 0, false,
      "LIBGENCMD", DEFAULT_LIBGENCMD, NULL, NULL, NULL, 
      "%s[=%s] [-v] <target> <object-files>",
      "<libgen-commands>", NULL,
      "Generating %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  libgen"
      "\n    generate a (new) library-file (<target>) from a list of"
      " relocatable input"
      "\n    files"
      "\n"
      "\n  <libgen-commands>"
      "\n    the command template to be used for constructing the commands"
      " which are"
      "\n    used for generating <target>; instead of supplying <libgen-"
      "commands>"
      "\n    directly, the environment variable 'LIBGENCMD' can be used; if"
      " 'LIBGENCMD'"
      "\n    is not defined or empty, '"DEFAULT_LIBGENCMD"' is used instead."
      "\n    The very first occurrence of '@ARGV' is replaced by <object-"
      "files> (each"
      "\n    other occurrence of '@ARGV' remains unchanged); if no '@ARGV'"
      " occurs in"
      "\n    <libgen-commands>, <object-files> is appended to <libgen-"
      "commands> instead."
      "\n    In the completed command-template, the symbol '%t' has a special"
      "meaning:"
      "\n      - '%t[<newsfx>/<oldsfx>]' is replaced with <target>, but with"
      " the suffix"
      "\n        <oldsfx> replaced with <newsfx> in target,"
      "\n      - any other occurrences ot '%t' are replaced with an unchanged"
      " <target>."
      "\n"
      "\n  <target>"
      "\n    The name of the library being generated"
      "\n"
      "\n  <object-files>"
      "\n    the names of the input files used for generating <target>. There"
      " is no"
      "\n    check if the <object-files> are real binary relocatable objects"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    write the command-text which generates <target> to stdout prior"
      " to the"
      "\n    execution of the command (instead of the message 'Generating"
      " <target> ...'"
      "\n    (followed by a status message of either ' done' or ' failed'))"
    },
    { "rogen", NULL, do_libgen, 1, 0, false,
      "ROGENCMD", DEFAULT_ROGENCMD, NULL, NULL, NULL,
      "%s[=%s] [-v] <target> <object-files>",
      "<rogen-commands>", NULL,
      "Generating %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  rogen"
      "\n    generate a (new) relocatable object-file (<target>) from a list"
      " of"
      "\n    relocatable input files"
      "\n"
      "\n  <rogen-commands>"
      "\n    the command template to be used for constructing the commands"
      " which are"
      "\n    used for generating <target>; instead of supplying <rogen-"
      "commands>"
      "\n    directly, the environment variable 'ROGENCMD' can be used; if"
      " 'ROGENCMD'"
      "\n    is not defined or empty, '"DEFAULT_ROGENCMD"' is used instead."
      "\n    The very first occurrence of '@ARGV' is replaced by <object-"
      "files> (each"
      "\n    other occurrence of '@ARGV' remains unchanged); if no '@ARGV'"
      " occurs in"
      "\n    <rogen-commands>, <object-files> is appended to <rogen-commands>"
      " instead."
      "\n    In the completed command-template, the symbol '%t' has a special"
      " meaning:"
      "\n      - '%t[<newsfx>/<oldsfx>]' is replaced with <target>, but with"
      " the suffix"
      "\n        <oldsfx> replaced with <newsfx> in target,"
      "\n      - any other occurrences ot '%t' are replaced with an unchanged"
      " <target>."
      "\n"
      "\n  <target>"
      "\n    The name of the relocatable object-file to be generated"
      "\n"
      "\n  <object-files>"
      "\n    the names of the input files used for generating <target>. There"
      " is no"
      "\n    check if the <object-files> are real binary relocatable objects"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    write the command-text which generates <target> to stdout prior"
      " to the"
      "\n    execution of the command (instead of the message 'Generating"
      " <target> ...'"
      "\n    (followed by a status message of either ' done' or ' failed'))"
    },
    { "sogen", NULL, do_libgen, 1, 0, false,
      "SOGENCMD", DEFAULT_SOGENCMD, NULL, "SOOPTS", NULL,
      "%s[=%s] [-v] <target> <object-files>",
      "<sogen-commands>", NULL,
      "Generating %s ...",
      "\n\nArguments/Options:"
      "\n"
      "\n  sogen"
      "\n    generate a (new) shared object-file (<target>) from a list of"
      "\n    relocatable input files"
      "\n"
      "\n  <sogen-commands>"
      "\n    the command template to be used for constructing the commands"
      " which are"
      "\n    used for generating <target>; instead of supplying <sogen-"
      "commands>"
      "\n    directly, the environment variable 'SOGENCMD' can be used; if"
      " 'SOGENCMD'"
      "\n    is not defined or empty, '"DEFAULT_SOGENCMD"' is used instead."
      "\n    The very first occurrence of '@ARGV' is replaced by <object-"
      "files> (each"
      "\n    other occurrence of '@ARGV' remains unchanged); if no '@ARGV'"
      " occurs in"
      "\n    <sogen-commands>, <object-files> is appended to <sogen-commands>"
      " instead."
      "\n    In the completed command-template, the symbol '%t' has a special"
      " meaning:"
      "\n      - '%t[<newsfx>/<oldsfx>]' is replaced with <target>, but with"
      " the suffix"
      "\n        <oldsfx> replaced with <newsfx> in target,"
      "\n      - any other occurrences ot '%t' are replaced with an unchanged"
      " <target>."
      "\n"
      "\n  <target>"
      "\n    The name of the shared object-file being generated"
      "\n"
      "\n  <object-files>"
      "\n    the names of the input files used for generating <target>. There"
      " is no"
      "\n    check if the <object-files> are real binary relocatable objects"
      "\n"
      "\n  -v (alt: --verbose)"
      "\n    write the command-text which generates <target> to stdout prior"
      " to the"
      "\n    execution of the command (instead of the message 'Generating"
      " <target> ...'"
      "\n    (followed by a status message of either ' done' or ' failed'))"
    },
    { NULL, NULL, 0, 0, 0, false, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL }
};

/* Module-internal global variable holding the name of the program.
*/
static char *progname = NULL, *progpath = NULL;

/* Return true if the first argument (string) is a prefix of the second one
** (or equals the second argument) and false otherwise.
*/
static
bool is_prefix (const char *p, const char *s)
{
    int c;
    while ((c = *p++) == *s++ && c);
    return !c;
}

#if DEBUG
static void
dump_vec (const char *name, int veclen, char **vec, FILE *f)
{
    fprintf (f, "%s = {", name);
    if (veclen > 0) {
	int ix;
	fprintf (f, " \"%s\"", vec[0]);
	for (ix = 1; ix < veclen; ++ix) { fprintf (f, ", \"%s\"", vec[ix]); }
    }
    fputs (" }\n", f); fflush (f);
}
#endif

/* Display either the usage message and terminate (exit-code = 0) or an error
** message concerning the usage and abort the program (exit-code = 64).
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
		fprintf (stderr, "%s: no topic for '%s' available\n",
				 progname, acname);
		exit (64);
	    }
	    printf ("Usage: %s ", progname);
	    printf (act->synopsis, act->pfx_name, act->prog_desc,
		    act->prog_args);
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
    printf ("\n\nFor further help issue '%s help <topic>', where topic is"
	    " one of\n(", progname);
    for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
	if (ix > 0) { printf (", "); }
	printf ("%s", act->pfx_name);
    }
    printf (")\n\n");
    exit (0);
}

/* Duplicate a string
*/
static
char *sdup (const char *s)
{
    char *res = tmalloc (strlen (s) + 1, char);
    if (res) { strcpy (res, s); }
    return res;
}

#include "lib/isws.c"

#include "lib/nows.c"

/*##0*/

#define CC_EOS          0
#define CC_WHITESPACE   1
#define CC_DOUBLEQUOTE  2
#define CC_SINGLEQUOTE  3
#define CC_BACKSLASH    4
#define CC_SEMICOLON    5
#define CC_NORMAL       6
#define NUMCC (CC_NORMAL + 1)

#define S0 0
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define S5 5
#define S6 6
#define S7 7
#define NUMSTATES (S7 + 1)

static char strans[NUMSTATES][NUMCC] = {
/*CC_EOS CC_WHITESPACE CC_DOUBLEQUOTE CC_SINGLEQUOTE CC_BACKSLASH CC_SEMICOLON CC_NORMAL*/
/*S0*/ { S7, S0, S1, S3, S4, S7, S5 },
/*S1*/ { S6, S1, S5, S1, S2, S1, S1 },
/*S2*/ { S6, S1, S1, S5, S1, S1, S1 },
/*S3*/ { S6, S3, S2, S5, S3, S3, S3 },
/*S4*/ { S6, S5, S5, S5, S5, S5, S5 },
/*S5*/ { S6, S6, S1, S3, S4, S6, S5 },
/*S6*/ { S6, S6, S6, S6, S6, S6, S6 },
/*S7*/ { S7, S7, S7, S7, S7, S7, S7 }
};

#define AC_NOOP 0
#define AC_INC1 1
#define AC_INC2 2
#define AC_BACK 3
#define AC_INSCHR 4
#define AC_INSNUL 5
#define AC_INSBSL 6
#define AC_RETN 7
#define AC_RET2 8

#if DEBUG
static const char *
st_name (int state)
{
    switch (state) {
	case S0: return "S0";
	case S1: return "S1";
	case S2: return "S2";
	case S3: return "S3";
	case S4: return "S4";
	case S5: return "S5";
	case S6: return "S6";
	case S7: return "S7";
	default: return "*unknown";
    }
}

static const char *
ac_name (int action)
{
    switch (action) {
	case AC_NOOP:   return "AC_NOOP";
	case AC_INC1:   return "AC_INC1";
	case AC_INC2:   return "AC_INC2";
	case AC_INSCHR: return "AC_INSCHR";
	case AC_INSNUL: return "AC_INSNUL";
	case AC_INSBSL: return "AC_INSBSL";
	case AC_RETN:   return "AC_RETN";
	case AC_RET2:   return "AC_RET2";
	default     :   return "*unknown";
    }
}
#endif

static char r_action[NUMSTATES][NUMCC] = {
       /*  EOS    ' '/'\t'   '"'      '\''     '\\'     ';'       ?   */
/*S0*/ { AC_NOOP, AC_NOOP, AC_NOOP, AC_NOOP, AC_NOOP, AC_NOOP, AC_INC1 },
/*S1*/ { AC_NOOP, AC_INC1, AC_NOOP, AC_INC1, AC_NOOP, AC_INC1, AC_INC1 },
/*S2*/ { AC_INC2, AC_INC1, AC_INC1, AC_INC1, AC_INC1, AC_INC1, AC_INC1 },
/*S3*/ { AC_INC1, AC_INC1, AC_INC1, AC_NOOP, AC_INC1, AC_INC1, AC_INC1 },
/*S4*/ { AC_INC2, AC_INC1, AC_INC1, AC_INC1, AC_INC1, AC_INC1, AC_INC1 },
/*S5*/ { AC_INC1, AC_INC1, AC_NOOP, AC_NOOP, AC_NOOP, AC_BACK, AC_INC1 },
/*S6*/ { AC_RETN, AC_RETN, AC_RETN, AC_RETN, AC_RETN, AC_RETN, AC_RETN },
/*S7*/ { AC_RETN, AC_RETN, AC_RETN, AC_RETN, AC_RETN, AC_RETN, AC_RETN }
};

static int
sh_cclass (int ch)
{
    switch (ch) {
	case '\0':
	    return CC_EOS;
	case ' ': case '\t':
	    return CC_WHITESPACE;
	case '"':
	    return CC_DOUBLEQUOTE;
	case '\'':
	    return CC_SINGLEQUOTE;
	case '\\':
	    return CC_BACKSLASH;
	case ';':
	    return CC_SEMICOLON;
	default:
	    return CC_NORMAL;
    }
}

static size_t
nextword_len (const char *in, const char **_in)
{
    size_t cc = 0;
    int state = S0, action;
    while (state != S6 && state != S7) {
	int ch = *in;
	int cclass = sh_cclass (ch);
	if (cclass != CC_EOS) { ++in; }
	action = r_action[state][cclass];
	state = strans[state][cclass];
	switch (action) {
	    case AC_NOOP: break;
	    case AC_INC1: cc++; break;
	    case AC_INC2: cc += 2; break;
	    case AC_BACK: --in; break;
	}
    }
    *_in = in; return cc;
}

static char w_action[NUMSTATES][NUMCC] = {
       /* EOS ' '/'\t' '"' '\'' '\\' ';' ? */
/*S0*/ { AC_NOOP,   AC_NOOP,   AC_NOOP,   AC_NOOP,   AC_NOOP,   AC_NOOP,
	 AC_INSCHR },
/*S1*/ { AC_INSNUL, AC_INSCHR, AC_NOOP,   AC_INSCHR, AC_NOOP,   AC_INSCHR,
	 AC_INSCHR },
/*S2*/ { AC_INSBSL, AC_INSCHR, AC_INSCHR, AC_INSCHR, AC_INSCHR, AC_INSCHR,
	 AC_INSCHR },
/*S3*/ { AC_INSNUL, AC_INSCHR, AC_INSCHR, AC_NOOP,   AC_INSCHR, AC_INSCHR,
	 AC_INSCHR },
/*S4*/ { AC_INSBSL, AC_INSCHR, AC_INSCHR, AC_INSCHR, AC_INSCHR, AC_INSCHR,
	 AC_INSCHR },
/*S5*/ { AC_INSNUL, AC_INSNUL, AC_NOOP,   AC_NOOP,   AC_NOOP,   AC_BACK,
	 AC_INSCHR },
/*S6*/ { AC_RETN,   AC_RETN,   AC_RETN,   AC_RETN,   AC_RETN,   AC_RETN,
	 AC_RETN   },
/*S7*/ { AC_RET2,   AC_RET2,   AC_RET2,   AC_RET2,   AC_RET2,   AC_RET2,
	 AC_RET2   },
};

static char *
nextword_ins (const char *in, const char **_in, char **_out)
{
    int state = S0, action;
    char *out = *_out, *word = NULL;
    while (state != S6 && state != S7) {
	int ch = *in;
	int cclass = sh_cclass (ch);
	if (cclass != CC_EOS) { ++in; }
	action = w_action[state][cclass];
	state = strans[state][cclass];
	switch (action) {
	    case AC_NOOP:   break;
	    case AC_INSCHR: *out++ = ch; break;
	    case AC_INSNUL: *out++ = '\0'; break;
	    case AC_INSBSL: *out++ = '\\'; *out++ = '\0'; break;
	    case AC_BACK: --in; *out++ = '\0'; break;
	}
    }
    if (state == S6) { word = *_out; }
    *_out = out; *_in = in;
    return word;
}

/* Split a string into a vector of strings using shell-alike word-detection.
*/
static int
shsplit (const char *s, char ***_out, int *_outlen, const char **_rs)
{
    const char *p;
    char *buf, *b, *r, **rv, *v;
    size_t wordsz, max_wsz;
    int wc = 0, ix, jx, ec;

    p = s; max_wsz = 0;
    while ((wordsz = nextword_len (p, &p))) {
	++wc; if (wordsz > max_wsz) { max_wsz = wordsz; }
    }

    if ((rv = (char **) malloc ((++wc) * sizeof(char *))) == NULL) {
	return -1;
    }
    if ((buf = (char *) malloc (max_wsz)) == NULL) {
	free (rv); return -1;
    }

    p = s; ix = 0; b = buf;
    while ((r = nextword_ins (p, &p, &b))) {
	if (!(v = sdup (r))) { goto ERREXIT; }
	rv[ix++] = v; b = buf;
    }
    rv[ix] = NULL;
    if (_rs) { *_rs = p; }
    *_out = rv; *_outlen = ix;
    free (buf);
    return 0;
ERREXIT:
    ec = errno;
    for (jx = 0; jx < ix; ++jx) { free (rv[jx]); }
    free (rv); free (buf);
    errno = ec;
    return -1;
}
/*##1*/

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
    if (!(res = tmalloc (ressz, char *))) { return res; }

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
    while ((lc = tolower (*l++)) == (rc = tolower (*r++)) && lc);
    return (lc&255) - (rc&255);
}
#endif

typedef struct {
    const char *acname,		/* action name */
	       *cfname;		/* configuration name */
    int isopt,			/* cfname is an parameter */
	len;			/* length of configuration name */
} rcdef_t;

static
rcdef_t rcdefs[] = {
    { "compile", "compiler", 0, 8 },
    { "compile", "cc", 0, 2 },
    { "link", "linker", 0, 6 },
    { "link", "ld", 0, 2 },
    { "compile", "compiler_options", 1, 16 },
    { "compile", "ccopts", 1, 6 },
    { "compile", "copts", 1, 5 },
    { "link", "linker_options", 1, 14 },
    { "link", "ldopts", 1, 6 },
    { "link", "lopts", 1, 5 },
    { "libgen", "libgencmd", 0, 6 },
    { "rogen", "rogencmd", 0, 5 },
    { "sogen", "sogencmd", 0, 5 },
    { NULL, NULL, -1, 0 }
};

#include "lib/cuteol.c"

static
int get_ident (char *p, char *q, char **_r)
{
    if (!isalpha (*p) && *p != '_') { return -1; }
    while (p < q && (isalnum (*++p) || *p == '_'));
    if (p < q && nows (*p)) { return -1; }
    *p = '\0'; *_r = ++q;
    return 0;
}

/* Parse a configuration file (returning the names and options for a compiler/
** linker via the last argument). A return value of -1 indicates an opening
** failure, 0 a success and a positive value the number of errors found during
** the parsing process ...
*/
static
int read_cgenrc (const char *cgenrc, cdesc_t *_out, int *_outlen)
{
    int lc = 0, errc = 0, ix, ndesc = 0;
    FILE *fp;
    rcdef_t *rcdef;
    cdesc_t out = NULL, o1 = NULL;
    char buf[1024], rem[1024], *p, *q, *v;
    if (!cgenrc) { return 0; }
    if (!(fp = fopen (cgenrc, "r"))) { return -1; }
    while (fgets (buf, sizeof(buf), fp)) {
	++lc;
	if (!cuteol (buf)) {
	    while (fgets (rem, sizeof(rem), fp) && !cuteol (rem));
	}
	p = buf; while (isws (*p)) { ++p; }
	if (!*p || *p == '#') { continue; }
	if (*p == '=') {
	    ++errc; fprintf (stderr, "%s(line %d): expecting a cfname\n",
				     cgenrc, lc);
	    continue;
	}
	if (!(q = strchr (p, '='))) {
	    ++errc; fprintf (stderr, "%s(line %d): expecting '='\n", cgenrc,
				     lc);
	    continue;
	}
	if (get_ident (p, q, &q)) {
	    ++errc; fprintf (stderr, "%s(line %d): invalid identifier\n",
				     cgenrc, lc);
	    continue;
	}

	/* Try to find the identifier in the list of valid cfnames ... */
	for (ix = 0; (rcdef = &rcdefs[ix])->cfname; ++ix) {
	    if (!lccmp (rcdef->cfname, p)) { break; }
	}

	/* It is an error if the identifier is not in this list ... */
	if (!rcdef->cfname) {
	    ++errc; fprintf (stderr, "%s(line %d): invalid variable\n", cgenrc,
				     lc);
	    continue;
	}

	/* Set p to point behind the position of the '=' ... */
	p = q;

	/* Search for 'acname' in the list of the already loaded values ... */
	for (ix = 0; ix < ndesc; ++ix) {
	    if (!strcmp (rcdef->acname, out[ix].acname)) { break; }
	}

	/* If the 'acname'-configuration was not already loaded, then generate
	** a new one ...
	*/
	if (ix >= ndesc) {
	    if (!(o1 = (cdesc_t) realloc (out, ++ndesc * sizeof(*out)))) {
		fprintf (stderr, "%s: %s\n", progname, strerror (errno));
		exit (1);
	    }
	    out = o1;
	    out[ix].acname = rcdef->acname;
	}

	/* It is an error if this configuration was already loaded ... */
	if ((rcdef->isopt && out[ix].popts)
	||  (!rcdef->isopt && out[ix].prog)) {
	    ++errc;
	    fprintf (stderr, "%s(line %d): ambiguous '%s'\n", cgenrc, lc,
			     rcdef->cfname);
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

    /* If the configuration file was correct, the configuration list (and it's
    ** length) are returned (through the last two parameters); otherwise,
    ** release all items allocated to this point ...
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
	    /* Keiner der üblichen Verdächtigen ==> Wort kann direkt ausgegeben
	    ** werden
	    */
	    fputs (w, out);
	} else if (!strpbrk (w, "\"\\`$")) {
	    /* Kein ", \, ` oder $ ==> Wort kann in " eingeschlossen ausgegeben
	    ** werden
	    */
	    fputc ('"', out); fputs (w, out); fputc ('"', out);
	} else {
	    /* ", \, ` oder $ gefunden ==> Wort muß in ' eingeschlossen werden
	    */
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

/* Write either " failed\n", " failed:\n", " done:\n" or " done\n", depending
** on the value of (the integer) 'exitstate' and the flag 'something_follows',
** to the specified output "file".
*/ 
static
void print_exitstate (FILE *out, int exitstate, int something_follows)
{
    fputs ((exitstate ? " failed" : " done"), out);
    fputs ((something_follows ? ":\n" : "\n"), out);
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

/* Helper structure for collection sub-directories during the recurvive removal
** of a directory.
*/
struct _dlist;
typedef struct _dlist dlist_s, *dlist_t;
struct _dlist {
    dlist_t next;
    char path[1];
};

static int
is_dotordotdot (const char*s)
{
    return (*s == '.' && (!s[1] || (s[1] == '.' && !s[2])));
}

/* Remove a directory - including (recursively) all of it' entries.
*/
static int
rmrec (const char *path, FILE *out)
{
    int rc;
    DIR *dp;
    struct dirent *de;
    size_t bsz = 0, pl; char *buf = NULL, *p;
    dlist_t subdirs = NULL, ne;
    struct stat st;

    if (!(dp = opendir (path))) { return -1; }
    while ((de = readdir (dp))) {
	if (is_dotordotdot (de->d_name)) { continue; }
	pl = strlen (path) + strlen (de->d_name) + 2;
	if (pl > bsz) {
	    bsz = pl + 1023; bsz -= bsz % 1024;
	    if (!(p = (char *) realloc (buf, bsz))) { goto ERREXIT; }
	    buf = p; 
	}
	snprintf (buf, bsz, "%s/%s", path, de->d_name);
	if (lstat (buf, &st)) {
	    if (errno == ENOENT) { continue; }
	    goto ERREXIT;
	}
	if (S_ISDIR (st.st_mode)) {
	    if (!(ne = tsmalloc (dlist_s, strlen (buf)))) { goto ERREXIT; }
	    ne->next = subdirs; subdirs = ne;
	    strcpy (ne->path, buf);
	} else if (rmfile (buf, out)) {
	    goto ERREXIT;
	}
    }
    closedir (dp);
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

static int
rmfsentry (const char *path, FILE *out)
{
    struct stat st;
    if (lstat (path, &st)) { return (errno == ENOENT ? 0 : -1); }
    if (S_ISDIR (st.st_mode)) { return rmrec (path, out); }
    return rmfile (path, out);
}

#if 0
#include "lib/cwd.c"
#endif

/* Perform the 'clean'-action (directly - not calling an external program for
** this reason).
*/
static int
cleanup (FILE *out, const char *wd, int verbose, int nfiles, char **files)
{
    int rc, errs = 0, ix = 0;
    action_t *act;
    if (verbose < 2) {
	for (ix = 0; (act = &actions[ix])->pfx_name; ++ix) {
	    if (!strcmp (act->pfx_name, "clean")) { break; }
	}
	if (act->pfx_name && verbose) {
	    fprintf (out, act->short_msg, wd);
	}
	for (ix = 0; ix < nfiles; ++ix) {
	    rc = rmfsentry (files[ix], NULL);
	    if (rc) { ++errs; }
	}
	if (verbose) { print_exitstate (out, (errs ? 1 : 0), 0); }
    } else {
	for (ix = 0; ix < nfiles; ++ix) {
	    rc = rmfsentry (files[ix], out);
	    if (rc) { ++errs; }
	}
    }
    return (errs ? 1 : 0);
}

static size_t
sfx_subst (const char *w, const char *subst, size_t sl,
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

/* Replace each occurrence of 'pat' (except when prefixed with '\') in 'where'
** with 'subst'.
*/
static char *
rplc (const char *pat, const char *where, const char *subst)
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

    /* 2. Allocate enough memory for the result and fill it with 'where' (with
    **    each occurrence of 'pat' replaced with 'subst') ...
    */
    if ((res = tmalloc (ressz, char))) {
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

/* Generate the command from the program (cmd), the value of the environment
** variable (envvar) and a list of arguments.
*/
char **
gen_cmd (const char *prog, const char *popts, bool split_prog,
	 action_t *act, const char *target, int argc, char **argv,
	 const char **_nxprog)
{
    int ix, jx, kx, nx, optc = 0, progc = 0, cmdc = 0, aviix, px[2], ac;
    const char *sv, *envval, *nxprog = NULL;
    char **progv = NULL, **optv = NULL, **cmdv = NULL, **pv[2], **av;

    /* Retrieve the "program" to be executed if it was not already
    ** specified ...
    */
    if (!prog) {
	/* Try the environment variable for this command first ... */
	sv = prog; if (act->env_cmd) { prog = getenv (act->env_cmd); }
	if (!prog || !*prog) { prog = sv; }
	/* Try the (compiled) default for this command second. Because this
	** default may contain a complete command template instead of only a
	** program-name, we need to assume that 'split_prog' was set; in other
	** words: we will set it manually ...
	*/
	if (!prog || !*prog) { prog = act->default_cmd; split_prog = true; }
	if (prog && !*prog) { prog = sv; }
    }
    /* It is an error if (after the steps above) we could not retrieve a valid
    ** program ...
    */
    if (!prog) {
	usage ("no valid %s specified; see '%s help' for help, please!",
	       act->prog_desc, progname);
    }
    if (split_prog) {
	/* If the '-s'-option was one of the options to 'compile' or 'link'
	** then split 'prog' into a vector of arguments (e.g. special calling
	** of gcc); the splitting is done in a shell-alike manner ...
	*/
	if (shsplit (prog, &progv, &progc, &nxprog)) { return NULL; }

	/* Increase the number of arguments by the length of the 'prog'-
	** vector ...
	*/
    } else {
	/* If 'split_prog' was not requested, then allocate a vector with
	** exactly one element ...
	*/
	if (!(progv = tmalloc (2, char *))) { return NULL; }
	*progv = progv[1] = NULL; if (!(*progv = sdup (prog))) { goto ERREXIT; }
	/* Increase the number of arguments by the length of the 'prog'-
	** vector ...
	*/
	progc = 1;
    }
    cmdc += progc;

    /* Retrieve the 'options'-string ... */
    envval = NULL;
    if (!(act->env_opts && (envval = getenv (act->env_opts)) && *envval)) {
	envval = NULL; if (act->env_flags) { envval = getenv (act->env_flags); }
    }
    if (!envval || !*envval) { envval = popts; }

    /* And generate an 'options'-vector from this string ... */
    if (envval && *envval) {
	if (shsplit (envval, &optv, &optc, NULL)) { goto ERREXIT; }
	/* Increase the number of arguments by the length of the options-
	** vector ...
	*/
    } else {
	if (!(optv = tmalloc (1, char *))) { goto ERREXIT; }
	*optv = NULL; optc = 0;
    }
    cmdc += optc;

    /* Increase the number of arguments by the number of the remaining
    ** arguments ...
    */
    cmdc += argc;

    /* Allocate enough memory for the command vector ... */
    if (!(cmdv = tmalloc (cmdc + 1, char *))) { goto ERREXIT; }
    memset (cmdv, 0, (cmdc + 1) * sizeof(char *));

    pv[0] = progv; pv[1] = optv;
    px[0] = progc; px[1] = optc;

    /* Now fill the command vector piece by piece ... */
    ix = 0; aviix = -1; nx = -1;
    for (kx = 0; kx < 2; ++kx) {
	ac = px[kx]; av = pv[kx];
	for (jx = 0; jx < ac; ++jx) {
	    if (!strcmp (av[jx], "@ARGV")) {
		free (av[jx]); aviix = jx + 1; nx = kx; goto NX;
	    }
	    cmdv[ix++] = av[jx];
	}
    }
NX:
    /* Insert the command line arguments ... */
    for (jx = 0; jx < argc; ++jx) {
	if (!(cmdv[ix++] = sdup (argv[jx]))) { goto ERREXIT; }
    }

    /* Insert the remaining options ... */
    if (aviix > 0) {
	ac = px[nx]; av = pv[nx];
	for (jx = aviix; jx < ac; ++jx) {
	    cmdv[ix++] = av[jx]; av[jx] = NULL;
	}
	for (kx = nx + 1; kx < 2; ++kx) {
	    ac = px[kx]; av = pv[kx];
	    for (jx = 0; jx < ac; ++jx) {
		cmdv[ix++] = av[jx]; av[jx] = NULL;
	    }
	}
    }

    /* End of the command vector */
    cmdv[ix] = NULL; cmdc = ix;


    /* Empty the 'progv' and 'optv' vectors (as their elements were moved to
    ** 'cmdv' ...
    */
    for (jx = 0; jx < progc; ++jx) { progv[jx] = NULL; }
    for (jx = 0; jx < optc; ++jx) { optv[jx] = NULL; }

    /* After the command vector was filled, each argument is modifier by
    ** substituting each '%t' with 'target' ...
    */
    for (jx = 0; jx < ix; ++jx) {
	char *sv = rplc ("%t", cmdv[jx], target);
	if (!sv) { goto ERREXIT; }
	free (cmdv[jx]); cmdv[jx] = sv;
    }

    /* Remove the (no longer used) 'progv' and 'optv' vectors ... */
    if (progv) { argv_free (progv); progv = NULL; }
    if (optv) { argv_free (optv); optv = NULL; }

    /* Return the remaining 'prog'-cmdline through the '_nxprog'-argument (if
    ** this argument is valid ...
    */
    if (_nxprog) { *_nxprog = nxprog; }

    /* Return the command vector ... */
    return cmdv;

ERREXIT:
    if (progv) { argv_free (progv); }
    if (optv) { argv_free (optv); }
    if (cmdv) { argv_free (cmdv); }
    return NULL;
}

/* Check if the argument points to a regular file which is executable for the
** calling user.
*/
static bool
is_xfile (const char *path)
{
    struct stat st;
    if (stat (path, &st)) { return false; }
    if (!S_ISREG (st.st_mode)) { return false; }
    return (access (path, X_OK) ? false : true);
}

/* Return either (a copy of) the argument if this argument is a (relative or
** absolute) pathname or try to find the argument in the PATH and return it's
** absolute pathname.
*/
static char *
which (const char *cmd)
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
	/* Remaining path element after the last ':' ... */
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

typedef struct buflist *buflist_t;
struct buflist {
    buflist_t next;
    size_t buflen;
    char *buf;
};

static
int buflist_append (buflist_t *_first, buflist_t *_last,
		    const char *buf, size_t buflen)
{
    buflist_t res = malloc (sizeof(struct buflist) + buflen);
    if (! res) { return -1; }
    res->next = NULL;
    res->buflen = buflen;
    res->buf = (char *)res + sizeof(struct buflist);
    memcpy (res->buf, buf, res->buflen);
    if (*_first) {
	(*_last)->next = res; (*_last) = res;
    } else {
	(*_first) = res; (*_last) = res;
    }
    return 0;
}

static
void buflist_out (FILE *out, buflist_t head)
{
    while (head) {
	buflist_t next = head->next;
	fwrite (head->buf, sizeof(char), head->buflen, out);
	head = next;
    }
}

static
void buflist_free (buflist_t *_head)
{
    while (*_head) {
	buflist_t next = (*_head)->next;
	memset ((*_head), 0, sizeof(struct buflist) + (*_head)->buflen);
	free (*_head);
	*_head = next;
    }
}

#include "lib/opentty.h"

/* Perform the requested action ('compile' or 'link') by executing the
** corresponding command in a sub-process. Display the output depending on the
** 'verbose' argument.
*/
static int
spawn (FILE *out, int verbose, bool split_prog,
       action_t *act, const char *prog, const char *popts,
       const char *target, int argc, char **argv, const char **_nxcmd)
{
    extern char **environ;
    char **cmdv;
    const char *cmd, *nxcmd = NULL;
    pid_t child;
    int waitstat, excode, cmdout[2], ec;
    cmdv = gen_cmd (prog, popts, split_prog, act, target, argc, argv, &nxcmd);
    if (_nxcmd) { *_nxcmd = nxcmd; }
    if (!cmdv) { return -1; }
    if (!(cmd = which (cmdv[0]))) { return -1; }
    cmdout[0] = -1; cmdout[1] = -1;
    if (verbose > 0) {
	print_command (out, cmdv);
    } else if (verbose == 0) {
	fprintf (out, act->short_msg, target);
	//if (pipe (cmdout) < 0) { return -1; }
	/* I'm abusing the 'tty'-features of the classical unix systems a bit
	** here as a 'pipe()'-replacement. This seems the only way to get a
	** colorized output from the compilers without using a special flag
	** which changes from compiler to compiler ...
	*/
	if (opentty (cmdout, 1)) { return -1; }
    }
    /*if ((out_fd = open ("/dev/null", O_WRONLY|O_APPEND)) < 0) { return -1; }*/
    fflush (stdout); fflush (stderr);
    switch (child = fork ()) {
	case -1: /* ERROR (fork failed) */
	    //close (out_fd);
	    ec = errno; close (cmdout[1]); close (cmdout[0]); errno = ec;
	    return -1;
	case 0:  /* CHILD */
	    if (verbose <= 0) {
		close (cmdout[0]);
		dup2 (cmdout[1], 1); dup2 (cmdout[1], 2);
		close (cmdout[1]);
	    }
	    /*if (verbose <= 0) { dup2 (out_fd, 1); dup2 (out_fd, 2); }*/
	    /*close (out_fd);*/
	    execve (cmd, cmdv, environ);
	    exit (1);
	default: /* PARENT */
	    /* Free the unused resources ... */
	    argv_free (cmdv);
	    /*close (out_fd);*/

	    if (verbose <= 0) {
		int rc = 0;
		buflist_t first = NULL, last = NULL;
		char buf[128];
		ssize_t rlen = 0;
		close (cmdout[1]);
		while ((rlen = read (cmdout[0], buf, sizeof(buf))) > 0) {
		    if (rc == 0) {
			rc = buflist_append (&first, &last, buf, (size_t)rlen);
		    }
		}
		close (cmdout[0]);
		waitpid (child, &waitstat, 0);
		excode = 0;
		if (WIFEXITED (waitstat)) {
		    excode = WEXITSTATUS (waitstat);
		} else if (WIFSIGNALED (waitstat)) {
		    excode = -WTERMSIG (waitstat);
		}
		if (verbose == 0) {
		    print_exitstate (stdout, excode, (excode != 0));
		}
		/* I want an output only if some errors occurred ... */
		if (excode != 0) {
		    buflist_out (stderr, first);
		    if (rc != 0) {
			fputs ("\n(output incomplete)\n", stderr);
		    }
		}
		buflist_free (&first); last = first;
	    } else {
		/* Wait for the child process to terminate ... */
		waitpid (child, &waitstat, 0);

		/* ... and retrieve it's exit status ... */
		excode = 0;
		if (WIFEXITED (waitstat)) {
		    excode = WEXITSTATUS (waitstat);
		} else if (WIFSIGNALED (waitstat)) {
		    excode = -WTERMSIG (waitstat);
		}
		if (verbose == 0) { print_exitstate (stdout, excode, 1); }
	    }
	    return (excode ? -1 : 0);
    }
    //return -1;
}

static void
check_args (action_t *act, int argc)
{
    if (argc < act->min_args) {
	usage ("missing argument(s) for %s; see '%s help' for more, please!",
	       act->pfx_name, progname);
    }
    if (act->max_args > act->min_args && argc - 1 > act->max_args) {
	usage ("too many arguments for %s; see '%s help' for more, please!",
	       act->pfx_name, progname);
    }
}

static int
do_help (action_t *act, const char *prog, int argc, char **argv)
{
    char *topic = NULL;
    if (prog) { usage ("%s=<program> not allowed here", act->pfx_name); }
    check_args (act, argc);
    if (argc > 1) { topic = argv[1]; }
    usage ("help", topic);
    return 0;
}

static int
do_clean (action_t *act, const char *prog, int argc, char **argv)
{
    int ix, verbosity;
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
	    if (newdir) { usage ("ambiguous option '-C'"); }
	    if (argv[ix][2]) {
		newdir = &argv[ix][2];
	    } else if (ix + 1 < argc) {
		newdir = argv[++ix];
	    } else {
		usage ("missing argument for option '%s'", argv[ix]);
	    }
	    continue;
	}
	if (!strncmp (argv[ix], "--cd=", 5)) {
	    if (newdir) { usage ("ambiguous option '--cd'"); }
	    if (!*(newdir = &argv[ix][5])) {
		usage ("invalid empty argument for option '--cd'");
	    }
	    continue;
	}
	if (!strncmp (argv[ix], "--chdir=", 8)) {
	    if (newdir) { usage ("ambiguous option '--chdir'"); }
	    if (!*(newdir = &argv[ix][8])) {
		usage ("invalid empty argument for option '--chdir'");
	    }
	    continue;
	}
	if (!strcmp (argv[ix], "--cd") || !strcmp (argv[ix], "--chdir")) {
	    if (newdir) { usage ("ambiguous option '%s'", argv[ix]); }
	    if (ix + 1 < argc) {
		usage ("missing argument for option '%s'", argv[ix]);
	    }
	    continue;
	}
	if (*argv[ix] != '-') { break; }
	usage ("invalid option '%s'", argv[ix]);
    }

    check_args (act, argc - ix);

    if (newdir && chdir (newdir)) {
	fprintf (stderr, "%s %s: failed to chdir into '%s' - %s\n",
			 progname, act->pfx_name, newdir, strerror (errno));
	exit (1);
    }
    if (!newdir) { newdir = "."; }
    verbosity = (verbose ? 2 : (silent ? 0 : 1));
    cleanup (stdout, newdir, verbosity, argc - ix, &argv[ix]);
    return 0;
}

static int
do_generate (action_t *act, const char *prog, int argc, char **argv)
{
    int optx, ix, rc, cdesclen = 0, ac, verbose = 0;
    bool split_prog = false;
    char *target = NULL, *cf = NULL, *opt, **av;
    const char *popts = NULL;
    cdesc_t cdesc = NULL;

    for (optx = 1; optx < argc; ++optx) {
	opt = argv[optx]; if (*opt != '-' || !strcmp (opt, "--")) { break; }
	if (is_prefix ("-c", opt) || is_prefix ("-f", opt)) {
	    if (cf) { usage ("ambiguous '-c/-f'-option"); }
	    if (opt[2]) {
		cf = &opt[2];
	    } else {
		if (optx >= argc - 1) {
		    usage ("missing argument for option '-c/-f'");
		}
		cf = argv[++optx];
	    }
	    continue;
	}
	if (!strcmp (opt, "-s") || !strcmp (opt, "--split-prog")) {
	    split_prog = true; continue;
	}
	if (!strcmp (opt, "-v") || !strcmp (opt, "--verbose")) {
	    verbose = 1; continue;
	}

	usage ("invalid option '%s'", opt);
    }

    if (!cf && access (".cgenrc", F_OK) == 0) { cf = ".cgenrc"; }
    rc = read_cgenrc (cf, &cdesc, &cdesclen);
    if (rc > 0) {
	fprintf (stderr, "%s: errors in configuration file\n", progname);
	exit (1);
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
    rc = spawn (stdout, verbose, split_prog, act, prog, popts, target, ac, av,
		NULL);
    //if (verbose == 0) { print_exitstate (stdout, rc); }
    return (rc ? 1 : 0);
}

static int
do_libgen (action_t *act, const char *prog, int argc, char *argv[])
{
    /* 1. Step: Set the program-name ... */
    char *target = NULL, *cf = NULL, **av, *opt, *nullarg[] = { NULL };
    const char *nxprog = NULL;
    //const char *popts = NULL;
    int optx = 0, ac, rc, cdesclen = 0, ix;
    int verbose = 0, verb1;
    cdesc_t cdesc = NULL;

    for (optx = 1; optx < argc; ++optx) {
	opt = argv[optx]; if (*opt != '-' || !strcmp (opt, "--")) { break; }
	if (is_prefix ("-c", opt) || is_prefix ("-f", opt)) {
	    if (cf) { usage ("ambiguous '-c/-f'-option"); }
	    if (opt[2]) {
		cf = &opt[2];
	    } else {
		if (optx >= argc - 1) {
		    usage ("missing argument for option '-c/-f'");
		}
		cf = argv[++optx];
	    }
	    continue;
	}
	if (!strcmp (opt, "-v") || !strcmp (opt, "--verbose")) {
	    verbose = 1; continue;
	}

	usage ("invalid option '%s'", opt);
    }

    if (!cf && access (".cgenrc", F_OK) == 0) { cf = ".cgenrc"; }
    if (cf) {
	rc = read_cgenrc (cf, &cdesc, &cdesclen);
	if (rc > 0) {
	    fprintf (stderr, "%s: errors in configuration file\n", progname);
	    exit (1);
	}
	if (cdesc) {
	    for (ix = 0; ix < cdesclen; ++ix) {
		if (!strcmp (act->pfx_name, cdesc[ix].acname)) {
		    //popts = cdesc[ix].popts;
		    if (!prog) { prog = cdesc[ix].prog; }
		    break;
		}
	    }
	}
    }

    check_args (act, argc - optx);
    target = argv[optx++];

    ac = argc - optx; av = &argv[optx];
    rc = spawn (stdout, verbose, true, act, prog, NULL, target, ac, av,
		&nxprog);
    verb1 = (verbose > 0 ? verbose : -1);
    while (rc == 0 && nxprog && *nxprog) {
	prog = nxprog; nxprog = NULL;
	rc = spawn (stdout, verb1, true, act, prog, NULL, target, 0, nullarg,
		    &nxprog);
    }
    //if (verbose == 0) { print_exitstate (stdout, rc); }
    return (rc ? 1 : 0);
}

#if NORMALIZED_PROGPATH
static void
normalize_path (const char *in, char **_out)
{
    const char *p = in, *r;
    char *res, *wrk, *q, lastchar;
    if (!(wrk = (char *) malloc (strlen (in) + 1))) {
	fprintf (stderr, "%p: %s\n", progname, strerror (errno)); exit (1);
    }
    q = wrk; lastchar = '/';
    while (*p) {
	r = p;
	if (*p == '.' && (p == in || lastchar == '/')) {
	    lastchar = *++p; if (*p == '/' || *p == '\0') { continue; }
	    if (*p == '.') {
		lastchar = *++p;
		if (*p == '/' || *p == '\0') {
		    if (q != wrk) { --q; while (q != wrk && *--q != '/'); }
		    if (*q == '/') { ++q; }
		    continue;
		}
	    }
	    if (q == wrk || *(q - 1) != '/') { *q++ = '/'; }
	    p = r; while (*p != '\0' && *p != '/') { *q++ = *p++; }
	    lastchar = *p; if (*p == '/') { *q++ = *p++; }
	} else if (*p == '/') {
	    if (p++ == in || lastchar != '/') { lastchar = (*q++ = '/'); }
	} else {
	    lastchar = (*q++ = *p++);
	}
    }
    if (q != wrk) { if (*--q != '/') { ++q; } }
    if (!(res = (char *) malloc ((size_t) (q - wrk) + 1))) {
	fprintf (stderr, "%p: %s\n", progname, strerror (errno)); exit (1);
    }
    memcpy (res, wrk, (size_t) (q - wrk)); res[q - wrk] = '\0';
    free (wrk);
    *_out = res;
}

#include "lib/bconc.c"

#endif /*NORMALIZED_PROGPATH*/

/* Main program
**
*/
int main (int argc, char *argv[])
{
    int mode;
    char *p, *prog = NULL;
    action_t *act;

#if NORMALIZED_PROGPATH
    const char *wd;
    size_t progsz = 0;
    if (*argv[0] == '/') {
	normalize_path (argv[0], &p);
    } else {
	wd = cwd ();
	if (!bconc (&prog, &progsz, cwd, "/", *argv)) {
	    fprintf (stderr, "%s: FATAL ERROR - %s\n", *argv, strerror (errno));
	    exit (71);
	}
	normalize_path (prog, &p); free (prog); prog = NULL;
    }
#else /*NORMALIZED_PROGPATH*/
    p = argv[0];
#endif /*NORMALIZED_PROGPATH*/
    progname = strrchr (p, '/');
    if (progname) {
	progpath = p; *progname++ = '\0';
    } else {
	progpath = NULL; progname = p;
    }

    if (argc - 1 < 1) {
	usage ("missing argument(s); see '%s help' for more, please!",
	       progname);
    }

    /* Argument: compile=<prog> | link=<prog> | clean | help */
    p = argv[1];
    if ((prog = strchr (p, '='))) {
	/* Get the program name */
	*prog++ = '\0'; if (!*prog) { prog = NULL; }
    }

    /* Select the action routine from the command-parameter 'p' ...
    */
    for (mode = 0; (act = &actions[mode], act->pfx_name); ++mode) {
	if (is_prefix (p, act->pfx_name)) { break; }
	if (act->eq_name && !strcmp (p, act->eq_name)) { break; }
    }
    if (!act->pfx_name) { usage ("invalid command '%s'", p); }

    return act->proc (act, prog, argc - 1, &argv[1]);
}
