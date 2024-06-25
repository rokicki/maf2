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
$Log: mafbase.h $
Revision 1.17  2011/01/30 19:59:26Z  Alun
Various new string methods added
Revision 1.16  2010/07/08 16:15:48Z  Alun_Williams
Corrected #ifdef to ensure release build does not GPF on  internal errors
Revision 1.15  2010/06/10 16:26:45Z  Alun
Fixes to ensure clean compilation with g++
Revision 1.14  2010/06/10 13:58:02Z  Alun
All tabs removed again
Revision 1.13  2010/05/18 10:21:33Z  Alun
Changes to many limits and many comments added.
String_Length type added to reduce use of size_t
Boolean type added to facilitate optimisation of bool type functions
Several new GAT_ values in Group_Automaton_Type enumeration
Revision 1.12  2009/11/10 20:06:38Z  Alun
Comment changed
Revision 1.11  2009/10/13 20:43:29Z  Alun
Compatibility of FSA utilities with KBMAG improved.
Compatibility programs produce fewer FSAs KBMAG would not (if any)
Revision 1.10  2009/09/12 18:48:30Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Language_Size data type introduced and used where appropriate
#define FMT_XX types for data types which might be platform specific
Revision 1.9  2008/11/05 00:15:02Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.11  2008/11/05 01:15:01Z  Alun
Another new word ordering
Revision 1.10  2008/11/03 10:36:26Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.9  2008/10/10 08:54:35Z  Alun
Added empty() method for String_Buffer()
Revision 1.8  2008/09/04 10:12:40Z  Alun
"Early Sep 2008 snapshot"
Revision 1.5  2007/12/20 23:25:45Z  Alun
*/
#pragma once
#ifndef MAFBASE_INCLUDED
#define MAFBASE_INCLUDED 1

#ifndef AWCC_INCLUDED
#include "awcc.h"
#endif

// APIMETHOD is used on methods which are only made virtual so as to
// allow them to be called from utilities.
//In the Windows build this avoids the need for the symbol to be explicitly
//exported. This is currently irrelevant in GNU builds and having unnecessary
//virtual methods tends to result in additional warnings from the compiler.
#ifdef __GNUG__
#define APIMETHOD
#else
#define APIMETHOD virtual
#endif

#include <limits.h>

/* Basic types used throughout MAF. It should be possible to change these
   without affecting the code, unless otherwise noted.

   Do not widen the short types unless you really need to. Doing so will
   increase memory usage significantly.

   The FMT_XX constants are used to specify arguments to printf() like
   functions. Almost all output is done via the formatv() method in
   stream.cpp. This implements those parts of C99 printf() that we need,
   so it does not matter whether the compiler's CRT understands these or not.
*/

/* Ordinal must be a signed type capable of holding values from -2 to
   the maximum number of generators. */
typedef short Ordinal;
#if INT_MAX/SHRT_MAX >= SHRT_MAX
typedef int Transition_ID;
#define FMT_TID "%d"
#else
typedef long Transition_ID;
#define FMT_TID "%ld"
#endif

/* Change the next type to allow for longer equations.
   Doing so might well cause stack overflow problems.

   If this type is widened type Total_Length needs to
   be changed to be a type with the same signedness as Word_Length.
*/
typedef unsigned short Word_Length;
typedef size_t String_Length;
/* Total_Length is the type used for representing sums of Word_Length
   values, most usually the total of the LHS and RHS of an equation.

   Word_Length and Total_Length values will always be non-negative, but we
   use a signed type since Word_Length is promoted to int in
   expresssions.
*/
typedef int Total_Length;

