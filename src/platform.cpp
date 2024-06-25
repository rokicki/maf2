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


// $Log: platform.cpp $
// Revision 1.9  2010/06/10 14:26:38Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.8  2010/06/10 13:57:46Z  Alun
// All tabs removed again
// Revision 1.7  2010/05/09 11:11:55Z  Alun
// delete_file() method added
// Revision 1.6  2009/09/12 18:47:57Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2009/08/24 11:36:51Z  Alun
// Don't let stdin be closed
// Revision 1.4  2008/11/02 15:01:08Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/11/02 16:01:08Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/08/02 18:24:06Z  Alun
// "Aug 2008 snapshot"
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "awcc.h"
#ifdef WIN32
#include "wstream.h"
#else
#include <errno.h>
#include "fstream.h"
#endif
#include "platform.h"
#include "variadic.h"

class Platform_Implementation : public Platform
{
    Input_Stream * stdin_stream;
    Output_Stream * stdout_stream;
    Output_Stream * stderr_stream;
    Stream_Mangler log_stream; // don't move this above stdout_stream!
    Stream_Mangler error_stream;  // or this above stderr_stream
    clock_t status_time;
    unsigned long reference_count;
  public:
    Platform_Implementation() :
#ifdef WIN32
      stdin_stream(new Windows_File_Stream(GetStdHandle(STD_INPUT_HANDLE),false)),
      stdout_stream(new Windows_File_Stream(GetStdHandle(STD_OUTPUT_HANDLE),false)),
      stderr_stream(new Windows_File_Stream(GetStdHandle(STD_ERROR_HANDLE),false)),
#else
      stdin_stream(new CRT_File_Stream(stdin,false)),
      stdout_stream(new CRT_File_Stream(stdout,false)),
      stderr_stream(new CRT_File_Stream(stderr,false)),
#endif
      log_stream(*stdout_stream),
      error_stream(*stderr_stream),
      reference_count(0)
    {
      status_time = clock();
    }
    Platform &attach()
    {
      ++reference_count;
      return *this;
    }
    void detach()
    {
      if (--reference_count == 0)
        delete this;
    }
    ~Platform_Implementation()
    {
      delete stdin_stream;
      delete stdout_stream;
      delete stderr_stream;
    }
    size_t output(Output_Stream * stream,const char * control,Variadic_Arguments &args)
    {
      size_t retcode = stream->formatv(control,args);
      return retcode;
    }
    size_t log_output(const char * control,Variadic_Arguments &args)
    {
      size_t retcode = log_stream.formatv(control,args);
      status_time = clock();
      return retcode;
    }
    Output_Stream * get_stdout_stream()
    {
      return stdout_stream;
    }
    Output_Stream * get_log_stream()
    {
      return &log_stream;
    }
    Output_Stream * get_stderr_stream()
    {
      return &error_stream;
    }
    Input_Stream * get_stdin_stream()
    {
      return stdin_stream;
    }
    Output_Stream * open_text_output_file(String filename,bool null_is_stdout)
    {
      if (filename)
      {
#ifdef WIN32
        HANDLE output_file = CreateFile(filename,GENERIC_WRITE|GENERIC_READ,
                                        0,0,CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL|
                                        FILE_FLAG_SEQUENTIAL_SCAN,0);
        return output_file != INVALID_HANDLE_VALUE ? new Windows_File_Stream(output_file) : 0;
#else
        FILE * output_file = fopen(filename,"w");
        return output_file ? new CRT_File_Stream(output_file) : 0;
#endif
      }
      if (null_is_stdout)
        return stdout_stream;
      return 0;
    }
    Output_Stream * open_binary_output_file(String filename)
    {
#ifdef WIN32
      HANDLE output_file = CreateFile(filename,GENERIC_WRITE|GENERIC_READ,
                                      0,0,CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL|
                                      FILE_FLAG_SEQUENTIAL_SCAN,0);
      return output_file != INVALID_HANDLE_VALUE ? new Windows_File_Stream(output_file) : 0;
#else
      FILE * output_file = fopen(filename,"wb");
      return output_file ? new CRT_File_Stream(output_file) : 0;
#endif
    }
    bool close_output_file(Output_Stream * stream)
    {
      bool retcode = true;
      if (stream != stdout_stream)
      {
        retcode = stream->close();
        delete stream;
      }
      return retcode;
    }
    bool delete_file(String filename)
    {
#ifdef WIN32
      return DeleteFile(filename)!=0;
#else
      return remove(filename)==0;
#endif
    }
    Input_Stream * open_input_file(String filename,bool null_is_stdin)
    {
      if (filename)
      {
#ifdef WIN32
        HANDLE input_file = CreateFile(filename,GENERIC_READ,
                                        FILE_SHARE_READ,0,OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL|
                                        FILE_FLAG_SEQUENTIAL_SCAN,0);
        return input_file != INVALID_HANDLE_VALUE ? new Windows_File_Stream(input_file) : 0;
#else
        FILE * input_file = fopen(filename,"rb");
        return input_file ? new CRT_File_Stream(input_file) : 0;
#endif
      }
      if (null_is_stdin)
        return stdin_stream;
      return 0;
    }

    size_t read(Input_Stream * input_stream,Byte * buffer,size_t buf_size)
    {
      return input_stream->read(buffer,buf_size);
    }
    void close_input_file(Input_Stream * stream)
    {
      if (stream != stdin_stream)
      {
        stream->close();
        delete stream;
      }
    }
    void set_gap_stdout(bool on = true)
    {
      log_stream.watch(on);
      error_stream.watch(on);
    }
    bool status_needed(int gap)
    {
      return clock() >= status_time+gap*CLOCKS_PER_SEC;
    }
    String last_error_message(String_Buffer * buffer)
    {
#ifdef WIN32
      char * temp;
      DWORD len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
                                FORMAT_MESSAGE_FROM_SYSTEM|
                                FORMAT_MESSAGE_IGNORE_INSERTS,0,
                                GetLastError(),0,(LPTSTR) &temp,0,0);
      if (len)
      {
        Letter *ptr = buffer->reserve(len);
        strcpy(ptr,temp);
        LocalFree(temp);
      }
      else
        buffer->set("Unknown Error\n");
      return buffer->get();
#else
      Letter * message = strerror(errno);
      return buffer->set(message);
#endif
    }
};

Platform * Platform::create_platform()
{
  return new Platform_Implementation();
}
