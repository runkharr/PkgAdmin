/* lib/trans_path.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Normalize an absolute pathname (remove './' occurrences, resolve '../' and
** translate multiple consecutive occurrences of '/' into a single '/') ...
**
*/
#ifndef TRANS_PATH_C
#define TRANS_PATH_C

static int trans_path (char *p, const char *q)
{
    enum cclass { CCEOS = 0, CCSLASH, CCDOT, CCOTHER } cclass;
    enum state { S0 = 0, S1, S2, S3, END }
	state = S0, prev_state;
    enum action { NOOP = 0, STORE, BACK1, BACK, STOP } action;
    static const enum state strans[5][4] = {
	       /* CCEOS    CCSLASH  CCDOT    CCOTHER*/
	/*S0*/  { END,     S1,      S2,      S0 },
	/*S1*/  { END,     S1,      S2,      S0 },
	/*S2*/  { END,     S1,      S3,      S0 },
	/*S3*/  { END,     S1,      S0,      S0 },
	/*END*/ { END,     END,     END,     END },
    };
    static const enum action saction[5][4] = {
	      /*  CCEOS      CCSLASH  CCDOT        CCOTHER*/
	/*S0*/  { NOOP,      STORE,   STORE,       STORE },
	/*S1*/  { NOOP,      NOOP,    STORE,       STORE },
	/*S2*/  { BACK1,     BACK1,   STORE,       STORE },
	/*S3*/  { BACK,      BACK,    STORE,       STORE },
	/*END*/ { STOP,      STOP,    STOP,        STOP },
    };
    char *sp = p;
    if (*q != '/') { return -1; }
    for (;;) {
	int ch = *q;
	if (ch) { ++q; }
	switch (ch) {
	    case '\0': cclass = CCEOS; break;
	    case '/':  cclass = CCSLASH; break;
	    case '.':  cclass = CCDOT; break;
	    default:   cclass = CCOTHER; break;
	}
	action = saction[(int) state][(int) cclass];
	prev_state = state; state = strans[(int) prev_state][(int) cclass];
	switch (action) {
	    case NOOP:
		break;
	    case STORE:
		*p++ = ch; break;
	    case BACK1:
		--p; break;
	    case BACK:
		/* The two dots (after the '/') were already stored, so we
		** need to go back three steps ... */
		p -= 3;
		while (p > sp && *--p != '/');
		++p;
		break;
	    case STOP:
		if (*--p != '/') { ++p; } else if (p <= sp) { ++p; }
		*p = '\0'; return 0;
	}
    }
}

#endif /*TRANS_PATH_C*/
