/* opentty.h
**
** $Id$
**
** Author: Boris Jakubith
** E-Mail: runkharr@googlemail.com
** Copyright: (c) 2018, Boris Jakubith <runkharr@googlemail.com>
** Released under GPL v2.
**
** Interface file for 'opentty.c'
** (Open a tty, both master and slave ends, and (optionally) set it into RAW
**  mode ...)
**
*/
#ifndef OPENTTY_H
#define OPENTTY_H

/* Simplifying the process of opening the "master" part of a pseudo-tty.
** Returns -1 on failure and the file-descriptor of the master-side of
** a pseudo-tty on success.
*/
int pty_getmaster (void);

/* Open the slave-side of a pseudo-tty (using the argument 'mfd', which must be
** the file-descriptor of an opened pseuto-tty master) makes the access to this
** tty exclusive and then sets it into raw mode iff the argument 'raw' is non-
** zero. Returns -1 on failure and the file-descriptio of the slave-side of
** the pseudo-tty on success.
*/
int pty_openslave (int mfd, int raw);

/* Opening a tty, master and slave. and sets the slave-side to exclusive
** access. If raw is set (!= 0), then the slave-side of the tty is set into
** 'raw' mode (no line control, no interrupts, no echo, ...). Returns 0 on
** success and passes the filedescriptors through the first argument
** (fd[0] = master, fd[1] = slave) in this case. On failure, -1 is returned
** and the first argument remains unchanged.
*/
int pty_openpair (int fd[2], int raw);

#endif /*OPENTTY_H*/
