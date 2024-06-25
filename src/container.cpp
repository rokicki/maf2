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


// $Log: container.cpp $
// Revision 1.9  2010/05/09 11:11:53Z  Alun
// Added delete_file() method
// Revision 1.8  2009/10/12 21:01:52Z  Alun
// Added option for input_error() not to be fatal, so that reduce() and similar
// utilties can be more user-friendly when used interactively
// Revision 1.7  2009/09/12 18:47:11Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.6  2008/12/27 08:41:52Z  Alun
// usage_error() method added. Heap stats use unsigned variables
// Revision 1.5  2008/11/02 20:57:26Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.4  2008/08/02 18:18:32Z  Alun
// "Aug 2008 snapshot"
// Revision 1.3  2007/12/20 23:25:41Z  Alun
//

#include <stdlib.h>
#include "awcc.h"
#include "variadic.h"
#include "container.h"
#include "platform.h"
#include "heap.h"

Container::Container(Platform & platform_) :
  platform(platform_.attach()),
  stdin_stream(platform.get_stdin_stream()),
  stdout_stream(platform.get_stdout_stream()),
  stderr_stream(platform.get_stderr_stream()),
  log_stream(platform.get_log_stream()),
  log_level(2),
  interactive(false)
{}

Container::~Container()
{
  platform.detach();
}

void Container::close_input_file(Input_Stream * stream)
{
  platform.close_input_file(stream);
}

bool Container::close_output_file(Output_Stream * stream)
{
  if (!platform.close_output_file(stream))
  {
    String_Buffer buffer;
    error_output("Error writing to file: %s\n",
                 platform.last_error_message(&buffer).string());
    return false;
  }
  return true;
}

void Container::error_output(const char * control,Variadic_Arguments &args)
{
  platform.output(stderr_stream,control,args);
}

void Container::error_output(const char * control,...)
{
  DECLARE_VA(va,control);
  platform.output(stderr_stream,control,va);
}

void Container::input_error(const char * control,...)
{
  {
    DECLARE_VA(va,control);
    platform.output(stderr_stream,control,va);
  }
  if (!interactive)
  {
    Heap::prevent_leak_dump();
    exit(1); /* Yes that's it. No exception handling, and no clean-up
                MAF does not expect this function to return, and if it
                is changed so that it does, a lot of source code will need
                to be reviewed, and probably changed. */
  }
}

void Container::usage_error(const char * control,...)
{
  /* This is the same as the previous method, but it seemed like a good
     idea to have it as a separate method in case we want to distinguish
     usage errors from errors caused by syntax errors in the input file.
  */
  {
    DECLARE_VA(va,control);
    platform.output(stderr_stream,control,va);
  }
  Heap::prevent_leak_dump();
  exit(1); // As above
}

void Container::io_error(bool fatal,const char * control,...)
{
  {
    DECLARE_VA(va,control);
    platform.output(stderr_stream,control,va);
  }
  if (fatal)
  {
    Heap::prevent_leak_dump();
    exit(3); // As above
  }
}

String Container::last_error_message(String_Buffer * buffer)
{
  return platform.last_error_message(buffer);
}

/**/

Output_Stream * Container::open_binary_output_file(String filename)
{
  Output_Stream *retcode = platform.open_binary_output_file(filename);
  if (!retcode)
  {
    String_Buffer buffer;
    error_output("Error opening file %s for writing: %s\n",filename.string(),
                 platform.last_error_message(&buffer).string());
    return 0;
  }
  return retcode;
}

/**/

Input_Stream * Container::open_input_file(String filename,unsigned flags)
{
  Input_Stream * answer;
  answer = platform.open_input_file(filename,(flags & OIF_NULL_IS_STDIN)!=0);
  if (!answer)
  {
    String_Buffer buffer;
    if (flags & OIF_REPORT_ERROR+OIF_MUST_SUCCEED)
      io_error((flags & OIF_MUST_SUCCEED)!=0,
               "Unable to open file %s: %s\n",filename.string(),
               platform.last_error_message(&buffer).string());
  }
  return answer;
}

/**/

Output_Stream * Container::open_text_output_file(String filename,bool null_is_stdout)
{
  Output_Stream *retcode = platform.open_text_output_file(filename,null_is_stdout);
  if (!retcode)
  {
    String_Buffer sb;
    io_error(true,"Error opening file %s for writing: %s\n",filename.string(),
             platform.last_error_message(&sb).string());
    return 0;
  }
  return retcode;
}

/**/

bool Container::delete_file(String filename)
{
  return platform.delete_file(filename);
}

/**/

size_t Container::output(Output_Stream * stream,const char * control,
                        Variadic_Arguments & args)
{
  return platform.output(stream,control,args);
}

/**/

size_t Container::output(Output_Stream * stream,const char * control,...)
{
  DECLARE_VA(va,control);
  size_t retcode = platform.output(stream,control,va);
  return retcode;
}

/**/

bool Container::progress(unsigned level,const char * control,...)
{
  DECLARE_VA(va,control);
  return vprogress(level,control,va);
}

/**/

bool Container::vprogress(unsigned level,const char * control,Variadic_Arguments &args)
{
  if (level <= log_level)
  {
    platform.log_output(control,args);
    return true;
  }
  return false;
}

void Container::result(const char * control,...)
{
  DECLARE_VA(va,control);
  vresult(control,va);
}

/**/

void Container::vresult(const char * control,Variadic_Arguments &args)
{
  platform.output(get_stdout_stream(),control,args);
}

/**/

size_t Container::read(Input_Stream *stream,Byte * buffer,size_t buf_size)
{
  return platform.read(stream,buffer,buf_size);
}

/**/

void Container::set_gap_stdout(bool on)
{
  platform.set_gap_stdout(on);
}

void Container::set_interactive(bool on)
{
  interactive = on;
}

/**/

bool Container::status(unsigned level,int gap,const char * control,...)
{
  DECLARE_VA(va,control);
  return status(level,gap,control,va);
}

bool Container::status(unsigned level,int gap,const char * control,Variadic_Arguments &args)
{
  if (platform.status_needed(gap))
  {
    if (log_level >= 2)
    {
      const Heap_Status * hs = Heap::get_global_heap()->status(true);
      if (hs && hs->os_allocation >= 100)
        progress(2,"Heap use so far: In use %zu Reserved %zu, Utilisation %d%%\n",
                 hs->total_allocation,hs->os_allocation,
                 int(hs->total_allocation/(hs->os_allocation/100)));
    }
    if (level <= log_level)
      platform.log_output(control,args);
    else
      platform.log_output("",args);
    return true;
  }
  return false;
}

bool Container::status_needed(int gap)
{
  return platform.status_needed(gap);
}

Container * Container::create(Platform *platform)
{
  if (!platform)
    platform = Platform::create_platform();
  return new Container(*platform);
}

void Container::die()
{
  Heap::prevent_leak_dump();
  exit(2);
}
