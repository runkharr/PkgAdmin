
static const char *cwd (void)
{
    static size_t wdsz = 0;
    static char *wd = NULL;
    if (wdsz == 0) {
	wdsz = 1024;
	check_ptr ("cwd", wd = t_allocv (char, wdsz));
    }
    for (;;) {
	unlessnull (getcwd (wd, wdsz)) { return wd; }
	wdsz += 1024;
	check_ptr ("cwd", wd = realloc (wd, wdsz));
    }
}
