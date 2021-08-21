/* Copyright (C) 2005, 2008 Jaap Taal and Marcel Vlastuin.

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

#ifndef ERROR_H
#define ERROR_H

#define STR_DESCR_PROGRAM       "program"
#define STR_DESCR_CCPU          "competition CPU speed"
#define STR_DESCR_TCPU          "test computer CPU speed"
#define STR_DESCR_PLAYNUM       "number of players"
#define STR_DESCR_PLAY          "player number"
#define STR_DESCR_PROG          "program name"
#define STR_DESCR_TIMEOUT       "timeout"
#define STR_DESCR_FIRSTERR      "firsterror"

#define ERROR_INCOMPLETE_CMD "  <I: Incomplete %s command \"%s\", "
#define ERROR_INCOMPLETE_CMD2 " is missing: \"%s\">\n", cmd_sender
#define ERROR_INC_PROGRAM       ERROR_INCOMPLETE_CMD    STR_DESCR_PROGRAM       ERROR_INCOMPLETE_CMD2
#define ERROR_INC_CCPU          ERROR_INCOMPLETE_CMD    STR_DESCR_CCPU          ERROR_INCOMPLETE_CMD2
#define ERROR_INC_PLAYNUM       ERROR_INCOMPLETE_CMD    STR_DESCR_PLAYNUM       ERROR_INCOMPLETE_CMD2
#define ERROR_INC_PLAY          ERROR_INCOMPLETE_CMD    STR_DESCR_PLAY          ERROR_INCOMPLETE_CMD2
#define ERROR_INC_PROG          ERROR_INCOMPLETE_CMD    STR_DESCR_PROG          ERROR_INCOMPLETE_CMD2
#define ERROR_INC_TIMEOUT       ERROR_INCOMPLETE_CMD    STR_DESCR_TIMEOUT       ERROR_INCOMPLETE_CMD2

#define ERROR_INVALID_CMD "  <I: Invalid %s command \"%s\", "
#define ERROR_INVALID_CMD2 " is not correct: \"%s\">\n", cmd_sender
#define ERROR_INV_CCPU          ERROR_INVALID_CMD       STR_DESCR_CCPU          ERROR_INVALID_CMD2
#define ERROR_INV_TCPU          ERROR_INVALID_CMD       STR_DESCR_TCPU          ERROR_INVALID_CMD2
#define ERROR_INV_PLAYNUM       ERROR_INVALID_CMD       STR_DESCR_PLAYNUM       ERROR_INVALID_CMD2
#define ERROR_INV_PLAY          ERROR_INVALID_CMD       STR_DESCR_PLAY          ERROR_INVALID_CMD2
#define ERROR_INV_TIMEOUT       ERROR_INVALID_CMD       STR_DESCR_TIMEOUT       ERROR_INVALID_CMD2
#define ERROR_INV_FIRSTERR      ERROR_INVALID_CMD       STR_DESCR_FIRSTERR      ERROR_INVALID_CMD2

#define ERROR_MAX_PLAYNUM "  <I: %s: Numbers of players is more than MAXPLAYERS=%d: \"%s\">\n", cmd_sender
#define ERROR_MIN_PLAYNUM "  <I: %s: Numbers of players is less than one: \"%s\">\n", cmd_sender
#define ERROR_PLAY_DNE "  <I: Invalid %s command \"%s\": Player %d does not exist>\n", cmd_sender
#define ERROR_PLAY_AINI "  <I: Invalid %s command \"%s\": Player %d was already initialized>\n", cmd_sender
#define ERROR_UNKNOWN_CMD "  <I: %s sends unknown command: \"%s\"\n"
#endif

