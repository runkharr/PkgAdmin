/* separators.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** directory and path separators and end of line  - system dependent.
**
*/
#ifndef SEPARATORS_C
#define SEPARATORS_C

# if defined(__WIN32) || defined(__WIN64) || \
    defined(__MINGW32) || defined(__MINGW64)
#  define DIRSEP '\\'
#  define PATHSEP ';'
#  define EOL "\r\n"
# else
#  define DIRSEP '/'
#  define PATHSEP ':'
#  define EOL "\n"
# endif

#endif /*SEPARATORS_C*/
