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


#include <stdio.h>
#include "stream.h"

class CRT_File_Stream : public Stream
{
  FILE * file;
  bool close_on_delete;
  public:
    CRT_File_Stream(FILE * file_,bool close_on_delete_ = true) :
      file(file_),
      close_on_delete(close_on_delete_)
    {}
    ~CRT_File_Stream()
    {
      if (close_on_delete)
        close();
    }
    virtual bool is_open()
    {
      return file!=0;
    }
    bool flush()
    {
      return fflush(file)==0;
    }
    bool close()
    {
      bool retcode = file ? fclose(file)==0 : true;
      file = 0;
      return retcode;
    }
    void error(char **data)
    {
      *data = 0;
    }
    size_t write(const Byte * data,size_t nr_bytes)
    {
      return fwrite(data,1,nr_bytes,file);
    }
    size_t read(Byte * data,size_t nr_bytes)
    {
      return fread(data,1,nr_bytes,file);
    }
};
