/* lib/travdirne.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Traverse a directory tree but don't process empty directories ...
**
*/
#ifndef TRAVDIRNE
#define TRAVDIRNE

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <sys/types.h>

#include "lib/mrmacs.c"
#include "lib/slist.c"
#include "lib/pbCopy.c"

#include "lib/dirtrav-types.c"

static int
travdirne (char **_buf, size_t *_bufsz, const char *dirname,
	   getfiletype_t get_filetype, travop_t travop, void *travdata)
{
    int errnosave, rc, dirnotdone = 1, filetype;
    char *path = NULL;
    size_t bufsz = 0, pathlen;
    slist_t dlf = NULL, dlp = NULL;
    DIR *dirp = NULL;
    struct dirent *de = NULL;

    if (isNULL(_buf) || isNULL(_bufsz)) { errno = EINVAL; return -1; }
    ifnull (dirp = opendir (dirname)) { return -2; }
    while (noNULL(de = readdir (dirp))) {
	if (!*de->d_name) { continue; }
	if (*de->d_name == '.') {
	    if (!de->d_name[1]) { continue; }
	    if (de->d_name[1] == '.' && !de->d_name[2]) { continue; }
	}
	if (dirnotdone) {
	    if ((rc = travop (dirname, FT_DIRECTORY, travdata))) {
		goto ERROUT;
	    }
	    dirnotdone = 0;
	}
	pathlen = strlen (dirname) + strlen (de->d_name) + 2;
	if (pathlen >= *_bufsz) {
	    bufsz = pathlen + 127; bufsz -= bufsz % 128;
	    ifnull (path = t_allocv (char, bufsz)) { goto FATAL; }
	    *_buf = path; *_bufsz = bufsz;
	}
	path = pbCopy (*_buf, dirname);
	path = pbCopy (path, "/");
	pbCopy (path, de->d_name);
	if ((rc = get_filetype (*_buf, &filetype))) { goto ERROUT; }
	if (filetype == FT_DIRECTORY) {
	    if (slist_append (dlp, *_buf)) { goto FATAL; }
	    ifnull (dlf) { dlf = dlp; }
	    continue;
	}
	if ((rc = travop (*_buf, filetype, travdata))) { goto ERROUT; }
    }
    closedir (dirp);
    /* Now process the sub-directories ... */
    for (dlp = dlf; noNULL(dlp); dlp = dlf) {
	rc = travdirne (_buf, _bufsz, dlp->sval, get_filetype,
			      travop, travdata);
	if (rc) { goto ERROUT; }
	dlf = dlp->next; dlp->next = NULL; slist_free (dlp);
    }
    return 0;
FATAL:
    rc = -3;
ERROUT:
    errnosave = errno;
    closedir (dirp);
    slist_free (dlf);
    errno = errnosave;
    return rc;
}

#endif /*TRAVDIRNE*/