/* MAF_MAX_STATES is the maximum number of states we are theoretically going to
   support, and is also the largest number of elements we allow in any finite
   set we are going to realise. It seems sensible to limit to this to the
   typical value of INT_MAX since even on a 64-bit machine an FSA for a
   word-difference machine on the usual four letter alphabet could require up
   to 192GB of memory. In the 32-bit world it makes no sense to allow for
   more than 2^29 states, as there is no hope of instantiating an FSA with
   more states than that, since even on a two-letter alphabet there is
   no way it could use less than 4GB of RAM.

   In principle you should be able to define this to a larger value, at least
   in the case where the LP64 model is used. In the Windows LLP64 model
   to allow for larger numbers of states you would have to change the code
   below to make Element_ID long long. Element_ID _must_ be a signed type.
   The main reason for this is that in an index automaton equations are
   represented by negative state numbers. There are other situations where
   having a spare bit is desirable - for example FSA factories might like to
   use a bit to encode some information which would otherwise require the
   key to be larger.

   MAF will build and run as a 64 bit program on Mac OSX 10.4 or later with
   Element_ID either int or long.

   In practice the number of states we can support is far below MAF_MAX_STATES
   because of space constraints on the transition table.
   The largest FSA the author managed to work with in the 32-bit version of
   MAF had around 55,000,000 states prior to minimisation. */

#ifndef MAF_MAX_STATES
#ifdef _WIN64
#define MAF_MAX_STATES 0x7fffffff
typedef unsigned __int64 Transition_Count;
#define FMT_TC "%llu"
#endif
#endif

#ifndef MAF_MAX_STATES
#if LONG_MAX == 0x7fffffff
#define MAF_MAX_STATES  0x1fffffffL
#define MAF_MAX_COUNT   0x7fffffffL
typedef unsigned long Transition_Count;
#define FMT_TC "%lu"
#else
#define MAF_MAX_STATES 0x7fffffffL
#define MAF_MAX_COUNT  0x7fffffffL
typedef unsigned long long Transition_Count;
#define FMT_TC "%llu"
#endif
#endif

/* I am only using int for Element_ID when necessary, because it has always been
   long in 32-bit versions of MAF, and int and long are the same size.
   Element_UID is provided for times when an unsigned type is needed for some
   arithmetic operation.
*/
#if MAF_MAX_STATES <= INT_MAX && LONG_MAX != INT_MAX
typedef int Element_ID;
typedef unsigned int Element_UID;
#define FMT_ID "%d"
#elif MAF_MAX_STATES <= LONG_MAX
typedef long Element_ID;
typedef unsigned long Element_UID;
#define FMT_ID "%ld"
#else
typedef long long Element_ID
typedef unsigned long long Element_UID
#define FMT_ID "%lld"
#endif

typedef Element_ID Element_Count;
typedef Element_ID State_ID; /* State numbers for states of generic FSAs.
                                We use a signed type to support RWS FSAs */
typedef Element_Count State_Count;

// MAF uses Boolean instead of bool as a return type for some Boolean functions
// With some compilers (C12/C13) making this type an int improves performance
// perceptibly, probably because when bool is used the compiler implements
// many flag tests using shifts.
typedef int Boolean;

/* MAX_STATES is the theoretical number of states we can support.
   In practice the number of states we can support is far below this
   maximum because of space constraints on the transition table. */
const State_Count MAX_STATES = MAF_MAX_STATES;
#ifdef MAF_MAX_COUNT
const State_Count MAX_STATE_COUNT = MAF_MAX_COUNT;
#else
const State_Count MAX_STATE_COUNT = MAF_MAX_COUNT;
#endif
typedef State_ID ID; // used to assign a unique key to each equation in a Rewriter_Machine
#undef MAF_MAX_STATES
#undef MAF_MAX_COUNT

// We use type Node_Count to count equations and nodes in a Rewriter_Machine
typedef Element_Count Node_Count;
#define FMT_NC FMT_ID

