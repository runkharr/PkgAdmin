/* gnu-inline.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Define __inline__ as macro if this is not the C-compiler from the GNU
** Compiler Collection (GCC) ...
**
*/
#ifndef GNU_INLINE_C
#define GNU_INLINE_C

#ifndef __GNUC__
#define __inline__
#endif

#endif /*GNU_INLINE_C*/
