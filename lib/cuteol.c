/* cuteol.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2013, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Synopsis:
**    int eoltype = cuteol (line);
**
** Removes the 'end of line' characters from the string 'line'; returns
**  0  -  if the string was not terminated by any 'end of line' characters,
**  1  -  if the string was terminated by "\n" (UNIX-EOL),
**  2  -  if the string was terminated by "\r" (MacOS-EOL),
**  3  -  if the string was terminated by "\r\n" (DOS/Windows-EOL)
** ...
**
*/
#ifndef CUTEOL_C
#define CUTEOL_C

static int cuteol (char *s)
{
    char *p = s - 1;
    while (*++p);
    if (p == s) { return 0; }
    if (*--p == '\r') { *p = '\0'; return 2; }
    if (*p == '\n') {
	*p = '\0'; if (p > s && *--p == '\r') { *p = '\0'; return 3; }
	return 1;
    }
    return 0;
}

#endif /*CUTEOL_C*/