// We want to use the largest size of unsigned integer to calculate language
// sizes. Unfortunately CL 12 does not support long long. More unfortunately
// C++ 98 does not have ULLONG_MAX even when compilers have long long
// We should use std::max() but MAF only trusts/uses the Ansi C89 run time
#if defined(ULLONG_MAX) || !defined(_MSC_VER)
typedef unsigned long long Language_Size;
#else
typedef unsigned __int64 Language_Size;
#endif
#define FMT_LS "%llu"
const Language_Size LS_EMPTY = 0;
#ifdef ULLONG_MAX
const Language_Size LS_INFINITE = ULLONG_MAX;
#else
const Language_Size LS_INFINITE = LS_EMPTY - 1;
#endif
const Language_Size LS_HUGE = LS_INFINITE - 1;

typedef char Letter; // change to wchar_t to work in Unicode - code changes will be needed
typedef unsigned char Byte;  /* to emphasize when a char * is not really a char * */

// I attempted to wrap va_list in class Variadic_Arguments to avoid the need
// for including stdarg.h everywhere and because I have experienced problems
// with some compilers' implementation of stdarg.h in the (now distant) past.
// But g++ does not like my doing this, so on GNU platform Variadic_Arguments
// is just a define for the CRT va_list

#if defined(__GNUG__) && !defined(MAF_NO_WRAP_STDARG)
#define MAF_NO_WRAP_STDARG 1
#endif

#ifdef MAF_NO_WRAP_STDARG
#include <stdarg.h>
#define Variadic_Arguments va_list
#else
class Variadic_Arguments;
#endif

class String_Buffer;

/* String is a slight abstraction of a generic const Letter * with a few
   general purpose methods. It does not own and cannot modify the data
   it points to.
*/

class String
{
  private:
    const Letter * ptr;
  public:
    enum Make_Filename_Suffix_Flag
    {
      MFSF_Keep,    // If filename is specified with a suffix use that
      MFSF_Replace, // If filename is specified with a suffix replace it
      MFSF_Append,   /* If filename is specified with a suffix add the new
                       suffix as a second suffix */
      MFSF_Replace_No_Dot /* Same as MFSF_Replace, but the methods that
                             take this flag don't assume that a suffix
                             with no dot was an oversight by the caller */
    };
    String(const Letter * ptr_ = 0):
      ptr(ptr_)
    {}
    String_Length length() const
    {
      String_Length i = 0;
      if (ptr)
        for (;ptr[i]!=0;i++)
          ;
      return i;
    }
    Letter * clone() const
    {
      // caller owns returned string
      String_Length l = length();
      Letter * answer = new Letter[l+1];
      String_Length i;
      for (i = 0;i < l;i++)
        answer[i] = ptr[i];
      answer[i] = 0;
      return answer;
    }
    Letter * clone_substring(String_Length start,String_Length end) const
    {
      // caller owns returned string
      String_Length l = end > start ? end-start : 0;
      Letter * answer = new Letter[l+1];
      String_Length i;
      for (i = start;i < end;i++)
        answer[i] = ptr[i];
      answer[i] = 0;
      return answer;
    }
    bool find(Letter ch,String_Length * pos=0) const
    {
      for (String_Length i = 0;ptr[i];i++)
        if (ptr[i] == ch)
        {
          if (pos)
            *pos = i;
          return true;
        }
      return false;
    }
    bool is_equal(String other,bool ignore_case = false) const;
    String_Length matching_length(String other,bool ignore_case = false) const;
    operator const Letter *() const
    {
      return ptr;
    }
    // This method is needed because g++ is too stupid to be able to pass
    // String as a char * to printf() like methods. g++ gives a bus error
    // while Microsoft C++ compiles with no errors and works fine
    const Letter *string() const
    {
      return ptr;
    }
    String operator++(int)
    {
      String answer(*this);
      ptr++;
      return answer;
    }
    String &operator++()
    {
      ptr++;
      return *this;
    }
    String operator--(int)
    {
      String answer(*this);
      ptr--;
      return answer;
    }
    String &operator--()
    {
      ptr--;
      return *this;
    }
    static String make_filename(String_Buffer * buffer,
                                String const prefix,String const name,
                                String const suffix,
                                bool override_prefix = false,
                                Make_Filename_Suffix_Flag = MFSF_Append);
    // suffix returns a pointer to the final .xxx part of a filename, or
    // to the end of the filename of there is no suffix
    String suffix();
};


