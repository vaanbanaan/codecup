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

#ifndef STATE_H
#define STATE_H

#define STR_CAIAIO_STOP                 "stop_caiaio"
#define STR_CAIAIO_LOCK                 "lock"
#define STR_CAIAIO_UNLOCK               "unlock"
#define STR_CAIAIO_CPU                  "cpu_speed"

#define STR_REFEREE_START               "referee"
#define STR_REFEREE_KILL                "kill_referee"

#define STR_PLAYER_NUMBER               "number_players"
#define STR_PLAYER_INIT                 "player"
#define STR_PLAYER_START                "start"
#define STR_PLAYER_KILL                 "kill"
#define STR_PLAYER_REQUEST_TIME         "request_time"
#define STR_PLAYER_LISTEN               "listen"
#define STR_FIRSTERROR                  "firsterror" 
#define STR_NO_FIRSTERROR               "no_firsterror"

#define RESP_LOCK_OK                    "lock_ok"

typedef enum {
  STATE_NONE = 0,
  CAIAIO_STOP,
  CAIAIO_ERROR,
  CAIAIO_LOCK,
  CAIAIO_UNLOCK,
  CAIAIO_CPU,
  REFEREE_START,
  REFEREE_KILL,
  PLAYER_NUMBER,
  PLAYER_INIT,
  PLAYER_START,
  PLAYER_KILL,
  PLAYER_REQUEST_TIME,
  PLAYER_LISTEN
} io_state;

io_state state_parse_token(const char * const line);
int readint(char ** const linep, char * const word, int * const integer);
int readword(char ** const linep, char *word);

#endif

