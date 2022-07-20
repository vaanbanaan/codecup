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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"

// this function needs a word buffer to store an int-string temporarily
int readint(char ** const linep, char * const word, int * const integer)
{
  char *endptr;
  int k;
  if (sscanf(*linep, "%s", word) <= 0)
  {
    word[0] = '\0';
    return 1;
  }
  *linep += strlen(word);
  if (**linep == ' ' || **linep == '\t') ++*linep;
  k = strtol(word, &endptr, 10);
  if (word == endptr) return 1;
  *integer = k;
  return 0;
}

int readword(char ** const linep, char *word)
{
  if (sscanf(*linep, "%s", word) <= 0)
  {
    word[0] = '\0';
    return 1;
  }
  *linep += strlen(word);
  if (**linep == ' ' || **linep == '\t') ++*linep;
  return 0;
}

io_state state_parse_token(const char * const word)
{
  io_state r;
//  fprintf(stderr, "manager_init_parse: |%s|\n", word); fflush(stderr);
  if (!strcmp(word, STR_CAIAIO_STOP)) r = CAIAIO_STOP;
  else if (!strcmp(word, STR_CAIAIO_LOCK)) r = CAIAIO_LOCK; 
  else if (!strcmp(word, STR_CAIAIO_UNLOCK)) r = CAIAIO_UNLOCK;
  else if (!strcmp(word, STR_CAIAIO_CPU)) r = CAIAIO_CPU;
  else if (!strcmp(word, STR_REFEREE_START)) r = REFEREE_START;
  else if (!strcmp(word, STR_REFEREE_START)) r = REFEREE_START;
  else if (!strcmp(word, STR_REFEREE_KILL)) r = REFEREE_KILL;
  else if (!strcmp(word, STR_PLAYER_NUMBER)) r = PLAYER_NUMBER;
  else if (!strcmp(word, STR_PLAYER_INIT)) r = PLAYER_INIT;
  else if (!strcmp(word, STR_PLAYER_START)) r = PLAYER_START;
  else if (!strcmp(word, STR_PLAYER_KILL)) r = PLAYER_KILL;
  else if (!strcmp(word, STR_PLAYER_REQUEST_TIME)) r = PLAYER_REQUEST_TIME;
  else if (!strcmp(word, STR_PLAYER_LISTEN)) r = PLAYER_LISTEN;
  else r = STATE_NONE;
  return r;
}