/* This is a minimal growable string area for return values
   I originally tried using an auto_ptr like class but it was far more
   trouble than it was worth: causing both portability issues and
   exposing compiler bugs. */
class String_Buffer
{
  private:
    String_Length allocated;
    Letter * buffer;
  public:
    String_Buffer(String_Length initial_size = 32) :
      allocated(initial_size),
      buffer( new Letter[initial_size ? initial_size : 1])
    {
      *buffer = 0;
    }
    ~String_Buffer()
    {
      delete [] buffer;
    }
    String set(String new_value);
    String append(String suffix);
    String append(Letter suffix);
    String append(const String_Buffer &suffix)
    {
      return append(suffix.get());
    }
    String empty()
    {
      return set("");
    }
    String format(const char * control,...) __attribute__((format(printf,2,3)));
    String format(const char * control,Variadic_Arguments &va);
    String make_filename(String const prefix,String const name,
                         String const suffix,
                         bool override_prefix = false,
                         String::Make_Filename_Suffix_Flag flag = String::MFSF_Append)
    {
      return String::make_filename(this,prefix,name,suffix,override_prefix,
                                   flag);
    }
    String make_destination(String output_name,String input_name,String suffix,
                            String::Make_Filename_Suffix_Flag flag =
                              String::MFSF_Append);
    /* reserve() ensures that the buffer can hold a string of new_size letters.
      It is guaranteed that there is room in the buffer for a terminating
      0 at buffer[new_size].
      The return value is the new address of the buffer.
      If a non-zero value for allocated is passed,the actual size of the
      buffer in Letters is filled in (so it will be one more than
      the maximum length of string).
      Unless the "keep" parameter is true the existing contents of the
      buffer are discarded and the return value points to a null string.
      If keep is true then the contents of the old buffer are copied if need
      be.*/
    Letter * reserve(String_Length new_size,String_Length * allocated = 0,bool keep = false);
    String slice(String_Length start,String_Length end);
    String truncate(String_Length length)
    {
      return slice(0,length);
    }
    String get() const
    {
      return buffer;
    }
    bool operator==(const String_Buffer &other) const
    {
      return get().is_equal(other.get());
    }
};

/* MAF has extremely high memory requirements. This means that we have to
   care about every byte. Byte_Buffer is a base class for classes which
   want to pack data from an instance of some class allowing it to
   be quickly recreated. Note that it has no virtual methods, and neither
   should any class derived from it. We want the size of this class to
   be the same size as a pointer.

   The class that is going to use Byte_Buffer should have a method
     PACKED_DATA packed_data() const
   This should first work out how much memory is needed to store the object's
   data, then allocate the memory and write the data into it and finally
   return the pointer.
   Then it should declare a packed class name using the PACKED_CLASS() macro
   below.
   It should also have suitable methods for unpacking this data into an
   instance of the class, e.g. by declaring an assigment operator or constructor,
   or through an unpack() method. All of these should take a const reference to the
   the packed class.

   Typically the data in a byte buffer will be created by one class, and
   stored in another, and commonly it will turn out that a buffer passed
   as a parameter to that other class may or may not want to store the
   contents of the buffer. We want to make it easy to know who owns the
   memory and to allow for transfer of ownership without the need for
   block copying all the time.

   Therefore the class tries to enfore certain protocols analogous to an
   auto_ptr.
*/
class Byte_Buffer
{
  protected:
    void * buffer;
  public:
    explicit Byte_Buffer(void * ptr = 0) :
      buffer(ptr)
    {}
    Byte_Buffer(Byte_Buffer & other) :
      buffer(other.take())
    {}
    ~Byte_Buffer() /* N.B. NOT virtual */
    {
      if (buffer)
        delete (Byte *)  buffer;
    }
    operator const Byte * () const
    {
      return (const Byte *) buffer;
    }
    const void *look() const
    {
      return buffer;
    }
    void * take()
    {
      void * answer = buffer;
      buffer = 0;
      return answer;
    }
    const Byte_Buffer & operator= (void * ptr)
    {
      if (buffer)
        delete (Byte *)  buffer;
      buffer = (Byte *) ptr;
      return *this;
    }
    void clone(const void * data,size_t data_size)
    {
      Byte * new_data = data_size ? new Byte[data_size] : 0;
      const Byte * old_data = (const Byte *) data;
      operator=(new_data);
      for (size_t i = 0; i < data_size;i++)
        new_data[i] = old_data[i];
    }
    const Byte_Buffer & operator=(Byte_Buffer & other)
    {
      if (buffer)
        delete (Byte *)  buffer;
      buffer = other.take();
      return *this;
    }
};

