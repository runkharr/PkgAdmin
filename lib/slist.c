/* slist.c
**
** $Id: mrmacs.c 153 2013-03-03 20:47:13Z bj@rhiplox $
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** A simple type of linear lists of strings, a function which appends a new
** element behind a given one and a function which deletes a given element and
** all of it's successors ...
**
** vim: set tabstop=8 shiftwidth=4 noexpandtab:
*/
#ifndef SLIST_C
#define SLIST_C

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "lib/mrmacs.c"

typedef struct slist *slist_t;
struct slist {
	slist_t next;
	char *sval;
};

#define slist_append(lp, s) (_slist_append (&(lp), (s)))

static int
_slist_append (slist_t *_last, const char *sval)
{
    slist_t ne;
    ifnull (_last) { errno = EINVAL; return -1; }
    unlessnull (ne = t_allocp (struct slist, strlen (sval) + 1)) {
	ne->next = NULL;
	ne->sval = (char *)ne + sizeof(struct slist);
	strcpy (ne->sval, sval);
	unlessnull (*_last) {
	    (*_last)->next = ne;
	}
	*_last = ne;
	return 0;
    }
    return -2;
}

#define slist_free(lp) (_slist_free (&(lp)))

static int
_slist_free (slist_t *_list)
{
    slist_t lp;
    int errsave = errno;
    ifnull (_list) { errno = EINVAL; return -1; }
    while (noNULL(lp = *_list)) {
	*_list = lp->next;
	memset (lp->sval, 0, strlen (lp->sval));
	lp->sval = NULL; lp->next = 0;
	free (lp);
    }
    *_list = NULL; errno = errsave;
    return 0;
}

#endif /*SLIST_C*/
