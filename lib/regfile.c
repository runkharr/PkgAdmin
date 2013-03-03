/* regfile.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Test if a path points to a regular file (symbolic links resolved)
**
*/
#ifndef REGFILE_C
#define REGFILE_C

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static int is_regfile (const char *path)
{
    struct stat sb;
    if (stat (path, &sb)) { return -1; }
    return (S_ISREG(sb.st_mode) ? 1 : 0);
}

#endif /*REGFILE_C*/
