/* int2str.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** A simple integer to string conversion into a given buffer.
** if the integer fits into the buffer, a pointer to the buffer is
** returned. Otherwise, `NULL` is returned and `errno` is set to `ENOBUFS`.
** If the buffer pointer is `NULL` of the supplied buffer size is < 3, then
** (again) `NULL` is returned and `errno` is set to `EINVAL`
**
*/
#ifndef INT2STR_C
#define INT2STR_C

#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

# ifdef __cplusplus
extern "C" {
# endif

static const char *int2str (int val, char *buf, size_t bufsz, bool rightbound)
{
    bool sign = (val < 0);
    size_t minsz = 2 + (sign ? 1 : 0);
    char *res = 0;
    if (! buf || bufsz < minsz) {
	errno = EINVAL;
    } else {
	static const char digits[] = "0123456789";
	int dg; bool buf_ovfl = false;
	char *p = buf + bufsz;
	*--p = '\0';
	if (sign) {
	    val = -val;
	    // There is only one number which doesn't change its sign if its
	    // 2s complement is built. This is the number where all bits save
	    // from the highest one are reset,
	    // In this case, the division remainder from the division by 10 is
	    // built and negated. In the next step, the number is divided by 10
	    // and also negated. This negation (2s complement build) will now
	    // succeed. The retrieved digit (division remainder) is then
	    // the first one to be inserted into the buffer.
	    if (val < 0) {
		dg = - (val % 10); val = - (val / 10);
		if (! buf_ovfl) {
		    if (p > buf) {
			*--p = digits[dg];
		    } else {
			buf_ovfl = true;
		    }
		}
	    }
	}
	// All other digits prepend this one.
	do {
	    dg = val % 10; val /= 10;
	    if (! buf_ovfl) {
		if (p > buf) { *--p = digits[dg]; } else { buf_ovfl = true; }
	    }
	} while (val > 0);
	if (sign) {
	    if (p > buf) { *--p = '-'; } else { buf_ovfl = true; }
	}
	if (p > buf) {
	    if (rightbound) {
		while (p > buf) { *--p = ' '; }
	    } else {
		char *q = buf; --q; --p;
		while ((*++q = *++p));
	    }
	}
	if (buf_ovfl) { errno = ENOBUFS; } else { res = buf; }
    }
    return res;
}

# ifdef __cplusplus
}
# endif

#endif /*INT2STR_C*/
