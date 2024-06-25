/*
  Copyright 2008,2009,2010 Alun Williams
  This file is part of MAF.
  MAF is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  MAF is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with MAF.  If not, see <http://www.gnu.org/licenses/>.
*/


// $Log: stream.cpp $
// Revision 1.6  2010/06/10 13:57:50Z  Alun
// All tabs removed again
// Revision 1.5  2009/09/16 07:08:19Z  Alun
// Additional source code changes needed to get clean compilation on latest GNU
//
// Revision 1.4  2009/09/12 18:48:01Z  Alun
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// formatv() now handles c99 style format parameters
//
//
// Revision 1.3  2008/11/04 21:41:00Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/11/04 22:40:59Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.2  2007/09/07 20:20:16Z  Alun_Williams
//

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "awcc.h"
#include "stream.h"
#include "variadic.h"
#include "x_to_str.h"

#if _MSC_VER > 900
#define ecvt _ecvt
#endif

#define UARG u64
#define SARG i64
#define UL_TO_STR u64_to_str
#define L_TO_STR  i64_to_str

Output_Stream & operator<<(Output_Stream & os, const char * s)
{
  os.write(s,strlen(s));
  return os;
}

size_t Output_Stream::formatv(const char * control,Variadic_Arguments args)
{
  /* This is a function for doing generic printf() style output to a stream.
     Unlike some printf() type functions it does not need to buffer its
     output, so it should not be vulnerable to buffer overruns, even
     if it is given extremely long arguments.
     We don't provide support for floating point output since it is
     not usually needed, and attempting to include it causes problems
     with the CRT replacements on Windows
  */
#define MAXDIG 80
  char buffer[MAXDIG];
  int width,ndigit;
  int base = 0;
#ifdef NEED_FP_SUPPORT
  int dec,sign;
#endif
  int ljust;
  char zfill;
  char plus;
  int lflag;
  int pref;
  char *p,*s;
  char c;
  UARG u;
  SARG l;
  int i;
  size_t answer = 0;

  for (;;)
  {
    /* get control character */
    c = *control++;

    /* stop at end of string */
    if (c == 0)
      break;

    /* print characters other than '%' */
    if (c != '%')
    {
#ifdef WIN32
      if (c == '\n')
        put('\r');
      if (c != '\r')
        put (c);
#else
      put(c);
#endif
      answer++;
      continue;
    }

    pref = 0;
    plus = 0;
    ljust = 0;
    zfill = ' ';

    /* next control character */
    c = *control++;

    /* flags */
    for (;;)
    {
      if (c == '-')
        ljust = 1;
      else if (c == '+')
        plus = '+';
      else if (c == ' ')
      {
        if (plus==0)
          plus = ' ';
      }
      else if (c == '0')
        zfill = '0';
      else if (c == '#')
        pref = 1;
      else
        break;

      c = *control++;
    }

    /* width */
    width = 0;
    for (;;)
    {
      if (c >= '0' && c <= '9')
        width = width*10 + c - '0';
      else if (c == '*')
        width = next_argument(args,int);
      else
        break;

      c = *control++;
    }

    /* precision */
    if (c == '.')
    {
      c = *control++;
      ndigit = 0;
      for (;;)
      {
        if (c >= '0' && c <= '9')
          ndigit = ndigit*10 + c - '0';
        else if (c == '*')
          ndigit = next_argument(args,int);
        else
          break;
        c = *control++;
      }
    }
    else
      ndigit = INT_MAX;

    if (c == 'h')    /* 'h' means short */
    {
      c = *control++;
      lflag = 0;
      if (c == 'h')
        c = *control++;
    }
    else if (c == 'l')   /* 'l' means long */
    {
      lflag = 1;
      c = *control++;
      if (c == 'l')
      {
        lflag = 2;
        c = *control++;
      }
    }
    else if (c == 'z')   /* 'z' means size_t sized */
    {
      lflag = 3;
      c = *control++;
    }
    else if (c == 't')   /* 't' means ptrdiff_t sized */
    {
      lflag = 4;
      c = *control++;
    }
#ifdef HAVEI64
    else if (c == 'I'))
    {
      if (memcmp(control,"64",2) == 0)
      {
        lflag = 5;
        c = control[2];
        control += 3;
      }
      else if (memcmp(control,"32",2) == 0)
      {
        lflag = 6;
        c = control[2];
        control += 3;
      }
      else
        lflag = 7;
    }
#endif
    else if (c == 'j')
    {
      lflag = 8; /* should be intmax_t */
    }
    else if (c == 'L')
    {
      lflag = 9; /* long double */
    }
    else
      lflag = 0;

    /* get data into buffer */
    p = s = buffer;
    switch (c)
    {
      case 'X': /* unsigned */
      case 'x':
      case 'u':
      case 'o':
        switch (lflag)
        {
          default:
          case 6:
          case 0:
            u = next_argument(args,unsigned);
            break;
          case 1:
            u = next_argument(args,unsigned long);
            break;
          case 8:
          case 5:
          case 2:
            u = next_argument(args,UARG);
            break;
          case 7:
          case 3:
            u = next_argument(args,size_t);
            break;
          case 4:
            u = next_argument(args,ptrdiff_t);
            break;
        }

        switch (c)
        {
          case 'X':
          case 'x':
            if (u!=0 && pref!=0)
            {
              *p++ = '0';
              *p++ = c;
            }
            base = 16;
            break;
          case 'u':
            base = 10;
            break;
          case 'o':
            if (u!=0 && pref!=0)
              *p++ = '0';
            base = 8;
            break;
        }
        UL_TO_STR(p,u,base);
        p += strlen(p);
        break;
      case 'd':
      case 'i':
        switch (lflag)
        {
          case 6:
          case 0:
          default:
            l = next_argument(args,int);
            break;
          case 1:
            l = next_argument(args,long);
            break;
          case 8:
          case 5:
          case 2:
            l = next_argument(args,SARG);
            break;
          case 7:
          case 3:
            l = next_argument(args,size_t);
            break;
          case 4:
            l = next_argument(args,ptrdiff_t);
            break;
        }

        if (l>=0 && plus!=0)
          *p++ = plus;
        L_TO_STR(p,l,10);
        p += strlen(p);
        break;
      case 'p': /* pointer */
        L_TO_STR(p,(long)next_argument(args,void *),16);
        p += strlen(p);
        zfill = '0';
        width = sizeof(void *)*2;
        break;
#ifdef NEED_FP_SUPPORT
      case 'e': /* float */
      case 'E':
        if (ndigit > MAXDIG-8)
          ndigit = 6;

        if (lflag == 9)
          s = ecvt(next_argument(args,long double),ndigit+1,&dec,&sign);
        else
          s = ecvt(next_argument(args,double),ndigit+1,&dec,&sign);
        if (*s != '0')
          dec--;
        if (sign)
          *p++ = '-';
        else if (plus)
          *p++ = plus;
        *p++ = *s++;
        if (*s == 0)
        {
          if (pref)
            *p++ = '.';
        }
        else
        {
          *p++ = '.';
          while (*s)
            *p++ = *s++;
        }
        *p++ = c;
        if (dec<0)
        {
          *p++ = '-';
          dec = -dec;
        }
        else
          *p++ = '+';
        for (i = 2; i >= 0; i--)
        {
          p[i] = (char)((dec%10)+'0');
          dec /= 10;
        }
        p += 3;
        s = buffer;
        break;
#endif
#ifndef NEED_FP_SUPPORT
      case 'e':
      case 'E':
#endif
      case 'f':
      case 'g':
      case 'G':
        if (lflag == 9)
          next_argument(args,long double);
        else
          next_argument(args,double);
        zfill = ' ';
        *p++ = '?';
        break;
      case 'c': /* char */
        zfill = ' ';
        *p++ = (char)next_argument(args,int);
        break;
      case 's': /* string */
        zfill = ' ';
        if ((s = next_argument(args,char *)) == 0)
        {
          s = (char *) "(null)"; // This cast is OK because s is actually const
          p = s+strlen(s);
        }
        else
        {
          for (p = s; *p && --ndigit >= 0; p++)
            ;
        }
        break;
      default:
        *p++ = c;
        break;
    }

    /* print buffer with justification and padding */
    i = p - s;
    if ((width -= i) < 0)
      width = 0;
    if (ljust == 0)
      width = -width;
    if (width < 0)
    {
      if (*s == '-' && zfill == '0')
      {
        put(*s++);
        answer++;
        i--;
      }
      answer -= width;
      do
        put(zfill);
      while (++width != 0);
    }
    answer += i;
    while (--i >= 0)
    {
#ifdef WIN32
      char c = *s++;
      if (c == '\n')
        put('\r');
      if (c != '\r')
        put(c);
#else
      put(*s++);
#endif
    }
    answer += width;
    while (width)
    {
      put(zfill);
      width--;
    }
  }
  return answer;
}
