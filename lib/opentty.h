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

/* Opening a tty, master and slave. If raw is set (!= 0), then the slave-side
** of the tty is set into 'raw' mode (no line control, no interrupts, no echo,
** ...).
*/
int opentty (int fd[2], int raw);

#endif /*OPENTTY_H*/
