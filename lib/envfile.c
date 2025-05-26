/* envfile.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
** License: GNU General Public License, version 2
**
** Parse an environment file and set the environment variables from the
** definitions in this file
**
*/
#ifndef ENVFILE_C
#define ENVFILE_C

#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
# define ENVFILE_EXT_DEFINED
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef ENVFILE_EXT_DEFINED
# undef _DEFAULT_SOURCE
# undef ENVFILE_EXT_DEFINED
#endif

#include "isws.c"
#include "bgetline.c"

static bool iswsoreol (int ch)
{
    return ch == 0 || isws (ch);
}

static bool isnamech (int ch, bool first)
{
    const char *vc = NULL;
    if (ch == 0) { return false; }
    if (first) {
	vc ="_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    } else {
	vc = "0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmno"
	     "pqrstuvwxyz";
    }

    return strchr (vc, ch) != NULL;
}

typedef struct eslist *eslist_t;
struct eslist {
    eslist_t next;
    bool unset;
    char *n, *v;
};

static void eslist_free (eslist_t el)
{
    if (el) {
	int ec = errno;
	eslist_t nxt;
	while (el) { nxt = el->next; free (el); el = nxt; }
	errno = ec;
    }
}

static int get_oct (char **_s)
{
    int res = 0, ix;
    char *s = *_s;
    for (ix = 0; ix < 3; ++ix) {
	int ch = *s++, dg = 0;
	switch (ch) {
	    case '0': dg = 0; break;  case '1': dg = 1; break;
	    case '2': dg = 2; break;  case '3': dg = 3; break;
	    case '4': dg = 4; break;  case '5': dg = 5; break;
	    case '6': dg = 6; break;  case '7': dg = 7; break;
	    default:  dg = -1; break;
	}
	if (dg < 0) { break; }
	res = (res << 3) | dg;
    }
    res &= 0xFF; *_s = s;
    return (res > 0 && ix > 0 ? res : -1);
}

static int get_hex (char **_s)
{
    int res = 0, ix;
    char *s = *_s;
    for (ix = 0; ix < 2; ++ix) {
	int ch = *s++, dg = 0;
	switch (ch) {
	    case '0': dg = 0; break;  case '1': dg = 1; break;
	    case '2': dg = 2; break;  case '3': dg = 3; break;
	    case '4': dg = 4; break;  case '5': dg = 5; break;
	    case '6': dg = 6; break;  case '7': dg = 7; break;
	    case '8': dg = 8; break;  case '9': dg = 9; break;
	    case 'A': case 'a': dg = 10; break;
	    case 'B': case 'b': dg = 11; break;
	    case 'C': case 'c': dg = 12; break;
	    case 'D': case 'd': dg = 13; break;
	    case 'E': case 'e': dg = 14; break;
	    case 'F': case 'f': dg = 15; break;
	    default:  dg = -1; break;
	}
	if (dg < 0) { break; }
	res = (res << 4) | dg;
    }
    res &= 0xFF; *_s = s;
    return (res && ix == 2 ? res : -1);
}

    
static int get_esc (char **_s)
{
    int res = 0;
    char *s = *_s;
    switch (*++s) {
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	    res = get_oct (&s);
	    break;
	case 'a': res = '\a'; break;
	case 'b': res = '\b'; break;
	case 't': res = '\t'; break;
	case 'n': res = '\n'; break;
	case 'v': res = '\v'; break;
	case 'f': res = '\f'; break;
	case 'r': res = '\r'; break;
	case 'e': res = 0x1B; break;
	case '\\': res = '\\'; break;
	// case 'o': ++s; res = get_oct (&s); break;
	case 'x': res = get_hex (&s); break;
	default: /*--s;*/ res = *s; break;
    }
    *_s = s;
    return res;
}


