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

#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#define STR_CMD_MANAGER "-m"
#define STR_CMD_MANAGER_FILE "-f"
#define STR_CMD_DEBUG "-d"

extern char *cmd_manager;
extern char *cmd_manager_file;

typedef enum {
  CMD_UNKNOWN = 0,
  CMD_MANAGER,
  CMD_MANAGER_FILE,
  CMD_DEBUG
} commandflag;

void commandline(int argc, char *argv[]);
#endif

