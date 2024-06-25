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


// $Log: mafbase.cpp $
// Revision 1.8  2009/10/22 23:41:50Z  Alun
// Revision 1.7  2009/10/07 16:04:28Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.6  2009/09/12 18:47:43Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2008/11/02 20:46:00Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.4  2008/08/02 19:17:50Z  Alun
// "Aug 2008 snapshot"
// Revision 1.3  2007/11/15 22:58:08Z  Alun
//

#include <string.h>
#include "mafbase.h"
#include "mafctype.h"
#include "sstream.h"
#include "variadic.h"
#include "awdefs.h"

String String::suffix()
{
  /* look for suffix part of a filename */
  const Letter * sf = ptr + length();
  String answer = sf;
  while (sf > ptr && !is_path_separator(sf[-1]))
    if (*--sf == '.')
      return sf;
  return answer;
}

/**/

bool String::is_equal(String other,bool ignore_case) const
{
  const Letter *s = ptr ? ptr : "";
  const Letter *t = other.ptr ? other.ptr : "";
  if (ignore_case)
    while (*s && char_classification.to_upper_case(*s) ==
                 char_classification.to_upper_case(*t))
      s++,t++;
  else
    while (*s && *s == *t)
      s++,t++;
  return *s == *t;
}

/**/

size_t String::matching_length(String other,bool ignore_case) const
{
  const Letter *s = ptr ? ptr : "";
  const Letter *start = s;
  const Letter *t = other.ptr ? other.ptr : "";
  if (ignore_case)
    while (*s && char_classification.to_upper_case(*s) ==
                 char_classification.to_upper_case(*t))
      s++,t++;
  else
    while (*s && *s == *t)
      s++,t++;
  return s - start;
}

/**/

String String::make_filename(String_Buffer *buffer,String const prefix,
                             String const name,String const suffix,
                             bool override_prefix,
                             Make_Filename_Suffix_Flag suffix_flag)
{
  size_t pl;
  const Letter * pf;
  const Letter * nm;
  const Letter * sf;
  /* This method build a filename based on a starting name, a prefix,
     (usually a pathname or empty), a suffix and a set of flags to indicate
     what to do when the supplied filename contains a path/suffix already.
     This code started out life outside MAF which is why it looks at paths
     at all. Currently MAF doesn't care about paths at all, but it might one
     day, so the code has been retained.
  */

  /* look for prefix and suffix in name */
  sf = nm = name + name.length();
  bool suffix_done = false;
  while (nm > (const Letter *) name && !is_path_separator(nm[-1]))
  {
    if (*--nm == '.')
      if (suffix_flag != MFSF_Append && !suffix_done)
      {
        sf = nm;
        suffix_done = true;
      }
  }

  /* length of prefix */
  if (override_prefix || nm == name)
  {
    pf = prefix;
    pl = String(pf).length();
  }
  else
  {
    pf = name;
    pl = nm - pf;
  }

  /* length of base name */
  size_t nl = sf - nm;

  /* length of suffix */
  if (suffix_flag != MFSF_Keep || (*sf != '.'))
    sf = suffix;
  size_t sl = String(sf).length();
  bool no_fix = true;
  if (*sf && *sf != '.' && suffix_flag != MFSF_Replace_No_Dot)
  {
    sl++;
    no_fix = false;
  }

  size_t len = pl + nl + sl;
  Letter * answer = buffer->reserve(len);
  /* copy prefix to result */
  memcpy(answer,pf,pl*sizeof(Letter));
  /* copy base name to result */
  memcpy(answer+pl,nm,nl*sizeof(Letter));
  /* copy suffix to result */
  if (*sf)
    if (*sf == '.' || no_fix)
      memcpy(answer+pl+nl,sf,sl*sizeof(Letter));
    else
    {
      answer[pl+nl] = '.';
      memcpy(answer+pl+nl+1,sf,(sl-1)*sizeof(Letter));
    }
  answer[len] = 0;
  return answer;
}

String String_Buffer::make_destination(String output_name,String input_name,
                                       String suffix,
                                       String::Make_Filename_Suffix_Flag flag)
{
  if (!output_name)
  {
    if (!input_name)
      return 0;
    return make_filename("",input_name,suffix,false,flag);
  }
  return output_name;
}


Letter * String_Buffer::reserve(size_t new_size,size_t *actual/* = 0*/,bool keep /*= false*/)
{
  /* On entry new_size is the length of the string that is to be stored.
     It is guaranteed that there is room in the buffer for a terminating
     0 at buffer[new_size] */

  if (new_size >= allocated)
  {
    allocated = (new_size + 32) & ~31; // at least 1 more than requested
                                       // size, so enough for 0 at end
    Letter * new_buffer = new Letter[allocated];
    if (keep)
      for (size_t i = 0;(new_buffer[i] = buffer[i])!=0;i++)
        ;
    delete [] buffer;
    buffer = new_buffer;
  }
  if (!keep)
    buffer[0] = 0;
  if (actual)
    *actual = allocated;
  return buffer;
}

/**/

String String_Buffer::set(String new_value)
{
  size_t new_size = new_value.length();
  reserve(new_size);
  size_t i = 0;
  for (;i < new_size;i++)
    buffer[i] = new_value[i];
  buffer[i] = 0;
  return buffer;
}

String String_Buffer::append(String suffix)
{
  size_t old_length = get().length();
  size_t suffix_length = suffix.length();
  reserve(old_length + suffix_length,0,true);
  for (size_t i = 0;(buffer[old_length+i] = suffix[i])!=0;i++)
    ;
  return buffer;
}

String String_Buffer::append(Letter suffix)
{
  size_t old_length = get().length();
  reserve(old_length + 1,0,true);
  buffer[old_length] = suffix;
  buffer[old_length+1] = '\0';
  return buffer;
}

String String_Buffer::format(const char * control,...)
{
  DECLARE_VA(va,control);
  return format(control,va);
}

String String_Buffer::slice(size_t start,size_t end)
{
  if (start != 0)
  {
    Letter * s = buffer;
    if (end > allocated)
      end = allocated;
    for (size_t i = start;i < end;i++)
    {
      if (!buffer[i])
        break;
      *s++ = buffer[i];
    }
    *s++ = 0;
  }
  else if (end < allocated)
    buffer[end] = 0;
  return buffer;
}

String String_Buffer::format(const char * control,Variadic_Arguments & va)
{
  String_Buffer_Stream sbs(this);
  reserve(0);
  sbs.formatv(control,va);
  return get();
}

/**/

size_t String_Buffer_Stream::write(const Byte * data,size_t nr_bytes)
{
  Letter * buffer = sb->reserve(position + nr_bytes,0,true) + position;
  memcpy(buffer,data,nr_bytes*sizeof(Byte));
  position += nr_bytes;
  buffer[nr_bytes] = 0;
  return nr_bytes;
}

/**/

size_t String_Buffer_Stream::read(Byte * data,size_t nr_bytes)
{
  size_t i;
  String s = sb->get();
  i = s.length();
  s = s + min(position,i);
  for (i = 0; i < nr_bytes;i++)
    if (*s)
      *data++ = *s++;
    else
      break;
  return i;
}

