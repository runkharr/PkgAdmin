#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

struct tsz{
    char *name;
    int size;
};

static struct tsz unsigned_types[] = {
    { "unsigned char", sizeof(unsigned char) },
    { "unsigned short", sizeof(unsigned short) },
    { "unsigned int", sizeof(unsigned int) },
    { "unsigned long", sizeof(unsigned long) },
    { "unsigned long long", sizeof(unsigned long long) },
    { NULL, 0 }
};

static struct tsz signed_types[] = {
    { "char", sizeof(unsigned char) },
    { "short", sizeof(unsigned short) },
    { "int", sizeof(unsigned int) },
    { "long", sizeof(unsigned long) },
    { "long long", sizeof(unsigned long long) },
    { NULL, 0 }
};

struct rsz {
    char *name, *as_type;
    int size;
};

static struct rsz out_utypes[] = {
    { "byte_t", NULL, 1 },
    { "word_t", NULL, 2 },
    { "dword_t", NULL, 4 },
    { "qword_t", NULL, 8 },
    { NULL, NULL, 0 }
};

static struct rsz out_stypes[] = {
    { "sbyte_t", NULL, 1 },
    { "sword_t", NULL, 2 },
    { "sdword_t", NULL, 4 },
    { "sqword_t", NULL, 8 },
    { NULL, NULL, 0 }
};

int main (int argc, char *argv[])
{
    char line[1024], tname[60], *p, *q, *r, *name, *as_type;
    int ix, jx, sz;
    for (ix = 0; out_utypes[ix].name != NULL; ++ix) {
	sz = out_utypes[ix].size;
	for (jx = 0; unsigned_types[jx].name != NULL; ++jx) {
	    if (unsigned_types[jx].size == sz) {
		out_utypes[ix].as_type = unsigned_types[jx].name;
		break;
	    }
	}
    }
    for (ix = 0; out_stypes[ix].name != NULL; ++ix) {
	sz = out_stypes[ix].size;
	for (jx = 0; signed_types[jx].name != NULL; ++jx) {
	    if (signed_types[jx].size == sz) {
		out_stypes[ix].as_type = signed_types[jx].name;
		break;
	    }
	}
    }
    while (fgets (line, 1024, stdin) != NULL) {
	if ((p = strstr (line, "typedef <#")) != NULL) {
	    r = p; q = &p[10];
	    if ((p = strstr (q, "#>")) == NULL) { continue; }
	    if (p - q >= 60) { continue; }
	    memcpy (tname, q, (p - q)); tname[p - q] = '\0';
	    for (ix = 0; out_utypes[ix].name != NULL; ++ix) {
		if (!strcmp (out_utypes[ix].name, tname)) {
		    name = out_utypes[ix].name;
		    as_type = out_utypes[ix].as_type;
		    if (as_type != NULL) { goto WRITE_TYPEDEF; }
		}
	    }
	    for (ix = 0; out_stypes[ix].name != NULL; ++ix) {
		if (!strcmp (out_stypes[ix].name, tname)) {
		    name = out_stypes[ix].name;
		    as_type = out_stypes[ix].as_type;
		    if (as_type != NULL) { goto WRITE_TYPEDEF; }
		}
	    }
	}
	fputs (line, stdout);
	continue;
WRITE_TYPEDEF:
	fwrite (line, 1, (r - line), stdout);
	fputs ("typedef ", stdout); fputs (as_type, stdout);
	fputc (' ', stdout); fputs (name, stdout);
	fputc (';', stdout); fputs (&p[2], stdout);
    }
    return 0;
}
