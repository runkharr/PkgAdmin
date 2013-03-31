/* lib/bwhich.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Implementation of `which´, which uses an externally supplied buffer ...
**
*/
#ifndef BWHICH_C
#define BWHICH_C

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "lib/mrmacs.c"
#include "lib/check_ptr.c"
#include "lib/cwd.c"
#include "lib/pbCopy.c"

#define BWHICH_BCHUNKSZ 128

#define bwhich(buf, bufsz, cmd) (_bwhich (&(buf), &(bufsz), (cmd)))

static char *_bwhich (char **_buf, size_t *_bufsz, const char *cmd)
{
    char *rv = NULL;
    size_t rvsz = 0;
    static char *ePATH = NULL;
    char *p, *q, *r;
    size_t pl, cmdsz = strlen (cmd);
    struct stat sb;
    if (strchr (cmd, '/')) {
	if (stat (cmd, &sb)) { return NULL; }
	if (!S_ISREG(sb.st_mode)) { return NULL; }
	if (access (cmd, X_OK) < 0) { return NULL; }
	if (cmdsz >= *_bufsz) {
	    rvsz = cmdsz + BWHICH_BCHUNKSZ; rvsz -= rvsz % BWHICH_BCHUNKSZ;
	    check_ptr ("which", rv = t_realloc (char, *_buf, rvsz));
	    *_buf = rv; *_bufsz = rvsz;
	}
	strcpy (*_buf, cmd);
	return *_buf;
    }
    if (!ePATH) {
	ePATH = getenv ("PATH");
	if (!ePATH) {
	    if (geteuid () == 0) {
		ePATH = "/root/bin:/usr/bin:/usr/sbin:/bin:/sbin:"
		       "/usr/local/sbin";
	    } else {
		p = (char *) cwd ();
		q = t_allocv (char, strlen (p) + strlen ("/bin") + 1);
		check_ptr ("which", q);
		sprintf (q, "%s/bin", p);
		p = "/usr/local/bin:/usr/bin:/bin";
		ePATH = t_allocv (char, strlen (q) + strlen (p) + 2);
		check_ptr ("which", ePATH);
		sprintf (ePATH, "%s:%s", q, p);
		cfree (q);
	    }
	}
    }
    p = ePATH;
    while (*p != '\0') {
	q = strchr (p, ':'); if (!q) { q = &p[strlen (p)]; }
	if (p != q) {
	    pl = (q - p) + cmdsz + 1;
	    if (pl >= *_bufsz) {
		rvsz = pl + 128; rvsz -= rvsz % 128;
		check_ptr ("which", rv = realloc (*_buf, rvsz));
		*_buf = rv; *_bufsz = rvsz;
	    }
	    memcpy (*_buf, p, (size_t) (q - p));
	    r = pbCopy (*_buf + (q - p), "/");
	    pbCopy (r, cmd);
	    if (stat (*_buf, &sb)) { goto NEXT; }
	    if (! S_ISREG(sb.st_mode)) { goto NEXT; }
	    if (access (*_buf, X_OK) == 0) { return *_buf; }
	}
NEXT:
	p = q; if (*p == ':') { ++p; }
    }
    return NULL;
}

#endif /*BWHICH_C*/
