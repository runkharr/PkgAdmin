/* mrmacs.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Some macros for simplifying the use of `malloc()´, `realloc()´ and `free()´
** ...
**
*/
#ifndef MRMACS_C
#define MRMACS_C

#define t_alloc(t, n) ((t *) malloc ((n) * sizeof(t)))
#define t_realloc(t, p, n) ((t *) realloc ((p), (n) * sizeof(t)))
#define cfree(p) do { \
    void **q = &(p); \
    if (*q) { free (*q); *q = 0; } \
} while (0)

#endif /*MRMACS_C*/

