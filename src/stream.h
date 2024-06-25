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
$Log: stream.h $
Revision 1.2  2009/09/14 10:32:07Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


*/
#pragma once
#ifndef STREAM_INCLUDED
#define STREAM_INCLUJDED 1
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

typedef unsigned char Byte;

class Stream_Base
{
  public:
    virtual bool is_open() = 0;
    virtual bool flush() = 0;
    virtual bool close() = 0;
    virtual void error(char **data) = 0;
    virtual ~Stream_Base() {};
};

class Output_Stream : virtual public Stream_Base
{
  public:
    virtual size_t write(const Byte * data,size_t nr_bytes) = 0;
    size_t write(const char * data,size_t nr_bytes)
    {
      return write((const Byte *) data,nr_bytes);
    }
    size_t formatv(const char * control,Variadic_Arguments args);
    virtual size_t put(char c)
    {
      return write((const Byte *)&c,1);
    }
};

class Stream_Mangler : public Output_Stream
{
  public:
    Output_Stream & final_stream;
    bool watching;
    bool hash_pending;
    Stream_Mangler(Output_Stream & final_stream_) :
      final_stream(final_stream_),
      watching(false),
      hash_pending(false)
    {}
    bool is_open()
    {
      return final_stream.is_open();
    }
    virtual bool flush()
    {
      return final_stream.flush();
    }
    virtual bool close()
    {
      return final_stream.close();
    }
    void error(char **data)
    {
      final_stream.error(data);
    }
    virtual size_t write(const Byte * data,size_t nr_bytes)
    {
      if (watching)
      {
        size_t answer = 0;
        for (size_t i = 0; i < nr_bytes;i++)
          answer += put(data[i]);
        return answer;
      }
      else
        return final_stream.write(data,nr_bytes);
    }

    virtual size_t put(char c)
    {
      size_t extra = 0;
      if (!watching)
        return final_stream.put(c);
      if (c == '\n')
        hash_pending = true;
      else if (hash_pending)
      {
        extra = final_stream.put('#');
        hash_pending = false;
      }
      return extra + final_stream.put(c);
    }
    void watch(bool watching_)
    {
      watching = watching_;
      hash_pending = watching;
    }
};

class Input_Stream : public virtual Stream_Base
{
  public:
    virtual size_t read(Byte * data,size_t nr_bytes) = 0;
    size_t read(char * data,size_t nr_bytes)
    {
      return read((Byte *)data,nr_bytes);
    }
};

class Stream : virtual public Input_Stream, virtual public Output_Stream
{
};

#endif

