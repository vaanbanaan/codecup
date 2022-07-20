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
#include <stdarg.h>
#include "prflush.h"

int debug_flag = 0;

int prflush(const char *fmt, ...)
{
  int r;
  va_list ap;
  va_start(ap, fmt);
  r = vprintf(fmt, ap);
  va_end(ap);
  fflush(stdout);
  return r;
}

int fprflush(FILE * const stream, const char *fmt, ...)
{
  int r;
  va_list ap;
  va_start(ap, fmt);
  r = vfprintf(stream, fmt, ap);
  va_end(ap);
  fflush(stream);
  return r;
}

int debug(const char *fmt, ...)
{
  if (debug_flag) {
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    return r;
  }
  return 0;
}

