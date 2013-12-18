/* lib/travdir-types.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** The constants and function types required for 'travdir.c' and 'travdirne.c'
**
*/
#ifndef TRAVDIR_TYPES_C
#define TRAVDIR_TYPES_C

#define FT_FILE 0
#define FT_DIRECTORY 1
#define FT_SYMLINK 2
#define FT_PIPE 3
#define FT_SOCKET 4
#define FT_CHARDEV 5
#define FT_BLOCKDEV 6

typedef int (*travop_t) (const char *path, int filetype, void *travdata);
typedef int (*getfiletype_t) (const char *path, int *_filetype);

#endif /*TRAVDIR_TYPES_C*/
