/* mrmacs.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Some macros for simplifying the use of 'malloc()', 'realloc()' and 'free()'
** ...
**
*/
#ifndef MRMACS_C
#define MRMACS_C

#include <stdlib.h>

#include "gnu-inline.c"

#define ifnull(x) if ((void *)(x) == NULL)
#define unlessnull(x) if ((void *)(x) != NULL)
#define isNULL(x) ((void *)(x) == NULL)
#define noNULL(x) ((void *)(x) != NULL)
#define t_alloc(t) ((t *) mallocmb (sizeof (t)))
#define t_allocv(t, n) ((t *) mallocmb ((n) * sizeof (t)))
#define t_allocp(t, n) ((t *) mallocmb (sizeof (t) + (n)))
#define t_realloc(t, p, n) ((t *) reallocmb ((p), (n) * sizeof(t)))
#define cfree(p) do { \
    void **q = (void **) &(p); \
    if (*q) { free (*q); *q = 0; } \
} while (0)

static __inline__ void *mallocmb (size_t sz)
{
    if (sz < 64) { sz = 64; }
    return malloc (sz);
}

static __inline__ void *reallocmb (void *p, size_t sz)
{
    if (sz < 64) { sz = 64; }
    return realloc (p, sz);
}

#endif /*MRMACS_C*/
