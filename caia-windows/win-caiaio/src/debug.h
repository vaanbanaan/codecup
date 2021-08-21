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

#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_ON "I: debug flag is set\n"
#define DEBUG_MAINLOOP_STARTED "I: caiaio mainloop has started\n"
#define DEBUG_MAINLOOP_ENDED "I: caiaio mainloop has ended\n"
#define DEBUG_MESSAGE_FROM_MANAGER "M> %s"
#define DEBUG_MESSAGE_FROM_REFEREE "R> %s"
#define DEBUG_MESSAGE_FROM_PLAYER "%d> %s"
#define DEBUG_OK "  <I: ok>\n"
#define DEBUG_SEND_OK_BACK_TO_REFEREE "  <I: sends \"lock_ok\" to the referee>\n"
#define DEBUG_SEND_OK_BACK_TO_MANAGER "  <I: sends \"lock_ok\" to the manager>\n"
#define DEBUG_CAIAIO_SENDS_TO_MANAGER "  <I: sends \"%s\" to the manager>\n"
#define DEBUG_CAIAIO_SENDS_TO_REFEREE "  <I: sends \"%s\" to the referee>\n"
#define DEBUG_CAIAIO_SENDS_TO_PLAYER "  <I: sends \"%s\" to player %d>\n"
#define DEBUG_NEWLINE "\n"
#define DEBUG_GOODBYE_STRING "I: real time used = %d seconds\n"
#define DEBUG_PROGRAM_STOPPED "I: the program %s has stopped\n"
#define DEBUG_INFO_ON_STREAM "I: there is still information on the stream\n"
#define DEBUG_INFO_IN_BUFFER "I: there is still information in the buffer\n"
#define DEBUG_MANAGER_CLOSED_NO_BUFFER "Manager closed and no info in buffer\n"
#define DEBUG_ERROR_LOGFILE "Error opening logfile %s: %s\n"
#define DEBUG_CPU_SPEED "  <I: time factor is set to %.3f>\n"

#endif //DEBUG_H