/* Private_Byte_Buffer is a class that provides a growable area of memory
   that is automatically garbage collected, but the memory cannot be
   "stolen" as with a normal Byte_Buffer */
class Private_Byte_Buffer
{
  private:
    Byte_Buffer buffer;
    size_t reserved;
  public:
    Private_Byte_Buffer(size_t size = 0) :
      buffer(0),
      reserved(0)
    {
      reserve(size);
    }
    Byte * reserve(size_t size)
    {
      if (size > reserved)
        buffer = new Byte[reserved = size]; // operator= takes care of
                                            // deleting old value
      return (Byte *) buffer.look();
    }
    Byte * data()
    {
      return reserve(0);
    }
};

/* This is a class for Strings that own their data.
   It's a bit of a mess unfortunately */
class Owned_String : protected Byte_Buffer
{
  public:
    explicit Owned_String(Letter * ptr_ = 0) :
      Byte_Buffer(ptr_)
    {}
    operator const Letter *() const
    {
      return (const Letter *) buffer;
    }
    operator String() const
    {
      return String((const Letter *) buffer);
    }
    String string() const
    {
     return operator String();
    }
    Owned_String & operator=(Letter * s)
    {
      Byte_Buffer::operator=(s);
      return *this;
    }
    Owned_String & operator=(const String & s)
    {
      Byte_Buffer::operator=(s.clone());
      return *this;
    }
    Owned_String &operator=(const Owned_String & other)
    {
      Byte_Buffer::operator=(other.clone());
      return *this;
    }
    // NB != and == are NOT string comparisons but pointer ones needed to avoid
    // ambiguities
    bool operator!=(const Letter * ptr) const
    {
      return (const void *) buffer != ptr;
    }
    bool operator==(const Letter * ptr) const
    {
      return (const void *) buffer == ptr;
    }
    bool operator!() const
    {
      return buffer==0;
    }
    Letter * take()
    {
      // caller now owns string
      return (Letter *) Byte_Buffer::take();
    }
    /* Because this is not derived from String we have to reimplement some methods */
    String_Length length() const
    {
      return string().length();
    }
    Letter * clone() const
    {
      return string().clone();
    }
    Letter * clone_substring(String_Length start,String_Length end) const
    {
      return string().clone_substring(start,end);
    }
    bool is_equal(String other,bool ignore_case = false) const
    {
      return other.is_equal(string(),ignore_case);
    }
    size_t matching_length(String other,bool ignore_case = false) const
    {
      return other.matching_length(string(),ignore_case);
    }
    String suffix()
    {
      return string().suffix();
    }
};

