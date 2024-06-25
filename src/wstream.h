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
$Log: wstream.h $
Revision 1.2  2009/09/14 10:32:07Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
use of windows.h wrapped by awwin.h so we can clean up warnings and packing


*/
#include "awwin.h"
#include "stream.h"

class Windows_File_Stream : public Stream
{
  private:
    HANDLE file;
    bool normal_file;
    size_t buffered;
    char buffer[4096];
  public:
    Windows_File_Stream(HANDLE file_,bool normal_file_ = true) :
      file(file_),
      normal_file(normal_file_),
      buffered(0)
    {}
    ~Windows_File_Stream()
    {
      flush_buffer();
      if (normal_file)
        close();
    }
    bool flush_buffer()
    {
      if (buffered)
      {
        DWORD written;
        bool retcode = WriteFile(file,buffer,buffered,&written,0) ? written==buffered : 0;
        buffered = 0;
        return retcode;
      }
      return true;
    }
    virtual bool is_open()
    {
      return file!=INVALID_HANDLE_VALUE;
    }
    bool flush()
    {
      bool ok = flush_buffer();
      return (!normal_file || FlushFileBuffers(file)) && ok;
    }
    bool close()
    {
      bool ok = flush_buffer();
      bool retcode = file!=INVALID_HANDLE_VALUE ? CloseHandle(file)!=0 && ok : true;
      file = INVALID_HANDLE_VALUE;
      return retcode;
    }
    void error(char **data)
    {
      *data = 0;
    }
    size_t write(const Byte * data,size_t nr_bytes)
    {
      if (normal_file)
      {
        size_t retcode = nr_bytes;
        for (;nr_bytes;)
        {
          size_t to_do = min(sizeof(buffer)-buffered,nr_bytes);
          memcpy(buffer+buffered,data,to_do);
          buffered += to_do;
          data += to_do;
          nr_bytes -= to_do;
          if (buffered == sizeof(buffer))
            if (!flush_buffer())
              return 0;
        }
        return retcode;
      }
      DWORD written;
      if (!WriteFile(file,data,nr_bytes,&written,0))
        written = 0;
      return written;
    }
    size_t read(Byte * data,size_t nr_bytes)
    {
      DWORD read;
      return ReadFile(file,data,nr_bytes,&read,0) ? read : 0;
    }
};