static int parse_envfp (FILE *fp)
{
    eslist_t fst = NULL, lst = NULL, new;

    int lc = 0, ec = 0, res;
    char *buf = NULL, *p, *q, *r, *s, *t;
    size_t bufsz = 0;
    bool cleartheenvironment = false;
    while ((res = bgetline (fp, buf, bufsz) > 0)) {
	p = buf; ++lc;
	if (*p == '#') {
	    // Extract a possible meta command.
	    if (strncmp (p, "#:clear:", 8) == 0 && iswsoreol (p[8])) {
		cleartheenvironment = true;
	    }
	    // In any case, the comment is skipped.
	    continue;
	}
	while (isws (*p)) { ++p; }
	if (! *p) { continue; }	// Skip lines consisting solely of white space
	if (! isnamech (*p, true)) {
	    fprintf (stderr, "Line #%d: Invalid token.\n", lc);
	    ++ec; continue;
	}
	q = p + 1;
	while (isnamech (*q, false)) { ++q; }
	r = q; while (isws (*r)) { ++r; }
	if (! *r) {
	    // Unsetting the given variable ...
	    new = malloc ((size_t) (q - p) + 1 + sizeof(struct eslist));
	    if (! new) {
		int ec;
		eslist_free (fst);
		ec = errno;
		fprintf (stderr, "Line #%d: %s\n", lc, strerror (errno));
		free (buf); errno = ec;
		return -1;
	    }
	    new->next = NULL;
	    new->unset = true;
	    t = (char *) new + sizeof(struct eslist);
	    new->n = t; memcpy (t, p, (size_t)(q - p)); t[q - p] = '\0';
	    new->v = NULL;
	} else {
	    if (*r != '=') {
		fprintf (stderr, "Line #%d' Syntax error\n", lc);
		continue;
	    }
	    s = ++r; t = s;
	    while (*s) {
		if (*s == '\\') {
		    int ch = get_esc (&s);
		    if (ch > 0) { *t++ = ch; }
		} else {
		    *t++ = *s++;
		}
	    }
	    *t = '\0'; s = t;
	    // Setting the given variable.
	    new = malloc ((size_t) (q - p) + (size_t) (t - r) + 2 +
			  sizeof(struct eslist));
	    if (! new) {
		int ec;
		eslist_free (fst);
		ec = errno;
		fprintf (stderr, "Line #%d: %s\n", lc, strerror (errno));
		free (buf); errno = ec;
		return -1;
	    }
	    new->next = NULL;
	    new->unset = false;
	    t = (char *) new + sizeof(struct eslist);
	    new->n = t; memcpy (t, p, (size_t) (q - p)); t += q - p; *t++ = 0;
	    new->v = t; memcpy (t, r, (size_t) (s - r)); t += s - r; *t = 0;
	}
	if (!lst) { fst = lst = new; } else { lst->next = new; }
	lst = new->next;
    }
    if (res < 0) {
	int ec;
	eslist_free (fst);
	ec = errno;
	fprintf (stderr, "Before line #%d: %s; not setting anything\n",
			 lc, strerror (errno));
	free (buf);
	errno = ec;
    }

    if (cleartheenvironment) {
	clearenv();
    }
    for (new = fst; new; new = new->next) {
	if (new->unset) {
	    unsetenv (new->n);
	} else {
	    setenv (new->n, new->v, 1);
	}
    }
    eslist_free (fst);
    free (buf);
    if (ec > 0) {
	fprintf (stderr,
		 "WARNING! Found %d errors in the environment file.\n", ec);
    }
    return 0;
}

int read_envfile (const char *tag, const char *dir)
{
    char filename[PATH_MAX];
    int len, rc = 0, ec;
    FILE *envfp;
    if (dir) {
	len = snprintf (filename, PATH_MAX, "%s/.env.%s", dir, tag);
    } else {
	len = snprintf (filename, PATH_MAX, "./.env.%s", tag);
    }
    if ((size_t) len > PATH_MAX) {
	fprintf (stderr, "read_envfile(): tag and/or directory too long.\n");
	errno = EINVAL;
	return -1;
    }
    if (! (envfp = fopen (filename, "rb"))) {
	return -1;
    }
    rc = parse_envfp (envfp);
    ec = errno; fclose (envfp); errno = ec;
    return rc;

}

#endif /*ENVFILE_C*/