#define PACKED_CLASS_EX(class_name,unpacked_class,pack_method) \
class class_name : public Byte_Buffer \
{ \
  public: \
    explicit class_name(const unpacked_class & object,size_t * size = 0) : \
      Byte_Buffer(object.pack_method(size)) \
    {} \
    class_name(class_name & other) : \
      Byte_Buffer(other) \
    {} \
    class_name() \
    {} \
    const class_name & operator=(class_name & other) \
    { \
      Byte_Buffer::operator=(other); \
      return * this; \
    } \
    const class_name & operator=(const unpacked_class & object) \
    { \
      Byte_Buffer::operator=(object.pack_method()); \
      return * this; \
    } \
    static size_t size(const void *); \
    size_t size() const \
    { \
      return size(buffer); \
    } \
}

#define PACKED_CLASS(class_name,unpacked_class) \
          PACKED_CLASS_EX(class_name,unpacked_class,packed_data)

/* PACKED_DATA is used as the return type from packed_data() functions.
   This should really be a Byte_Buffer. However, this causes problems for
   compilers. auto_ptr implementations use peculiar tricks to avoid this
   but these tend to be non-portable.
   So we live without the auto-delete that a Byte_Buffer return type would
   give us and return the value as a void * instead. Note that we are
   NOT returning a const void *, otherwise the automatic management of
   ownership of the object would be broken completely.
*/
#define PACKED_DATA void *

typedef unsigned char Language;

const Word_Length WHOLE_WORD = (Word_Length) ~0;
const String_Length WHOLE_STRING = (String_Length) ~0;
const Word_Length MAX_WORD = WHOLE_WORD-2;
const Word_Length INVALID_LENGTH = WHOLE_WORD;
const Total_Length UNLIMITED = WHOLE_WORD*2;

enum Alphabet_Type
{
  AT_Char,
  AT_String,
  AT_Simple
};

enum Word_Ordering
{
  WO_Shortlex,
  WO_Weighted_Lex,   // lighter words are better, amongst words of equal weight use lex order
  WO_Weighted_Shortlex,//lighter words are better, amongst words of equal weight use shortlex
  WO_Accented_Shortlex,// shorter words are better, amongst words of equal weight use accented lex
  WO_Multi_Accented_Shortlex,// shorter words are better, amongst words of equal weight use multiaccented lex
  WO_Recursive,
  WO_Right_Recursive,
  WO_Wreath_Product,
  WO_Right_Wreath_Product,
  WO_Short_Recursive, // shorter words are better, amongst words of equal
                      // length use recursive
  WO_Short_Right_Recursive, // shorter words are better, amongst words of
                           // equal length use right_recursive
  WO_Short_Weighted_Lex,  // shorter words are better, amongst words of equal
                          // length use weighted lex
  WO_Right_Shortlex, // shortlex of the reversed words
  WO_Short_FPTP,     // shorter words are better,amongst words of equal length
                     // the word which first reaches the highest generator is
                     // greater
  WO_Grouped,//limiting case of Weighted_Shortlex where ratio between weights
             //is infinite
  WO_Ranked, // cross between Wreath_Product_Order and WO_Grouped
  WO_NestedRank, // cross between Wreath_Product_Order and WO_Grouped
  WO_Coset // this is similar to Wreath_Product ordering, but adjusted so
           // that h*_H*b < _H*b*a in the case where 'b' is at a higher level
           //  than 'a'
};

/* MAF does not care what the following two classes are. The platform object
   returns pointers to these when requested, but MAF never dereferences them
   (except in its default implementation of the interface in platform.cpp).
   The implementation could even use reinterpret casts and return some
   other type. The only requirement is that the implementation can turn
   them back into something it can use to perform the required task.
*/

class Output_Stream;
class Input_Stream;

#ifdef DEBUG
/* Definition of TERMINATE macro used to give DEBUG builds
   a chance to JIT debug an error.
   In the DEBUG case I want to raise an exception as close as possible
   to the offending piece of code, so don't want to call some library
   function to raise an exception. If this causes a problem then change
   * (char *) 0 = 0; to a call to a suitable library function: e.g.
   abort(), assert(false), or throw(). */
#define MAF_TERMINATE(container) (* (char *) 0 = 0)
#else
#define MAF_TERMINATE(container) container.die()
#endif

