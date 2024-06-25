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
$Log: container.h $
Revision 1.7  2010/05/09 11:11:54Z  Alun
Changes to allow recovery from parse errors in some situations.
delete_file() method added
Revision 1.6  2009/10/12 20:55:19Z  Alun
Added support for making input errors recoverable
Revision 1.5  2009/09/13 11:00:57Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.4  2008/12/27 08:38:24Z  Alun
usage_error() method added
Revision 1.3  2008/08/02 19:12:34Z  Alun
"Aug 2008 snapshot"
Revision 1.2  2007/12/20 23:25:45Z  Alun
*/

/**
In MAF all interaction with the I/O (and other) facilities of the operating
system is done through a hierarachy of two classes Container, and Platform.
Much of MAF is aware of the Container class, and indeed most objects have to
live in one. "Container" is somewhat similar to "system" in a Java or C# program
or "theApp" in an MFC program. However there is no requirement for the
container to be unique. It is possible, though usually unnecessary, to create
many containers.

The implementation of Container relies on the Platform class. Only Container
uses Platform directly.

Almost all the non-trivial methods of Container, and all the methods of
Platform, are virtual, so in principle it should be possible to pass a
handle to a different implementation of these classes, and nothing would
be upset.

On the whole there is no reason why there should be any need to change
anything in Container. Although it is written with a console style interface
in mind most of what is needed to support a GUI interface instead can be
accomplished by providing a different implementation of platform.

See platform.h for further details.
**/

#pragma once
#ifndef CONTAINER_INCLUDED
#define CONTAINER_INCLUDED

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif

//Classes referred to but defined elsewhere
class Platform;
class Input_Stream;
class Output_Stream;
class Heap;

// flags for open_input_file()
const unsigned OIF_MUST_SUCCEED = 1;
const unsigned OIF_REPORT_ERROR = 2;
const unsigned OIF_NULL_IS_STDIN = 4;

/* I've made this a separate base class to make it easier to provide
   more information about syntax errors in input files */
class Parse_Error_Handler
{
  public:
    virtual void input_error(const char * control,...) __attribute__((format(printf,2,3))) = 0;
};

class Container : public Parse_Error_Handler
{
    BLOCKED(Container)
    Platform &platform;
    Input_Stream * stdin_stream;
    Output_Stream * stdout_stream;
    Output_Stream * stderr_stream;
    Output_Stream * log_stream;
    unsigned log_level;
    bool interactive;
    Container(Platform & platform_);
  public:
    virtual ~Container();
    static Container *create(Platform * platform = 0);
    virtual void close_input_file(Input_Stream * stream);
    virtual bool close_output_file(Output_Stream * stream);
    virtual bool delete_file(String filename);
    MS_ATTRIBUTE(__declspec(noreturn)) virtual void die() __attribute__((noreturn));
    virtual void error_output(const char * control,...) __attribute__((format(printf,2,3)));
    virtual void error_output(const char * control,Variadic_Arguments &args);
    virtual void input_error(const char * control,...) __attribute__((format(printf,2,3)));
    MS_ATTRIBUTE(__declspec(noreturn)) virtual void usage_error(const char * control,...) __attribute__((format(printf,2,3))) __attribute__((noreturn));
    virtual void io_error(bool fatal,const char * control,...) __attribute__((format(printf,3,4)));
    virtual String last_error_message(String_Buffer * buffer);
    virtual Output_Stream * open_binary_output_file(String filename);
    virtual Input_Stream * open_input_file(String filename,
                                           unsigned flags = OIF_MUST_SUCCEED|
                                                            OIF_REPORT_ERROR|
                                                           OIF_NULL_IS_STDIN);
    virtual Output_Stream * open_text_output_file(String filename,
                                                  bool null_is_stdout = true);
    virtual size_t output(Output_Stream * stream,const char * control,...) __attribute__((format(printf,3,4)));
    virtual size_t output(Output_Stream * stream,const char * control,
                  Variadic_Arguments & args);
    virtual bool progress(unsigned level,const char * control,...) __attribute__((format(printf,3,4)));
    virtual bool vprogress(unsigned level,const char * control,Variadic_Arguments &args);
    // result is a method that will allow a program running silently to output its result to stdout
    // Currently it is only used by fsacount, but it is probable some other utilities should be using it
    virtual void result(const char * control,...) __attribute__((format(printf,2,3)));
    virtual void vresult(const char * control,Variadic_Arguments &args);
    virtual size_t read(Input_Stream *stream,Byte * buffer,size_t buf_size);
    virtual void set_gap_stdout(bool on = true);
    virtual void set_interactive(bool on = true);
    virtual bool status(unsigned level,int gap,const char * control,...) __attribute__((format(printf,4,5)));
    virtual bool status(unsigned level,int gap,const char * control,Variadic_Arguments &args);
    virtual bool status_needed(int gap);
    virtual unsigned get_log_level() const
    {
      return log_level;
    }
    void set_log_level(unsigned log_level_)
    {
      log_level = log_level_;
    }
    Output_Stream * get_log_stream()
    {
      return log_stream;
    }
    Output_Stream * get_stderr_stream()
    {
      return stderr_stream;
    }
    Output_Stream * get_stdout_stream()
    {
      return stdout_stream;
    }
    Input_Stream * get_stdin_stream()
    {
      return stdin_stream;
    }
};

#endif
