/* Copyright (C) 2005 Jaap Taal and Marcel Vlastuin.

This file is part of the Caia project. This project can be
downloaded from the website www.codecup.nl. You can email to
marcel@vlastuin.net or write to Marcel Vlastuin, Perenstraat 40,
2564 SE Den Haag, The Netherlands.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA. */

#ifndef MAINLOOP_H
#define MAINLOOP_H

//#define PLAYER_ERROR_ZEROSTRING_TAG "<caiaio:_player_sends_\"\\n\">"
#define PLAYER_ERROR_ZEROSTRING_TAG "<caiaio:_player_sends_'\\n'>"      // changed 5 September 2010 by Marcel Vlastuin
#define PLAYER_ERROR_STOPPED_TAG "<caiaio:_player_stopped>"
//#define PLAYER_ERROR_CRASHED_TAG "<caiaio:_player_crashed:_%s>"
#define PLAYER_ERROR_CRASHED_TAG "<caiaio:_player_crashed:_%d>"
#define PLAYER_ERROR_TIMEOUT_TAG "<caiaio:_player_timeout>"

#define multiply(x, y) (int)((float)(x)*(y))

void mainloop(void);

#endif

