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


/*
$Log: sstream.h $
Revision 1.2  2007/10/24 21:15:32Z  Alun
*/
#pragma once
#ifndef SSTREAM_INCLUDED
#define SSTREAM_INCLUDED 1

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
#ifndef STREAM_INCLUDED
#include "stream.h"
#endif

class String_Stream : public Stream
{
  Letter *s;
  public:
    String_Stream(Letter * s_) :
      s(s_)
    {}
    ~String_Stream()
    {
      *s = 0;
    }
    virtual bool is_open()
    {
      return true;
    }
    bool flush()
    {
      return true;
    }
    bool close()
    {
      *s = 0;
      return true;
    }
    void error(char **data)
    {
      *data = 0;
    }
    size_t write(const Byte * data,size_t nr_bytes)
    {
      for (size_t i = 0; i < nr_bytes;i++)
        *s++ = *data++;
      return nr_bytes;
    }
    size_t read(Byte * data,size_t nr_bytes)
    {
      size_t i;
      for (i = 0; i < nr_bytes;i++)
        if (*s)
          *data++ = *s++;
        else
          break;
      return i;
    }
};

class String_Buffer_Stream : public Stream
{
  String_Buffer *sb;
  size_t position;
  public:
    String_Buffer_Stream(String_Buffer * sb_) :
      sb(sb_),
      position(0)
    {}
    ~String_Buffer_Stream()
    {
    }
    virtual bool is_open()
    {
      return true;
    }
    bool flush()
    {
      return true;
    }
    bool close()
    {
      return true;
    }
    void error(char **data)
    {
      *data = 0;
    }
    size_t write(const Byte * data,size_t nr_bytes);
    size_t read(Byte * data,size_t nr_bytes);
};
#endif
