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
//#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib/opentty.h"

/* Opening a tty, master and slave. If raw is set (!= 0), then the slave-side
** of the tty is set into 'raw' mode (no line control, no interrupts, no echo,
** ...).
*/
int opentty (int fd[2], int raw)
{
    int mfd, sfd, ec;
    char *ttname = NULL;
    struct termios tts;
    if ((mfd = posix_openpt (O_RDWR|O_NOCTTY)) < 0) { return -1; }
    unlockpt (mfd);
    if (!(ttname = ptsname (mfd))) {
	ec = errno; close (mfd); errno = ec; return -1;
    }
    if ((sfd = open (ttname, O_RDWR)) < 0) {
	ec = errno; close (mfd); errno = ec; return -1;
    }
    if (raw) {
	tcgetattr (sfd, &tts);
	tts.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	tts.c_oflag &= ~OPOST;
	tts.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	tts.c_cflag &= ~(CSIZE|PARENB);
	tts.c_cflag |= CS8;
	//cfmakeraw (&tts);
	tcsetattr (sfd, TCSANOW, &tts);
    }
    fd[0] = mfd; fd[1] = sfd;
    return 0;
}
