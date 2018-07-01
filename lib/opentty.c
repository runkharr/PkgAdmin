/* opentty.c
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2018, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Open a tty, both master and slave ends, and (optionally) set it into RAW
** mode ...
**
*/
#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE 1
//#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "lib/opentty.h"

/* Simplifying the process of opening the "master" part of a pseudo-tty.
** Returns -1 on failure and the file-descriptor of the master-side of
** a pseudo-tty on success.
*/
int pty_getmaster (void)
{
    int fd = 0, ec;
    if ((fd = posix_openpt (O_RDWR|O_NOCTTY)) < 0) { return -1; }
    if (grantpt (fd) < 0) { ec = errno; close (fd); errno = ec; return -1; }
    unlockpt (fd);
    return fd;
}

/* Open the slave-side of a pseudo-tty (using the argument 'mfd', which must be
** the file-descriptor of an opened pseuto-tty master) makes the access to this
** tty exclusive and then sets it into raw mode iff the argument 'raw' is non-
** zero. Returns -1 on failure and the file-descriptio of the slave-side of
** the pseudo-tty on success.
*/
int pty_openslave (int mfd, int raw)
{
    int fd = 0, ec;
    const char *ttyname = ptsname (mfd);
    if (!ttyname) { return -1; }
    if ((fd = open (ttyname, O_RDWR)) < 0) { return -1; }
    if (ioctl (fd, TIOCEXCL) < 0) {
	ec = errno; close (fd); errno = ec; return -1;
    }
    if (raw) {
	struct termios tctl;
	tcgetattr (fd, &tctl);
#if 0
	tctl.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	tctl.c_oflag &= ~OPOST;
	tctl.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	tctl.c_cflag &= ~(CSIZE|PARENB);
	tctl.c_cflag |= CS8;
#endif
	cfmakeraw (&tctl);
	tcsetattr (fd, TCSANOW, &tctl);
    }
    return fd;
}

/* Opening a tty, master and slave. and sets the slave-side to exclusive
** access. If raw is set (!= 0), then the slave-side of the tty is set into
** 'raw' mode (no line control, no interrupts, no echo, ...). Returns 0 on
** success and passes the filedescriptors through the first argument
** (fd[0] = master, fd[1] = slave) in this case. On failure, -1 is returned
** and the first argument remains unchanged.
*/
int pty_openpair (int fd[2], int raw)
{
    int mfd, sfd, ec;
    if ((mfd = pty_getmaster ()) < 0) { return -1; }
    if ((sfd = pty_openslave (mfd, raw)) < 0) {
	ec = errno; close (mfd); errno = ec; return -1;
    }
    fd[0] = mfd; fd[1] = sfd;
    return 0;
}
