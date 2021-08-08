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

#include <stdlib.h>
#include <string.h>
#include "manager.h"
#include "commandline.h"
#include "prflush.h"
#include "debug.h"

char *cmd_manager = NULL;
char *cmd_manager_file = NULL;

commandflag command_parse_token(const char * const word)
{
  commandflag r;
  if (!strcmp(word, STR_CMD_MANAGER)) r = CMD_MANAGER;
  else if (!strcmp(word, STR_CMD_MANAGER_FILE)) r = CMD_MANAGER_FILE;
  else if (!strcmp(word, STR_CMD_DEBUG)) r = CMD_DEBUG;
  else r = CMD_UNKNOWN;
  return r;
}

void commandline(int argc, char *argv[])
{
  int i;
  commandflag cmd;
  for (i = 1; i < argc; i++)
  {
    cmd = command_parse_token(argv[i]);
    switch (cmd)
    {
      case CMD_MANAGER:
        if (cmd_manager != NULL)
        {
          //delete cmd_manager;
		  free(cmd_manager);
		  cmd_manager = NULL;
        }
        //cmd_manager = new char[strlen(argv[++i])+1];
		cmd_manager = malloc((strlen(argv[++i])+1) * sizeof(*cmd_manager));
		if (!cmd_manager) {
			exit(1);
		}
        strcpy(cmd_manager, argv[i]);
//      debug("** Commando voor de manager: %s\n", argv[i]);
        break;
      case CMD_MANAGER_FILE:
        if (cmd_manager_file != NULL)
        {
          //delete cmd_manager_file;
		  free(cmd_manager_file);
		  cmd_manager_file = NULL;
        }
        //cmd_manager_file = new char[strlen(argv[++i])+1];
		cmd_manager_file = malloc((strlen(argv[++i])+1) * sizeof(*cmd_manager_file));
		if (!cmd_manager_file) {
			exit(1);
		}
        strcpy(cmd_manager_file, argv[i]);
//      debug("** Instructie bestand voor de manager: %s\n", argv[i]);
        break;
      case CMD_DEBUG:
        debug_flag = 1;
        debug(DEBUG_ON);
        break;
      default:
        fprflush(stderr, "Non-existing option '%s' is used\n", argv[i]);
        exit(1);
    }
  }
}

