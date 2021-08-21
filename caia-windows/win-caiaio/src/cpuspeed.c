/*
Copyright (C) 2008 Maks Verver
Copyright (C) 2007 Thijs Marinussen
Copyright (C) 2005 Jaap Taal and Marcel Vlastuin.

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

/*
getTSC saves the current value of the time stamp counter in *counter
*/

#if __i386

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <synchapi.h>

inline void getTSC(unsigned long long *counter)
{
  __asm__("rdtsc\n"
          "movl %%eax, (%%edi)\n"
          "movl %%edx, 4(%%edi)\n"
          : : "D" (counter) : "eax","edx");
}

int cpuspeed(void)
{
  unsigned long long start,stop,clocks;
  struct timeval ts;
  ts.tv_sec = 0;
  ts.tv_usec = 100000; //Sleep 100.000 usec = 0.1 sec
  getTSC(&start);
  //select(0,NULL,NULL,NULL,&ts);
  Sleep(100);
  getTSC(&stop);
  clocks = stop-start; //Clocks now contains the number of clockticks in 0.1 second
  //The number of clockticks divided by 100.000 is the speed of the processor in Mhz
  //fprintf(stderr,"clocks=%llu\n",clocks);fflush(stderr);
  return clocks / 100000;
}

#elif __x86_64

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <synchapi.h>

static inline void getTSC(unsigned long long *counter)
{
  __asm__("rdtsc\n"
          "movl %%eax, (%%rdi)\n"
          "movl %%edx, 4(%%rdi)\n"
          : : "D" (counter) : "rax","rdx");
}

int cpuspeed(void)
{
  unsigned long long start,stop,clocks;
  //struct timeval ts;
  //ts.tv_sec = 0;
  //ts.tv_usec = 100000; //Sleep 100.000 usec = 0.1 sec
  getTSC(&start);
  //select(0,NULL,NULL,NULL,&ts);
  Sleep(100);
  getTSC(&stop);
  clocks = stop-start; //Clocks now contains the number of clockticks in 0.1 second
  //The number of clockticks divided by 100.000 is the speed of the processor in Mhz
  //fprintf(stderr,"clocks=%llu\n",clocks);fflush(stderr);
  return clocks / 100000;
}

#else

#include <stdio.h>
#include <sys/time.h>

int cpuspeed(void)
{
  long dif, count;
  volatile double dummy=1.0;
  struct timeval start, stop;
  gettimeofday(&start, NULL);
  for (count = 0; count < 50000000; ++count) dummy/=2;
  gettimeofday(&stop, NULL);
  dif = 1000000 * (stop.tv_sec - start.tv_sec) + stop.tv_usec - start.tv_usec;
  return int(266.0*5210000.0/float(dif))+(int)dummy;
}

#endif