#define MAF_INTERNAL_ERROR(container,a) \
  { \
    container.error_output("MAF:: Internal error at line %d of file %s\n",__LINE__,__FILE__);\
    container.error_output a ; \
    MAF_TERMINATE(container); \
  }

#ifdef DEBUG
#define MAF_ASSERT(expr,container,a) \
  if (!(expr)) \
    MAF_INTERNAL_ERROR(container,a)
#else
#define MAF_ASSERT(expr,container,a)
#endif

/* An enumeration type that is referred to very widely.

   Group_Automaton_Type really belongs in maf.h. But putting it here
   saves us from having to include maf.h in every file that wants to
   use Standard_Options

   If this enumeration is changed then maf.h's FSA_Buffer needs to
   change as well, and new members must be added its static members.
*/

enum Group_Automaton_Type
{
  GAT_Minimal_RWS,               /* The minimal rewriting system (possibly
                                    incomplete). If an automatic structure has
                                    been found, only the equations necessary
                                    for establishing the complete set of word
                                    differences and transitions is emitted. If
                                    the system is confluent it is correct. In
                                    other cases it is what has been found so
                                    far. */

  GAT_Fast_RWS,                  /* A rewriting system including secondary
                                    equations. Note that in the infinite case
                                    this is unlikely to be confluent even when
                                    the minimal system is. In such cases only
                                    equations up to the length of the longest
                                    primary are found.*/
  GAT_Provisional_RWS,           /* The rewriting system produced when MAF aborts */

  GAT_General_Multiplier,        /* The general multiplier: the product FSA
                                    which has a labelled accept state for
                                    every accepted word and generator, in which
                                    the right word is the product.
                                    For coset systems this is a MIDFA */
  GAT_Deterministic_General_Multiplier,
                                 /* For groups this is the same FSA as the
                                    General Multiplier. For coset systems it
                                    is the determinised version of the
                                    multiplier */
  GAT_GM2,                       /* GAT_GM2 and GAT_DGM2 are the general
                                    multipliers for words of length 1 and 2 */
  GAT_DGM2,


  GAT_WA,                        /* The word-acceptor */
  GAT_Coset_Table,               /* The coset table */
  GAT_Conjugacy_Class,           /* The conjugacy class representative */
  GAT_Conjugator,                /* The product FSA which can find the
                                    least word which conjugates an element
                                    to its conjugacy class representative */
  GAT_Full_Difference_Machine,   /* The complete difference machine - includes
                                    all possible transitions between word
                                    differences, not just those required for
                                    the general multiplier */
  GAT_L1_Acceptor,               /* The FSA which accepts the minimally
                                    reducible words */
  GAT_Primary_Recogniser,        /* FSA which recognises primary equations */
  GAT_Primary_Difference_Machine,/* The minimum difference machine that can
                                    correctly reduce all words */
  GAT_Equation_Recogniser,       /* The FSA which recognises all equations of
                                    the form ux=v where u and v are accepted
                                    words and no cancellations are possible.*/
  GAT_Reduction_Recogniser,     /* The FSA which accepts all pairs of
                                   words (u,v) where v is accepted and every
                                   prefix of u is accepted and u=v in the group
                                   (or the analogous coset system fsa) */

  GAT_Correct_Difference_Machine,/* The minimum difference machine that can
                                     reduce all words with an irreducible
                                     prefix in one pass. */
  GAT_Provisional_DM1,           // Provisional FSAs created when MAF can't
  GAT_Provisional_DM2,           //complete

  GAT_Subgroup_Word_Acceptor,    /* For a coset system the FSA that accepts
                                    precisely those words which are reduced in
                                    terms of the group generators, and which
                                    are members of the subgroup */
  GAT_Subgroup_Presentation,    /* For a coset system a presentation for the
                                   subgroup. This is not an FSA, but it is
                                   convenient to include it in this
                                   enumeration */
  GAT_Auto_Select = -1
};

#endif
