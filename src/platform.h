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
$Log: platform.h $
Revision 1.5  2010/05/09 11:11:53Z  Alun
delete_file() method added
Revision 1.4  2009/09/12 18:48:43Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.3  2008/08/02 18:20:58Z  Alun
"Aug 2008 snapshot"
Revision 1.2  2007/12/20 23:25:45Z  Alun
*/
#pragma once
#ifndef PLATFORM_INCLUDED
#define PLATFORM_INCLUDED 1

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/*
There are no global variables at all in MAF apart from a few constants.
This is because a single program should be able to create multiple MAF objects
at the same time and manage them completely independently.
The Platform interface supplies the functionality that most programs would
use Ansi C or C++ functions for. Most of MAF does not explicitly
rely on the C or C++ runtime I/O calls at all and anything that normally
would be implemented via a call to CRT I/O is replaced with a call to a
method in a Platform object instead. The Platform interface is wrapped by
the Container class, in order to reduce the number of methods that Platform
needs to implement, and in fact only Container uses Platform directly.

Platform.cpp provides a simple implementation of this interface suitable for
text based (console) programs based on the Ansi C 89 CRT. It should be a
fairly simple matter to replace this with an implementation using the run
time libary of your choice. You can create a container object by
calling MAF::create_container(), or Container::create()).
If you don't pass specify a platform object for it to use it will create one
using its default implementation.
*/

class Platform
{
  protected:
    virtual ~Platform() {};
  public:
    static Platform * create_platform();
    // A Container will call attach() once, when it is created.
    // The Platform object must not allow itself to be destroyed while
    // a container is attached to it. attach() must return *this;
    virtual Platform &attach() = 0;
    // A Container will call detach() once, when it is being destroyed.
    // The Platform object should delete any objects it created, including
    // the handles returned by get_stdout_stream() etc, if need be. More
    // than one container might attach to the same platform object, but this
    // never happens at the moment.
    virtual void detach() = 0;
    /* The platform object must provide the following handles.
       MAF will save the returned values in its container object,
       so the values should not change. The log_stream is used
       for all the status/progress messages MAF produces while it
       is trying to build the automata for an object.  */
    virtual Output_Stream * get_log_stream() = 0;
    virtual Output_Stream * get_stderr_stream() = 0;
    virtual Input_Stream * get_stdin_stream() = 0;
    virtual Output_Stream * get_stdout_stream() = 0;
    virtual bool close_output_file(Output_Stream * stream) = 0;
    virtual void close_input_file(Input_Stream * stream) = 0;
    virtual bool delete_file(String filename) = 0;
    // return value of last_error_message() should be buffer->get()
    // it is used to get suitable text for an error message after an I/O error.
    virtual String last_error_message(String_Buffer * buffer) = 0;
    virtual size_t log_output(const char * control,
                              Variadic_Arguments &args)
    {
      /* MAF will always almost always call this method to output to the log
         stream. You can over-ride it if convenient to do so as a means of
         handling log output appropriately. However, there are a few occasions
         where a handle to the container log_stream is passed directly to
         output() */
      return output(get_log_stream(),control,args);
    }
    virtual size_t output(Output_Stream * stream,const char * control,
                          Variadic_Arguments &args) = 0;
    /*
       All file open functions should return 0 on failure. MAF will
       then call last_error_message() to ask for any error text that should
       appear on the stdout_stream, and will then exit */
    /* Output files should always delete any existing file, and write the file
       sequentially from  byte 0. \n->\r\n translation should be performed if
       the file is opened with open_text_output_file() and translation is usual
       for the platform. No translations should be performed on files opened in
       binary mode. open_text_output_file expects to be able to pass 0 for
       filename and get a duplicate handle to the "stdout" stream. Also when
       closing such a handle the real stdout must not be closed.*/
    virtual Output_Stream * open_binary_output_file(String filename) = 0;
    virtual Output_Stream * open_text_output_file(String filename, bool null_is_stdout = true) = 0;
    /* Files opened for input should always be opened in "binary" mode.
       i.e. they should not perform any conversions. MAF can understand
       both Unix and Dos/Windows style files. */
    virtual Input_Stream * open_input_file(String filename,bool null_is_stdin) = 0;
    /* MAF will always attempt to read files in 4K chunks.
       MAF is not interactive, so doesn't usually read from stdin */
    virtual size_t read(Input_Stream *,Byte * buffer,size_t buf_size) = 0;
    /* set_gap_stdout() instructs the platform interface to ensure that
       any output sent to the "log stream" has a # at the beginning of
       each line, because stdout is going to be used as the destination
       of a GAP/KBMAG format record. This is used by the fsa* applications
       when they are sending their output to stdout to stop progress
       output from invaliding the output file. */
    virtual void set_gap_stdout(bool on = true) = 0;
    /* status_needed() should return true if more than the specified time
       (in seconds) has elapsed. MAF depends on this to monitor its own
       progress, so do not return false to suppress status output.
       Instead make log_output() do nothing */
    virtual bool status_needed(int /*gap*/) { return true;};
};

#endif
