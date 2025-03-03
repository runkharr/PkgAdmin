/* cmdvec.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Copy a (part of) an argv[] vector into a new (dynamically allocated)
** NULL-terminated vector of `char *` values.
**
**
*/
#ifndef CMDVEC_C
#define CMDVEC_C

#include <stdlib.h>

static char **gen_cmdvec (char **v, size_t v_len)
{
    size_t ix;
    char **res;
    size_t res_len;
    if (v_len == 0) {
	for (ix = 0; v[ix]; ++ix);
    } else {
	for (ix = 0; ix < v_len; ++ix) {
	    if (! v[ix]) { break; }
	}
    }
    res_len = ix;
    if ((res = (char **) malloc ((res_len + 1) * sizeof(char *)))) {
	for (ix = 0; ix < res_len; ++ix) { res[ix] = v[ix]; }
	res[res_len] = NULL;
    }
    return res;
}

#endif /*CMDVEC_C*/
