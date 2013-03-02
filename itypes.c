/* itypes.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2011, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Replace all (valid) 'typedef <#typename#>' placeholders in a file with
** a corresponding integer `typedef integer-type typename;´. The result
** is either written to a file (2nd argument to this program) or to
** stdout (if no 2nd argument were supplied or the 2nd argument was a single
** '-').
**
** Synopsis:
**    itypes infile outfile     # read from 'infile', write to 'outfile'
**    itypes - outfile          # read from stdin, write to 'outfile'
**    itypes infile [-]         # read from 'infile', write to stdout
**    itypes [- [-]]            # read from stdin, write to stdout
**    itypes -h                 # display a usage-message
**    itypes --help             # same as `itypes -h´
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

#include <sys/types.h>

struct tsz{
    char *name;
    int size;
};

static struct tsz unsigned_types[] = {
    { "unsigned char", sizeof(unsigned char) },
    { "unsigned short int", sizeof(unsigned short) },
    { "unsigned int", sizeof(unsigned int) },
    { "unsigned long int", sizeof(unsigned long) },
    { "unsigned long long int", sizeof(unsigned long long) },
    { NULL, 0 }
};

static struct tsz signed_types[] = {
    { "signed char", sizeof(unsigned char) },
    { "short int", sizeof(unsigned short) },
    { "int", sizeof(unsigned int) },
    { "long int", sizeof(unsigned long) },
    { "long long int", sizeof(unsigned long long) },
    { NULL, 0 }
};

static struct tsz *intypes[] = {
    unsigned_types, signed_types, NULL
};

struct rsz {
    char *name, *as_type;
    int size;
};

static struct rsz out_utypes[] = {
    { "uint8_t", NULL, 1 },
    { "byte_t", NULL, 1 },
    { "uint16_t", NULL, 2 },
    { "word_t", NULL, 2 },
    { "uint32_t", NULL, 4 },
    { "dword_t", NULL, 4 },
    { "uint64_t", NULL, 8 },
    { "qword_t", NULL, 8 },
    { NULL, NULL, 0 }
};

static struct rsz out_stypes[] = {
    { "int8_t", NULL, 1 },
    { "sbyte_t", NULL, 1 },
    { "int16_t", NULL, 2 },
    { "sword_t", NULL, 2 },
    { "int32_t", NULL, 4 },
    { "sdword_t", NULL, 4 },
    { "int64_t", NULL, 8 },
    { "sqword_t", NULL, 8 },
    { NULL, NULL, 0 }
};

static struct rsz *outtypes[] = {
    out_utypes, out_stypes, NULL
};

#define PROG "itypes"

#include "lib/set_prog.c"

#include "lib/isws.c"

static int wstart (const char *p, const char *s)
{
    return (p == s) || (isws (*--p));
}

static const char *findword (const char *s, const char *w)
{
    size_t wl, pl;
    const char *p = strstr (s, w);
    if (p) {
        wl = strlen (w); pl = strlen (p);
        if (p && wstart (p, s) && pl >= wl && isws (p[wl])) { return p; }
    }
    return NULL;
}

static int process_line (const char *line, FILE *outfile)
{
    int errs = 0, ix, kx;
    char tname[60];
    const char *p, *q, *r, *name, *as_type;
    struct rsz *outtypetbl;
    while ((p = findword (line, "typedef"))) {
        fwrite (line, 1, (size_t) (p - line), outfile);
        q = &p[sizeof ("typedef") - 1];

        /* Skip trailing white space (ASCII BL/HT) ... */
        while (isws (*q)) { ++q; }
        
        /* Is this really `typedef <#name#>´? */
        if (strncmp (q, "<#", 2) != 0) {
            /* No, so write it out upto the position of `q´, advance to
            ** this position and continue from there ...
            */
            fwrite (p, 1, (size_t) (q - p), outfile); line = q; continue;
        }
        /* Don't write the typedef yet ... */
        /*fwrite (p, 1, (size_t) (q - p), outfile); p = q;*/
        q += 2; ix = 0; *tname = '\0'; r = q;
        while (isalnum (*q) || *q == '_') {
            if (ix < sizeof(tname)) { tname[ix++] = *q; }
            ++q;
        }
        if (strncmp (q, "#>", 2) != 0) {
            /* Replace a broken `typedef <#...´ directive with a comment */
            fputs ("/* broken 'typedef <#", outfile);
            fwrite (r, 1, (size_t) (q - r), outfile);
            fputs ("#>' (missing '#>') */", outfile);
            while (isws (*q)) { ++q; }
            if (*q == ';') { ++q; }
            line = q; ++errs; continue;
        }
        if (ix >= sizeof(tname) - 1 || !isalpha (*tname)) {
            /* Replace an invalid typedef <#...#>´ with a comment */
            fputs ("/* invalid name in 'typedef <#", outfile);
            fwrite (r, 1, (size_t) (q - r), outfile);
            fputs ("#>' */", outfile);
            q += 2; while (isws (*q)) { ++q; }
            if (*q == ';') { ++q; }
            line = q; ++errs; continue;
        }
        q += 2; tname[ix] = '\0'; as_type = NULL;
        for (kx = 0; (outtypetbl = outtypes[kx]) && !as_type; ++kx) {
            for (ix = 0; (name = outtypetbl[ix].name) != NULL; ++ix) {
                if (strcmp (name, tname) == 0) {
                    as_type = outtypetbl[ix].as_type; break;
                }
            }
        }
        if (as_type == NULL) {
            /* Replace an unknown typedef <#...#>´ with a comment */
            fprintf (outfile,
                     "/* invalid 'typedef <#%s#>' (unknown name) */",
                     tname);
            while (isws (*q)) { ++q; }
            if (*q == ';' ) { ++q; }
            line = q; ++errs; continue;
        }
        fprintf (outfile, "typedef %s %s", as_type, name);
        r = q; while (isws (*q)) { ++q; }
        if (*q != ';') { fputc (';', outfile); }
        fwrite (r, 1, (size_t) (q - r), outfile);
        line = q;
    }
    if (*line) { fputs (line, outfile); }
    return errs;
}

