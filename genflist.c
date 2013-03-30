/* genflist.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2011, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Scan a complete directory tree for files and write the names of these files to stdout.
**
** Synopsis:
**
**    genflist [-f|-p] [-n] path
**
** Options:
**   -f (full-path)
**     write the pathnames including the 'path'-parameter
**   -p
**     prepend a './' to each pathname printed
**   -n (no directiroes)
**     don't write the names of directories
**
** vim: set tabstop=8 shiftwidth=4 noexpandtab:
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>

#include <sys/stat.h>

#include "lib/slist.c"

typedef int (*travop_t) (const char *path, void *travdata);

int dir_traverse (const char *dirname, travop_t travop, void *travdata)
{
    ##
