/* err.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** A printf-alike function for printing error messages - with the additional
** feature of replacing the format elements `%mc´, `%me´, `%mt´ with the error
** number (`%mc´) and error message string (`%me´ or `%mt´). Because of the
** limitations of `printf()´ this is done by constructing a new format-string
** using the functions from `append.c´ where the mentioned format-elements are
** already substituted ...
**
*/
#ifndef ERR_C
#define ERR_C

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
/*#include <signal.h>*/

#include "lib/prog.c"
#include "lib/append.c"

static void err (int ex, const char *format, ...)
{
    int errc = errno, em;
    const char *p, *q, *ep, *eq;
    char *buf = NULL, *bp = NULL, nb[32];
    size_t bufsz = 0, esz, csz;
    va_list eal;
    p = format;
    /* I can't use `format´ directly because of the `%mc´ and `%me´/`%mt´
    ** elements which must be replaced by the value of `errno´ (`%mc´) or
    ** the corresponding error message (`%me´ or `%mt´); so, a new format
    ** string is generated in `buf´/`bufsz´ where the mentioned format elements
    ** are substituted ...
    */
    while ((q = strchr (p, '%'))) {
	em = 0;
	if (!strncmp (q, "%mc", (esz = sizeof("%mc") - 1))
	||  (em = !strncmp (q, "%mt", (esz = sizeof("%mt") - 1)))
	||  (em = !strncmp (q, "%me", (esz = sizeof("%me") - 1)))) {
	    /* Because of the short-circuit or, `esz´ always contains the
	    ** correct length and `em´ is set only when an error message string
	    ** is to be inserted ...
	    */
	    csz = (size_t) (q - p);

	    /* First, insert all between the last position and the position of
	    ** of the detected format element into the buffer ...
	    */
	    if (!(bp = lappend (buf, bufsz, bp, p, csz))) { goto FATAL; }
	    if (em) {
		/* Error message strings cannot be inserted directly - they may
		** contain `%´ which must be redoubled for being skipped in the
		** resulting format string (printf-escape) ...
		*/
		ep = strerror (errc);
		while ((eq = strchr (ep, '%'))) {
		    csz = (size_t) (eq - ep);
		    if (!(bp = lappend (buf, bufsz, bp, ep, csz))) {
			goto FATAL;
		    }
		    if (!(bp = append (buf, bufsz, bp, "%%"))) { goto FATAL; }
		    ep = eq + 1;
		}
		/* Append any residues after the last `%´; this as the normal
		** operation as it is rather unlikely that a `%´ is found in
		** the error message string (unlikely BUT NOT impossible).
		*/
		if (*ep) {
		    bp = append (buf, bufsz, bp, strerror (errc));
		}
	    } else {
		/* The value of `errno´ (presaved in `errc´) is a number and
		** never contains any `%´, so it can be inserted directly ...
		*/
		snprintf (nb, sizeof(nb), "%d", errc);
		bp = append (buf, bufsz, bp, nb);
	    }
	    if (!bp) { goto FATAL; }
	    /* Now skip the format element and continue ... */
	    p = q + esz; continue;
	}
	/* In any other case, skip the `%´ and a (probably following `%´).
	** This means, that the other format elements are included as part of
	** the section of the format-string between two error-format elements
	** ...
	*/
	++q; if (*q == '%') { ++q; }
	p = q;
    }
    /* Insert any residues of the format-string ... */
    if (*p && !(bp = append (buf, bufsz, bp, p))) { goto FATAL; }
    fprintf (stderr, "%s: ", prog);
    va_start (eal, format); vfprintf (stderr, buf, eal); va_end (eal);
    fputs ("\n", stderr); free (buf);
    exit ((ex < 0 || ex > 127) ? 1 : ex);
FATAL:
    fprintf (stderr, "%s: Found an error situation during the execution of"
		     " `err()´! ABORTING!\n", prog);
    /* kill (getpid (), SIGABRT); */
    exit (99);
}

#endif /*ERR_C*/