static int prepare_tables (void)
{
    int ix, jx, kx, sz, errs = 0;
    struct rsz *outtypetbl, *ote;
    struct tsz *intypetbl;
    for (kx = 0; (outtypetbl = outtypes[kx]) != NULL; ++kx) {
        intypetbl = intypes[kx];
        for (ix = 0; outtypetbl[ix].name != NULL; ++ix) {
            sz = outtypetbl[ix].size;
            for (jx = 0; intypetbl[jx].name != NULL; ++jx) {
                if (intypetbl[jx].size == sz) {
                    outtypetbl[ix].as_type = intypetbl[jx].name;
                    break;
                }
            }
        }
    }
    for (kx = 0; (outtypetbl = outtypes[kx]) != NULL; ++kx) {
        for (ix = 0; (ote = &outtypetbl[ix])->name != NULL; ++ix) {
            if (ote->as_type == NULL) {
                fprintf (stderr, "%s: %s has no matching type\n",
                                 prog, ote->name);
                ++errs;
            }
        }
    }
    return errs;
}

static void usage (void)
{
    printf ("Usage: %s [infile [outfile]]\n"
            "       %s -h|--help\n"
            "\nwhere:"
            "\n  infile may be '-' (using stdin)"
            "\n  outfile may be '-' (using stdout)\n",
            prog, prog);
    exit (0);
}

int main (int argc, char *argv[])
{
    FILE *infile, *outfile;
    char line[1024], *infn, *outfn;
    int errs, isstdin = 0, isstdout = 0;

    set_prog (argc, argv);
    if (prepare_tables () > 0) {
        fprintf (stderr, "%s: invalid translation tables. This is an internal"
                         " error which should not occur! ABORTING!\n", prog);
        /*kill (getpid (), SIGABRT);*/
        exit (99);
    }
    if (argc < 2) {
        infile = stdin; isstdin = 1;
        outfile = stdout; isstdout = 1;
    } else {
        if (!strcmp (argv[1], "-h") || !strcmp (argv[1], "--help")) {
            usage ();
        }
        infn = argv[1]; outfn = (argc < 3 ? "-" : argv[2]);
        if (!strcmp (infn, "-")) {
            infile = stdin; isstdin = 1;
        } else {
            if (!(infile = fopen (infn, "r"))) {
                fprintf (stderr, "%s: '%s' - %s\n",
                                 prog, infn, strerror (errno));
                exit (2);
            }
        }
        if (!strcmp (outfn, "-")) {
            outfile = stdout; isstdout = 1;
        } else {
            if (!(outfile = fopen (outfn, "w"))) {
                fprintf (stderr, "%s: '%s' - %s\n",
                                 prog, outfn, strerror (errno));
                fclose (infile);
                exit (2);
            }
        }
    }
    errs = 0;
    while (fgets (line, 1024, infile) != NULL) {
	errs += process_line (line, outfile);
    }
    if (!isstdout) { fclose (outfile); outfile = NULL; }
    if (!isstdin) { fclose (infile); infile = NULL; }
    return (errs > 0 ? 1 : 0);
}
