
static const char *which (const char *cmd)
{
    static char *rv = NULL;
    static size_t rvsz = 0;
    static char *PATH = NULL;
    char *p, *q;
    size_t pl, cmdsz = strlen (cmd);
    struct stat sb;
    unlessnull (strchr (cmd, '/')) { return cmd; }
    ifnull (PATH) {
	PATH = getenv ("PATH");
	ifnull (PATH) {
	    if (geteuid () == 0) {
		PATH = "/root/bin:/usr/bin:/usr/sbin:/bin:/sbin:"
		       "/usr/local/sbin";
	    } else {
		p = (char *) cwd ();
		q = t_allocv (char, strlen (p) + strlen ("/bin") + 1);
		check_ptr ("which", q);
		sprintf (q, "%s/bin", p);
		p = "/usr/local/bin:/usr/bin:/bin";
		PATH = t_allocv (char, strlen (q) + strlen (p) + 2);
		check_ptr ("which", PATH);
		sprintf (PATH, "%s:%s", q, p);
		free (q);
	    }
	}
    }
    p = PATH;
    while (*p != '\0') {
	q = strchr (p, ':'); ifnull (q) { q = &p[strlen (p)]; }
	if (p != q) {
	    pl = (q - p) + cmdsz + 2;
	    if (pl > rvsz) { check_ptr ("which", rv = realloc (rv, pl)); }
	    sprintf (rv, "%.*s/%s", (int) (q - p), p, cmd);
	    if (stat (rv, &sb)) { goto NEXT; }
	    if (! S_ISREG(sb.st_mode)) { goto NEXT; }
	    if (access (rv, X_OK) == 0) { return rv; }
	}
NEXT:
	p = q; if (*p == ':') { ++p; }
    }
    return NULL;
}
