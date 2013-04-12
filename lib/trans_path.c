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

#define CCEOS 0
#define CCSLASH 1
#define CCDOT 2
#define CCOTHER 3

#define S0 0
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define S5 5
#define S6 6

static const char strans[7][4] = {
	   /*CCEOS    CCSLASH  CCDOT    CCOTHER*/
    /*S0*/ { S6,      S1,      S6,      S6 },
    /*S1*/ { S5,      S1,      S2,      S4 },
    /*S2*/ { S5,      S1,      S3,      S4 },
    /*S3*/ { S5,      S4,      S4,      S4 },
    /*S4*/ { S5,      S1,      S4,      S4 },
    /*S5*/ { -1,      -1,      -1,      -1 },
    /*S6*/ { -2,      -2,      -2,      -2 }
};

#define NOP 0
#define STORE 1
#define SKIP 2
#define DOTSTORE 3
#define BACK 4
#define DOTDOTSTORE 5
#define TERMINATE 6
#define ERROR 7

static const char saction[7][4] = {
	  /* CCEOS      CCSLASH  CCDOT        CCOTHER*/
    /*S0*/ { ERROR,     STORE,   ERROR,       ERROR },
    /*S1*/ { TERMINATE, SKIP,    SKIP,        STORE },
    /*S2*/ { TERMINATE, SKIP,    SKIP,        DOTSTORE },
    /*S3*/ { BACK,      BACK,    DOTDOTSTORE, DOTDOTSTORE },
    /*S4*/ { TERMINATE, STORE,   STORE,       STORE },
    /*S5*/ { TERMINATE, ERROR,   ERROR,       ERROR },
    /*S6*/ { ERROR,     ERROR,   ERROR,       ERROR }
};

static
int trans_path (char *p, char *q)
{
    int cclass, action, state = S0, lastch = 0;
    char *sp = p;
    for (;;) {
	switch (*q) {
	    case '\0': cclass = CCEOS; break;
	    case '/':  cclass = CCSLASH; break;
	    case '.':  cclass = CCDOT; break;
	    default:   cclass = CCOTHER; break;
	}
	action = saction[state][cclass];
	state = strans[state][cclass];
	switch (action) {
	    case NOP:
		lastch = *q; break;
	    case STORE:
		lastch = *q; *p++ = *q++; break;
	    case SKIP:
		/* When skipping, lastch must remain the same */
		/*lastch = *q;*/
		++q; break;
	    case DOTSTORE:
		lastch = *q; *p++ = '.'; *p++ = *q++; break;
	    case BACK:
		--p; if (*p == '/' && p > sp) { --p; }
		while (p > sp && *--p != '/');
		lastch = *q;
		break;
	    case DOTDOTSTORE:
		lastch = *q; *p++ = '.'; *p++ = '.'; *p++ = *q++; break;
	    case TERMINATE:
		if (lastch == '/' && p > sp) { --p; }
		*p = '\0'; return 0;
	    case ERROR:
		return -1;
	}
    }
}

#endif /*TRANS_PATH_C*/
