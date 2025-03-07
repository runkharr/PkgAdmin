/* parseint.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** A small (and very simple) function for parsing an integer value from a
** string.
**
*/
#ifndef PARSEINT_C
#define PARSEINT_C

#include <stdbool.h>
#include <errno.h>
#include <limits.h>

# ifdef __cplusplus
extern "C" {
# endif

static int parseint (const char *s, int *_out, const char **_next, int max)
{
    bool done = false, overflow = false, negative = false;
    const char *p = s;
    unsigned int out = 0, prev = 0;
    int res = -1;
    if (! p || ! _out) {
	errno = EINVAL; 
    } else {
	/* Extract a sign from the numeric string. */
	if (max == 0) { max = INT_MAX; }
	if (*p == '+' || *p == '-') { negative = (*p++ == '-'); }
	--p; /* Start a normal numerical parsing. */
	while (! done && *++p) {
	    unsigned int dg = 0;
	    switch (*p) {
		case '0': dg = 0; break; case '1': dg = 1; break;
		case '2': dg = 2; break; case '3': dg = 3; break;
		case '4': dg = 4; break; case '5': dg = 5; break;
		case '6': dg = 6; break; case '7': dg = 7; break;
		case '8': dg = 8; break; case '9': dg = 9; break;
		default: done = true; continue;
	    }
	    prev = out;
	    if (! overflow) {
		/* Advance in the conversion *only* if no overflow occurred
		** in the previous round.
		*/
		unsigned int dg0 = dg - (negative ? 1 : 0);
		if (prev > 0 && (max - dg0) / prev < 10) {
		    out = prev; overflow = true;
		}
	    }
	    out = prev * 10 + dg;
	}
	if (p == s) {
	    errno = EDOM;
	} else {
	    *_out = (negative ? - (int) out : (int) out);;
	    if (overflow) { errno = ERANGE; res = 1; } else { res = 0; }
	}
    }
    if (p && _next) { *_next = p; }
    return res;
}

# ifdef __cplusplus
}
# endif

#endif /*PARSEINT_C*/
