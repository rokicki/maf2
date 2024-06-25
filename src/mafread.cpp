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


// $Log: mafread.cpp $
// Revision 1.15  2010/06/17 10:22:06Z  Alun
// Check either subGenerators[] or normalSubGenerators present in sub files
// Revision 1.14  2010/06/10 13:57:34Z  Alun
// All tabs removed again
// Revision 1.13  2010/05/12 22:31:46Z  Alun
// Better handling of parse errors
// Revision 1.12  2009/11/11 00:15:50Z  Alun
// tabs removed
// Revision 1.11  2009/10/30 10:10:55Z  Alun
// Did not handle line continuation properly.
// Changes required by changes to alphabet property methods
// Revision 1.10  2009/09/14 09:58:03Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.9  2009/01/01 22:02:16Z  Alun
// Support for normal subgroups added. Better parse error messages
// Revision 1.8  2008/11/03 01:36:20Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.7  2008/09/20 09:55:15Z  Alun
// Final version built using Indexer.
// Revision 1.6  2007/12/20 23:25:42Z  Alun
//

/* This module contains most of the input side of I/O done in MAF.
   Input is taken from KBMAG format files so that MAF and KBMAG are largely
   compatible with one another.
   The classes that need to read KBMAG files do so via friend classes that
   provide the appropriate services. Things are done this way for three
   reasons:

   1) It reduces the number of "private" methods the main classes need to
      expose in the header files.

   2) It allows the classes that read KBMAG files to share common code without
   requiring inheritance to be used in the main classes.

   3) Should an interface with another file format be required, most changes
   will be localised here.
*/

#include <string.h>
#include "variadic.h"
#include "mafword.h"
#include "mafctype.h"
#include "container.h"
#include "fsa.h"
#include "maf_sub.h"
#include "maf_el.h"
#include "maf_rws.h"

enum Token_Type
{
  TT_False,
  TT_True,
  TT_Boolean,
  TT_Rec,
  TT_Identifier,
  TT_Natural,
  TT_Integer,
  TT_List,
  TT_String,
  TT_Word,
  TT_Expression,
  TT_Comma,
  TT_Range,
  TT_Definition, /* := */
  TT_End_Of_List,
  TT_End_Of_Rec
};

const int MAX_IDENTIFIER = 256;

#ifdef _MSC_VER
#pragma optimize("s",on) /* we don't need this code to be especially fast */
#pragma warning(disable:4640) /* the static objects in this module are all const */
#endif

class GAP_Reader : public Parse_Error_Handler
{
  protected:
    Input_Stream * input;
    Container & container;
    unsigned char buffer[4096];
    char identifier[2][MAX_IDENTIFIER+1];
    size_t buf_size;
    bool slash_pending;
    char pending_char;
    char * string_buffer;
    size_t string_size;
    size_t current;
    size_t length;
    unsigned long line;
    unsigned column;
  public:
    GAP_Reader(Input_Stream * input_,Container &container_) :
      input(input_),
      container(container_),
      current(0),
      slash_pending(false),
      string_buffer(new char[string_size = 32]),
      line(1),
      column(1),
      length(0)
    {
      buf_size = container.read(input,buffer,sizeof(buffer));
    }
    virtual ~GAP_Reader()
    {
      delete string_buffer;
    }
 protected:

    void input_error(const char * control,...) __attribute__((format(printf,2,3)))
    {
      // this is not quite right yet - the column will be a bit off usually
      DECLARE_VA(va,control);
      String_Buffer sb;
      sb.format(control,va);
      callback_parse_error(sb.get().string());
    }

    void parse_error(String s,String arg="")
    {
      container.input_error("Syntax error at line %lu column %u:\n%s%s\n",
                            line,column,s.string(),arg.string());
    }

    void callback_parse_error(String s,String arg="")
    {
      container.input_error("Syntax error before line %lu column %u:\n%s%s\n",
                            line,column,s.string(),arg.string());
    }

    void expect(Token_Type expected, Token_Type actual)
    {
      static const Letter * const descriptions[]=
      {
        "false",
        "true",
        "true or false",
        "rec(",
        "identifier",
        "non-negative integer",
        "integer",
        "start of list",
        "string",
        "word",
        "expression",
        ",",
        "..",
        "end of list"
      };
      bool wrong;
      if (expected == TT_Integer)
        wrong = actual != TT_Natural && actual != TT_Integer;
      else if (expected == TT_Boolean)
        wrong = actual != TT_True && actual != TT_False;
      else
        wrong = actual != expected;

      if (wrong)
      {
        String_Buffer sb;
        sb.format("Expected %s, got %s",descriptions[expected],
                                        descriptions[actual]);
        parse_error("Unexpected token:",sb.get().string());
      }
    }

    Token_Type next_token_type(bool rec_allowed)
    {
      skip_whitespace();
      unsigned char inchar = look_char(false);
      if (is_digit(inchar))
        return TT_Natural;
      if (inchar == '-')
        return TT_Integer;
      if (inchar == ')')
        return TT_End_Of_Rec;
      if (inchar == '"')
        return TT_String;
      if (inchar == '[')
        return TT_List;
      if (inchar == ']')
        return TT_End_Of_List;
      if (inchar == ',')
        return TT_Comma;
      if (inchar == '.')
        return TT_Range;
      if (inchar == ':')
        return TT_Definition;
      if (rec_allowed)
      {
        String identifier = parse_identifier(1);
        if (strcmp(identifier,"rec")==0)
          return TT_Rec;
        else if (strcmp(identifier,"true")==0)
          return TT_True;
        else if (strcmp(identifier,"false")==0)
          return TT_False;
        return TT_Identifier;
      }
      return TT_Expression;
    }

    String parse_pair(Token_Type * token_type,bool allow_dot = false)
    {
      /* Parse a phrase of the form identifier = value as far as is needed
         to determine the type of the value */
      String word = parse_identifier(0,allow_dot);
      eat(":=");
      *token_type = next_token_type(true);
      return word;
    }

    void eat(String string)
    {
      skip_whitespace();
      for (String s = string;*s;s++)
      {
        if (*s != next_char())
          parse_error("Expected ",string);
      }
    }

    int parse_member(String const * res_words,Token_Type * token_type)
    {
      /* identify the next pair in a rec() using the specified list of
         members */
      skip_whitespace();
      String word = parse_pair(token_type);
      return enum_value(res_words,word);
    }

    int enum_value(String const * res_words,String word) const
    {
      for (int i = 0; res_words[i];i++)
        if (strcmp(res_words[i],word)==0)
          return i;
      return -1;
    }

    bool skip_comma()
    {
      skip_whitespace();
      if (look_char(false) == ',')
      {
        next_char();
        return true;
      }
      return false;
    }

    void parse_unknown_value(Token_Type token_type)
    {
      switch (token_type)
      {
        case TT_Expression:
          parse_expression();
          break;
        case TT_Identifier:
        case TT_True:
        case TT_False:
          // These types were read already when we determined the token type
          break;
        case TT_List:
          parse_unknown_list();
          break;
        case TT_Integer:
          parse_integer();
          break;
        case TT_Natural:
          parse_natural();
          break;
        case TT_String:
          parse_string(0);
          break;
        case TT_Rec:
          parse_unknown_rec();
          break;
        case TT_Boolean:
        case TT_Word:
        case TT_Comma:
        case TT_Range:
        case TT_Definition:
        case TT_End_Of_List:
        case TT_End_Of_Rec:
          MAF_INTERNAL_ERROR(container,
                             ("Unexpected token type %d in"
                              " GAP_Reader::parse_unknown_value()",
                              token_type));
          break;
      }
    }

    void parse_unknown_list()
    {
      /* Parse an unrecognised GAP list. This is actually easier
         than parsing one we do understand!
      */
      eat("[");
      for (;;)
      {
        Token_Type token_type = next_token_type(false);
        if (token_type == TT_End_Of_List)
          break;
        parse_unknown_value(token_type);
        if (token_type == TT_Integer || token_type == TT_Natural)
        {
          token_type = next_token_type(false);
          if (token_type == TT_Range)
          {
            eat("..");
            token_type = next_token_type(false);
            expect(TT_Natural,token_type);
            parse_unknown_value(token_type);
          }
        }
        if (!skip_comma())
          break;
      }
      eat("]");
    }

    long parse_integer()
    {
      long answer = 0;
      bool negative = false;

      if (look_char(false) == '-')
      {
        negative = true;
        next_char();
      }
      answer = parse_natural();
      if (negative)
        answer = -answer;
      if (negative ? answer > 0 : answer < 0)
        parse_error("Sorry! Integer overflow occurred reading number");
      return answer;
    }

    unsigned long parse_natural()
    {
       unsigned long value = 0;
       for (;;)
       {
         unsigned char inchar = look_char(false);
         if (is_digit(inchar))
         {
           next_char();
           unsigned long new_value = value*10+inchar-'0';
           if (new_value < value)
             parse_error("Sorry! Integer overflow occurred reading number");
           value = new_value;
         }
         else
           return value;
       }
    }

    void append(char inchar)
    {
      // helper function for methods that put stuff into string_buffer
      if (length + 2 >= string_size)
      {
        char * new_string = new char[string_size += 32];
        memcpy(new_string,string_buffer,length);
        delete [] string_buffer;
        string_buffer = new_string;
      }
      string_buffer[length++] = inchar;
    }

    String parse_string(bool reset_length = true)
    {
      eat("\"");
      if (reset_length)
        length = 0;

      for (;;)
      {
        char inchar = next_char();
        if (inchar == '"')
          break;
        append(inchar);
      }
      string_buffer[length] = 0;
      return string_buffer;
    }

    String parse_expression(bool reset_length = true)
    {
      /* Actually all this does is to extract the string for the expression
         from the input and put it into the string buffer. So far as I
         understand a GAP expression cannot contain a ':' ',' or a ']', so we
         simply look for the next one of these. If this turns out not
         to be the case, the code here will have to get more complicated.
      */

      if (reset_length)
        length = 0;
      char inchar;

      for (;;)
      {
        if (skip_whitespace()) //we must not include comments
          inchar = ' '; //nor merge tokens as a result of doing so
        else
        {
          inchar = look_char(false);
          if (inchar == ',' || inchar == ']' || inchar == ':')
            break;
          next_char();
        }
        append(inchar);
      }
      string_buffer[length] = 0;
      return string_buffer;
    }

    String parse_list_as_text(bool reset_length = true)
    {
      /* All this does is to extract the string for the list from the input
         and put it into the string buffer. This is more difficult than
         it sounds because we need to filter out comments, and not get
         confused by strings or nested lists.
      */
      if (reset_length)
        length = 0;
      Token_Type tt;
      eat("[");
      for (;;)
      {
        if (skip_whitespace())
          append(' ');
        tt = next_token_type(false);
        if (tt == TT_End_Of_List)
        {
          /* eat the closing ']' */
          next_char();
          break;
        }
        else
        {
          switch (tt)
          {
            case TT_Comma:
              append(next_char());
              break;
            case TT_List:
              append(next_char());
              parse_list_as_text(false);
              append(']');
              break;
            case TT_String:
              append(next_char());
              parse_string(false);
              append('"');
              break;

           case TT_Definition:
             parse_error("Unexpected assignment operator (:=)."
                          " Did you forget a closing ]?\n");
             break;

           case TT_End_Of_Rec:
             parse_error("Unexpected ) ."
                          " Did you forget a closing ]?\n");
             break;

           case TT_False:
           case TT_True:
           case TT_Rec:
           case TT_Identifier:
           case TT_Natural:
           case TT_Integer:
           case TT_Word:
           case TT_Expression:
           case TT_Range:
           default: /* treat all other types as expressions */
              parse_expression(false);
              break;

           case TT_Boolean:     /* no token has this type */
           case TT_End_Of_List: /* we filtered that out */
             NOT_REACHED(break;);
          }
        }
      }
      string_buffer[length] = 0;
      return string_buffer;
    }

    void parse_unknown_rec()
    {
      static String const expected_words[] =
      {
        0
      };
      Token_Type token_type;

      eat("(");

      for (;;)
      {
        parse_member(expected_words,&token_type);
        parse_unknown_value(token_type);
        if (!skip_comma())
          break;
      }
      eat(")");
    }

    String parse_identifier(int index,bool allow_dot = false)
    {
      int i = 1;
      char * answer = identifier[index];
      unsigned flag = allow_dot ? CC_Subsequent|CC_Dot : CC_Subsequent;

      skip_whitespace();
      answer[0] = next_char();
      if (!is_initial(answer[0]))
        parse_error("Identifier expected");

      for (;;i++)
      {
        unsigned char inchar = look_char(false);
        if (is_flagged(inchar,flag))
        {
          if (i >= MAX_IDENTIFIER)
            parse_error("Sorry - identifier too long");
          answer[i] = inchar;
          next_char();
        }
        else
        {
          answer[i] = 0;
          break;
        }
      }
      return answer;
    }

    char next_char()
    {
      return look_char(true);
    }

    bool skip_whitespace()
    {
      // skip over whitespace and comments. We return true if we do skip over
      // something to allow caller to insert a token separator if need be

      bool skipped = false;

      for (;;)
      {
        char inchar = look_char(false);
        if (inchar == '#') // Skip comments
        {
          while ((inchar = next_char()) != '\n')
            ;
          skipped = true;
          continue;
        }
        if (!is_white(inchar))
          return skipped;
        skipped = true;
        next_char();
      }
    }

    char look_char(bool take)
    {
      // This is where we actually read the file
      // We may need to look at the same character several times before
      // we digest it, hence the parameter.

      if (slash_pending)
      {
        if (take)
          slash_pending = false;
        return pending_char;
      }

      /* read one character */
      for (;;)
      {
        if (!buf_size)
        {
          /* the files we read should be semicolon terminated, so we should
             never actually get to the end of file */
          parse_error("Unexpected end of file");
        }
        char answer = buffer[current];
        if (take || answer == '\r')
        {
          current++;
          column++;
          if (current == buf_size)
          {
            container.status(2,1,"Reading file. %lu lines read so far\n",line);
            buf_size = container.read(input,buffer,sizeof(buffer));
            current = 0;
          }

          if (answer == '\n')
          {
            line++;
            column = 1;
          }
        }
        if (answer == '\\')
        {
          if (!take)
          {
            /* This is very unpleasant. We have to read the current
               character to find out whether we have a real \ or a
               line continuation.
               We have no choice but to swallow it and put it back */
            answer = next_char();
            slash_pending = true;
            pending_char = answer;
          }
          else
          {
            /* here we have digested the slash, but we need to find out
               what the next input is, since if we are at the end of a
               line we should ignore the slash and return the next symbol
               instead */
            char next = look_char(false);
            if (next == '\n')
            {
              // silently digest both line continuation and newline
              next_char();
              continue;
            }
            else
              return '\\'; // we should return the slash and look_char is OK.
          }
        }
        if (answer != '\r')
          return answer;
      }
    }
};

class RWS_Reader :  public GAP_Reader
{
  public:
    MAF &maf;
    RWS_Reader(Input_Stream * input_,MAF &maf_) :
      GAP_Reader(input_,maf_.container),
      maf(maf_)
    {}

    void parse_rws(unsigned flags)
    {
      static String const expected_words[] =
      {
        "isRWS",
        "isConfluent",
        "ordering",
        "generatorOrder",
        "weight",
        "level",
        "inverses",
        "equations",
        "tidyint",
        "maxeqns",
        "maxstoredlen",
        "maf_maxstoredlen",
        "silent",
        "verbose",
        0
      };
      const int count = sizeof(expected_words)/sizeof(expected_words[0]);
      bool found[count];
      int i;
      Token_Type token_type;
      Word_Ordering ordering = WO_Shortlex;
      bool create_rm = (flags & CFI_CREATE_RM) != 0;
      bool order_is_set = (flags & CFI_RESUME) != 0;
      maf.name = parse_pair(&token_type).clone();
      expect(TT_Rec,token_type);
      eat("(");
      for (i = 0;i < count;i++)
        found[i] = 0;
      for (;;)
      {
        int i = parse_member(expected_words,&token_type);
        if (i != -1)
        {
          if (found[i])
            parse_error("Value already specified for ",expected_words[i]);
          found[i] = true;
          if (!found[0])
            parse_error("First member of GAP RWS record must be isRWS := true");
        }
        switch (i)
        {
          case -1:
            maf.container.error_output("Ignoring unknown member %s\n",identifier[0]);
            parse_unknown_value(token_type);
            break;
          case 0:
            expect(TT_True,token_type);
            break;
          case 1:
            expect(TT_Boolean,token_type);
            if (token_type == TT_True)
              maf.options.assume_confluent = true;
            break;
          case 2:
            {
              /* Aargh! KBMAG does not adhere to its own documented file format
                 with this field. fsa_format says that that generatorOrder and
                 ordering can come in either order, but must come before other
                 fields. However, the GAP interface puts this field after the
                 inverses. Luckily this does not matter since the inverse equations
                 don't need any balancing.
              */
              expect(TT_String,token_type);
              String s = parse_string();
              ordering = (Word_Ordering)
                               enum_value(maf.real_alphabet.word_orderings(),s);
              if ((int) ordering == -1)
                parse_error("Unknown word ordering:",s);
            }
            break;
          case 3:
            expect(TT_List,token_type);
            if (!(flags & CFI_RESUME))
              maf.set_generators(parse_list_as_text());
            else
              parse_list_as_text();
            break;

          case 4:
            expect(TT_List,token_type);
            if (!maf.alphabet.order_properties[ordering].has_weights())
              parse_error("weight field is not used when ordering is ",
                           maf.alphabet.word_orderings()[ordering]);
            if (!(flags & CFI_RESUME))
              parse_weights();
            else
              parse_list_as_text();
            break;
          case 5:
            expect(TT_List,token_type);
            if (!maf.alphabet.order_properties[ordering].has_levels())
              parse_error("level field is not used when ordering is ",
                          maf.alphabet.word_orderings()[ordering]);
            if (!(flags & CFI_RESUME))
              parse_levels();
            else
              parse_list_as_text();
            break;

          case 6:
            {
              if (!found[3])
                parse_error("generatorOrder must be specified before inverses");
              Ordinal g = 0;
              Ordinal nr_generators = maf.alphabet.letter_count();
              expect(TT_List,token_type);
              const Letter * inverse_string = parse_list_as_text();
              if (!(flags & CFI_RESUME))
              {
                for (g = 0;g < nr_generators;g++)
                {
                  if (*inverse_string == ' ')
                    inverse_string++;
                  if (is_initial(*inverse_string))
                  {
                    Ordinal ig = maf.alphabet.parse_identifier(inverse_string,true);
                    if (ig >= 0)
                      maf.set_inverse(g,ig,create_rm ? AA_ADD_TO_RM : 0);
                    else
                      parse_error("Unknown generator specified as inverse of ",maf.alphabet.glyph(g));
                    if (*inverse_string==' ')
                      inverse_string++;
                  }
                  if (*inverse_string == ',')
                    inverse_string++;
                  if (!*inverse_string)
                    break;
                }
              }
            }
            break;
          case 7:
            if (!(flags & CFI_RESUME))
            {
              if (maf.alphabet.order_properties[ordering].has_weights() &&
                  !found[4])
                parse_error("weight field must be specified before equations if"
                            " using ordering",maf.alphabet.word_orderings()[ordering]);
              if (maf.alphabet.order_properties[ordering].has_levels() &&
                  !found[5])
                parse_error("level field must be specified before equations if"
                            " using ordering ",maf.alphabet.word_orderings()[ordering]);
            }
            expect(TT_List,token_type);
            parse_equations(flags);
            break;
          case 8:
            parse_natural();
            break;
          case 9:
            maf.options.max_equations = parse_natural();
            break;
          case 10:
          case 11:
            expect(TT_List,token_type);
            parse_maxstoredlen(i==11);
            break;
          case 12:
            expect(TT_Boolean,token_type);
            if (token_type == TT_True)
              container.set_log_level(maf.options.log_level = 0);
            break;
          case 13:
            expect(TT_Boolean,token_type);
            if (token_type == TT_True)
              container.set_log_level(maf.options.log_level = 2);
            break;
        }
        if (found[3] && found[2] && !order_is_set)
        {
          maf.real_alphabet.set_word_ordering(ordering);
          order_is_set = true;
        }
        if (!skip_comma())
        {
          if (next_token_type(false) != TT_End_Of_Rec)
            parse_error("Expected comma");
          break;
        }
      }
      eat(")");
      eat(";");
      if (!found[0] || !found[3] || !found[7])
        parse_error("Expected data not found: file does not contain a GAP RWS");
    }

    void parse_equations(unsigned flags)
    {
      unsigned aa_flags = flags & CFI_CREATE_RM ? AA_DEFAULT : 0;
      if (flags & CFI_CREATE_RM && flags & CFI_RESUME)
        aa_flags |= AA_RESUME;
      Token_Type tt;
      eat("[");
      for (;;)
      {
        tt = next_token_type(false);
        if (tt == TT_End_Of_List)
          break;
        expect(TT_List,tt);
        parse_one_equation(aa_flags);
        if (!skip_comma())
          break;
      }
      eat("]");
    }

    void parse_one_equation(unsigned aa_flags)
    {
      eat("[");
      expect(TT_Expression,next_token_type(false));
      char * lhs = parse_expression().clone();
      eat(",");
      expect(TT_Expression,next_token_type(false));
      String rhs = parse_expression();
      maf.add_axiom(lhs,rhs,aa_flags,*this);
      delete [] lhs;
      eat("]");
    }

    void parse_levels()
    {
      Ordinal nr_generators = maf.alphabet.letter_count();
      eat("[");
      expect(TT_Natural,next_token_type(false));
      unsigned level = parse_natural();
      maf.real_alphabet.set_level(0,level);
      for (Ordinal i = 1; i < nr_generators;i++)
      {
        eat(",");
        expect(TT_Natural,next_token_type(false));
        level = parse_natural();
        maf.real_alphabet.set_level(i,level);
      }
      eat("]");
    }

    void parse_weights()
    {
      Ordinal nr_generators = maf.alphabet.letter_count();
      eat("[");
      expect(TT_Natural,next_token_type(false));
      unsigned weight = parse_natural();
      if (weight == 0)
        parse_error("Weights must be positive integers");
      maf.real_alphabet.set_weight(0,weight);
      for (Ordinal i = 1; i < nr_generators;i++)
      {
        eat(",");
        expect(TT_Natural,next_token_type(false));
        weight = parse_natural();
        maf.real_alphabet.set_weight(i,weight);
      }
      eat("]");
    }

    void parse_maxstoredlen(bool use)
    {
      eat("[");
      expect(TT_Natural,next_token_type(false));
      if (use)
        maf.options.max_stored_length[0] = parse_natural();
      else
        parse_natural();
      eat(",");
      expect(TT_Natural,next_token_type(false));
      if (use)
        maf.options.max_stored_length[1] = parse_natural();
      else
        parse_natural();
      eat("]");
      if (!use)
        maf.container.error_output("Ignoring maxstoredlen since values which "
                                   "work well with KBMAG may work badly\n"
                                   "in MAF. Use maf_maxstoredlen to set"
                                   " this in MAF\n");

    }

};

class Subalgebra_Reader :  public GAP_Reader
{
  public:
    Subalgebra_Descriptor &sub;
    Subalgebra_Reader(Input_Stream * input_,Subalgebra_Descriptor &sub_) :
      GAP_Reader(input_,sub_.maf.container),
      sub(sub_)
    {}

    void parse_sub()
    {
      static String const expected_words[] =
      {
        "subGenerators",
        "subGeneratorNames",
        "subGeneratorInverseNames",
        "normalSubGenerators",
        0
      };
      const int count = sizeof(expected_words)/sizeof(expected_words[0]);
      bool found[count];
      int i;
      Token_Type token_type;
      Container & container = sub.maf.container;

      sub.name = parse_pair(&token_type).clone();
      expect(TT_Rec,token_type);
      eat("(");
      for (i = 0;i < count;i++)
        found[i] = 0;
      for (;;)
      {
        int i = parse_member(expected_words,&token_type);
        if (i != -1)
        {
          if (found[i])
            parse_error("Value already specified for ",expected_words[i]);
          found[i] = true;
        }
        switch (i)
        {
          case -1:
            container.error_output("Ignoring unknown member %s\n",identifier[0]);
            parse_unknown_value(token_type);
            break;
          case 0:
          case 3:
            if (found[0] && found[3])
              parse_error("Subgroup generators must specified using one, not"
                          " both, of normalSubGenerators\nand subGenerators\n");
            expect(TT_List,token_type);
            parse_subgroup_generators();
            break;

          case 1:
            if (found[3])
              parse_error("If subgroup generators are specified using keyword"
                          " normalSubGenerators they\nmust be unnamed\n");
            expect(TT_List,token_type);
            sub.alphabet = Alphabet::create(AT_String,container);
            sub.alphabet->attach();
            sub.alphabet->set_letters(parse_list_as_text());
            if (sub.alphabet->letter_count() != sub.generator_count())
              container.input_error("Number of named subgroup generators (%d)"
                                    " does not match number of generating words"
                                    " (" FMT_ID ")\n",
                                    sub.alphabet->letter_count(),
                                    sub.generator_count());
            break;
          case 2:
            expect(TT_List,token_type);
            {
              if (!found[1])
                parse_error("subGeneratorNames must be specified before "
                            "subGeneratorInverseNames");
              Ordinal g = 0;
              Ordinal nr_generators = sub.alphabet->letter_count();
              expect(TT_List,token_type);
              const Letter * inverse_string = parse_list_as_text();
              sub.inverses = new Ordinal[nr_generators];
              for (g = 0;g < nr_generators;g++)
                sub.inverses[g] = INVALID_SYMBOL;
              for (g = 0;g < nr_generators;g++)
              {
                if (*inverse_string == ' ')
                  inverse_string++;
                if (is_initial(*inverse_string))
                {
                  Ordinal ig = sub.alphabet->parse_identifier(inverse_string,true);
                  if (ig >= 0)
                    sub.inverses[g] = ig;
                  else
                    parse_error("Unknown generator specified as inverse of ",sub.alphabet->glyph(g));
                  if (*inverse_string==' ')
                    inverse_string++;
                }
                if (*inverse_string == ',')
                  inverse_string++;
                if (!*inverse_string)
                  break;
              }
            }
            break;
        }
        if (!skip_comma())
          break;
      }
      eat(")");
      eat(";");
      if (!found[0] && !found[3])
        parse_error("subGenerators or normalSubGenerators field is required\n");

      sub.is_normal = found[3];
    }
    void parse_subgroup_generators()
    {
      Token_Type tt;
      eat("[");
      for (;;)
      {
        tt = next_token_type(false);
        if (tt == TT_End_Of_List)
          break;
        expect(TT_Expression,next_token_type(false));
        String lhs = parse_expression();
        sub.add_generator(lhs);
        if (!skip_comma())
          break;
      }
      eat("]");
    }
};

class Words_Reader : public GAP_Reader
{
  public:
    const MAF & maf;
    Words_Reader(Input_Stream * input_,const MAF &maf_) :
      GAP_Reader(input_,maf_.container),
      maf(maf_)
    {}

    void parse_words(Word_List * wl)
    {
      Token_Type tt;

      parse_pair(&tt,true);
      expect(TT_List,tt);
      eat("[");
      wl->empty();
      tt = next_token_type(false);
      if (tt != TT_End_Of_List)
        for (;;)
        {
          expect(TT_Expression,next_token_type(false));
          String expression = parse_expression();
          Ordinal_Word * word = maf.parse(expression,WHOLE_STRING);
          wl->add(*word);
          delete word;
          if (!skip_comma())
            break;
        }
      eat("]");
      eat(";");
    }
};

enum GAP_List_Format
{
  LF_Dense,
  LF_Sparse
};

enum GAP_Table_Format
{
  TF_Sparse,
  TF_Dense_Deterministic,
  TF_Dense_Nondeterministic
};


/* The FSR class is used for reading "Finite Set Records" in a generic way.
   This is currently implemented in the FSA_Reader class. It is not clear
   whether this is the right place. It is very likely that other GAP records
   use FSRs as well, and possibly at some point we might want to read them. In
   that case all that needs to be done is to move parse_fsr() and the functions
   it needs into the generic GAP_Reader class.
*/
class FSR
{
  public:
    FSR_Type type;
    State_Count size;
    unsigned arity;
    GAP_List_Format format;
    Byte_Buffer * names;
    FSR * base;
    FSR * labels;
    Label_ID *set_to_labels;
    Alphabet * alphabet;

    FSR() :
      type(FSR_Simple),
      size(0),
      names(0),
      arity(1),
      format(LF_Dense),
      base(0),
      labels(0),
      set_to_labels(0),
      alphabet(0)
    {}
    ~FSR()
    {
      if (base)
        delete base;
      if (labels)
        delete labels;
      if (alphabet)
        alphabet->detach();
      if (set_to_labels)
        delete [] set_to_labels;
      if (names)
        delete [] names;
    }
};

class GAP_FSA
{
  public:
    Owned_String name;
    unsigned flags;
    FSR alphabet;
    FSR states;
    Validated_State_List initial;
    Validated_State_List accepting;
    State_Count transition_count;
    GAP_Table_Format table_format;
    GAP_FSA() :
      flags(0),
      table_format(TF_Sparse)
    {}
};

class FSA_Reader : public GAP_Reader
{
  public:
    FSA_Reader(Input_Stream * input_,Container &container_) :
      GAP_Reader(input_,container_)
    {}

    const Alphabet * select_alphabet(const Alphabet *a,MAF * maf)
    {
      if (maf)
        if (*a == maf->alphabet)
          a = &maf->alphabet;
        else if (*a == maf->group_alphabet())
          a = &maf->group_alphabet();
      return a;
    }
    FSA_Simple * parse_fsa(GAP_FSA & gap_fsa,MAF * maf)
    {
      static String const expected_words[] =
      {
        "isFSA",
        "alphabet",
        "states",
        "flags",
        "initial",
        "accepting",
        "table",
        0
      };

      Token_Type token_type;
      FSA_Simple * fsa = 0;
      gap_fsa.name = parse_pair(&token_type,true);

      expect(TT_Rec,token_type);
      const int count = sizeof(expected_words)/sizeof(expected_words[0]);
      bool found[count];
      int i;

      eat("(");
      for (i = 0;i < count;i++)
        found[i] = 0;
      for (;;)
      {
        int member = parse_member(expected_words,&token_type);
        if (member != -1)
        {
          if (found[member])
            parse_error("Value already specified for ",expected_words[member]);
          found[member] = true;
          if (!found[0])
            parse_error("First member of FSA record must be isFSA := true");
        }
        switch (member)
        {
          case -1:
            container.error_output("Ignoring unknown member %s\n",identifier[0]);
            parse_unknown_value(token_type);
            break;
          case 0:
            expect(TT_True,token_type);
            break;
          case 1:
            expect(TT_Rec,token_type);
            parse_fsr(gap_fsa.alphabet,true);
            break;
          case 2:
            expect(TT_Rec,token_type);
            parse_fsr(gap_fsa.states,false);
            gap_fsa.initial.set_valid_range(1,gap_fsa.states.size);
            gap_fsa.accepting.set_valid_range(1,gap_fsa.states.size);
            break;
          case 3:
            expect(TT_List,token_type);
            gap_fsa.flags = 0;
            eat("[");
            for (;;)
            {
              token_type = next_token_type(false);
              if (token_type == TT_End_Of_List)
                break;
              expect(TT_String,token_type);
              int i = enum_value(FSA::flag_names,parse_string());
              gap_fsa.flags |= (1 << i);
              if (!skip_comma())
                break;
            }
            eat("]");
            break;
          case 4: // initial states
          case 5: // accept states
            if (!found[2])
              parse_error("States record must be specified before initial or accepting");
            expect(TT_List,token_type);
            eat("[");
            for (;;)
            {
              token_type = next_token_type(false);
              if (token_type == TT_End_Of_List)
                break;
              expect(TT_Natural,token_type);
              unsigned long low = parse_natural();
              unsigned long high = low;
              token_type = next_token_type(false);
              if (token_type == TT_Range)
              {
                eat("..");
                token_type = next_token_type(false);
                expect(TT_Natural,token_type);
                high = parse_natural();
              }
              if (member == 4)
                gap_fsa.initial.append_range(low,high);
              else
                gap_fsa.accepting.append_range(low,high);
              if (!skip_comma())
                break;
            }
            eat("]");
            break;
          case 6: // table
            expect(TT_Rec,token_type);
            {
              const Alphabet *a = gap_fsa.alphabet.arity==2 ?
                                            gap_fsa.alphabet.base->alphabet :
                                            gap_fsa.alphabet.alphabet;
              a = select_alphabet(a,maf);
              fsa = new FSA_Simple(container,*a,
                                   gap_fsa.states.size+1,gap_fsa.alphabet.size);
            }
            fsa->set_name(gap_fsa.name);
            fsa->change_flags(gap_fsa.flags);
            if (gap_fsa.accepting.count())
            {
              if (!gap_fsa.accepting.full())
              {
                State_List::Iterator sli(gap_fsa.accepting);
                if (gap_fsa.accepting.count()==1)
                  fsa->set_single_accepting(sli.first());
                else
                {
                  fsa->clear_accepting(gap_fsa.accepting.count()*sizeof(State_ID) > (size_t) fsa->state_count()/CHAR_BIT);
                  for (State_ID si = sli.first(); si;si = sli.next())
                    fsa->set_is_accepting(si,true);
                }
              }
            }
            else
              fsa->clear_accepting(false);

            State_List::Iterator sli(gap_fsa.initial);
            if (gap_fsa.initial.full())
              fsa->set_initial_all();
            else if (gap_fsa.initial.count() != 1 ||
                gap_fsa.states.size != 1 && sli.first() != 1)
            {
              fsa->clear_initial(gap_fsa.initial.count()*sizeof(State_ID) > (size_t) fsa->state_count()/CHAR_BIT);
              for (State_ID si = sli.first(); si;si = sli.next())
                fsa->set_is_initial(si,true);
            }

            if (gap_fsa.states.type == FSR_Labelled)
            {
              FSR * labels = gap_fsa.states.labels;
              if ((int) labels->type <= (int) LT_Last_Supported)
                fsa->set_label_type((Label_Type) labels->type);
              else
                parse_error("Type \"%s\" is not supported for labels",
                            FSA::state_types[labels->type].string());
              if (labels->alphabet)
              {
                fsa->set_label_alphabet(*select_alphabet(labels->alphabet,maf));
              }
              fsa->set_nr_labels(labels->size+1,LA_Mapped);
              if (labels->type != FSR_Simple)
                for (Label_ID i = 1; i <= labels->size;i++)
                  fsa->set_label_data(i,labels->names[i].take());
              for (State_ID i = 1; i <= gap_fsa.states.size;i++)
                fsa->set_label_nr(i,gap_fsa.states.set_to_labels[i]);
            }
            else if (gap_fsa.states.type != FSR_Simple &&
                     (int) gap_fsa.states.type <= (int) LT_Last_Supported)
            {
              fsa->set_label_type((Label_Type) gap_fsa.states.type);
              if (gap_fsa.states.alphabet)
                fsa->set_label_alphabet(*select_alphabet(gap_fsa.states.alphabet,maf));
              fsa->set_nr_labels(fsa->state_count(),LA_Direct);
              for (State_ID i = 1; i <= gap_fsa.states.size;i++)
                fsa->set_label_data(i,gap_fsa.states.names[i].take());
            }
            parse_table(gap_fsa,fsa);
            break;
        }
        if (!skip_comma())
          break;
      }
      if (!found[0] || !found[6])
        parse_error("Expected data not found: file does not contain an FSA");
      eat(")");
      eat(";");
      fsa->tidy();
      return fsa;
    }

    void parse_fsr(FSR & fsr,bool identifiers_are_alphabet)
    {
      /* FSRs are GAP/KBMAG's way of assigning meaning to the
         states and transitions of FSAs. The definition is
         recursive. */

      static String const expected_words[] =
      {
        "type",
        "size",
        "format",
        "names",
        "alphabet",
        "arity",
        "base",
        "labels",
        "setToLabels",
        "padding",
        0
      };

      static String const formats[] =
      {
        "dense",
        "sparse",
        0
      };

      const int count = sizeof(expected_words)/sizeof(expected_words[0]);
      bool found[count];
      Token_Type token_type;
      int i;

      for (i = 0;i < count;i++)
        found[i] = 0;
      eat("(");
      for (;;)
      {
        int member = parse_member(expected_words,&token_type);
        if (member != -1)
        {
          if (found[member])
            parse_error("Value already specified for ",expected_words[member]);
          found[member] = true;
          if (!found[0])
            parse_error("First member of FSR must specify type");
          if (member > 0 && !found[1])
            parse_error("Second member of FSR must specify size");
          if (member >= 2 && fsr.type == FSR_Simple)
            parse_error("Simple FSR should specify type and size only");
        }
        switch (member)
        {
          case -1:
            container.error_output("Ignoring unknown member %s\n",
                                   identifier[0]);
            // fall through
          case 9: /* padding - we don't care about this */
            parse_unknown_value(token_type);
            break;
          case 0: /* type */
            expect(TT_String,token_type);
            {
              String s = parse_string();
              int i = enum_value(FSA::state_types,s);
              if (i == -1)
                parse_error("Unrecognised record format:",s);
              fsr.type = (FSR_Type) i;
              if (fsr.type == FSR_Labeled)
                fsr.type = FSR_Labelled;
            }
            break;
          case 1: /* size */
            expect(TT_Natural,token_type);
            fsr.size = parse_natural();
            break;
          case 2: /* format */
            expect(TT_String,token_type);
            fsr.format = (GAP_List_Format) enum_value(formats,parse_string());
            break;
          case 3: /* names */
            expect(TT_List,token_type);
            if (identifiers_are_alphabet && fsr.type == FSR_Identifiers)
            {
              fsr.alphabet = Alphabet::create(AT_String,container);
              fsr.alphabet->attach();
              fsr.alphabet->set_letters(parse_list_as_text());
            }
            else
              parse_names(fsr);
            break;
          case 4: /* alphabet */
            fsr.alphabet = Alphabet::create(AT_String,container);
            fsr.alphabet->attach();
            fsr.alphabet->set_letters(parse_list_as_text());
            break;
          case 5: /* arity */
            if (fsr.type != FSR_Product)
              parse_error("arity only applies to product FSRs");
            expect(TT_Natural,token_type);
            fsr.arity = parse_natural();
            break;
          case 6: /* base */
            expect(TT_Rec,token_type);
            fsr.base = new FSR;
            parse_fsr(*fsr.base,true);
            break;
          case 7: /* labels */
            expect(TT_Rec,token_type);
            fsr.labels = new FSR;
            parse_fsr(*fsr.labels,false);
            break;
          case 8: /* setToLabels */
            if (fsr.type != FSR_Labelled)
              parse_error("setToLabels only applies to labelled FSRs");
            parse_set_to_labels(fsr);
            break;
        }
        if (!skip_comma())
          break;
      }
      eat(")");
      if (fsr.type == FSR_Labelled && !found[8])
        parse_error("setToLabels must be specified for labelled FSR");
    }


    void parse_names(FSR & fsr)
    {
      Token_Type tt;
      eat("[");
      if (fsr.type == FSR_Words || fsr.type == FSR_Strings ||
          fsr.type == FSR_Identifiers || fsr.type == FSR_List_Of_Integers ||
          fsr.type == FSR_List_Of_Words)
        fsr.names = new Byte_Buffer[fsr.size+1];
      if (fsr.format == LF_Dense)
      {
        Element_ID id = 0;
        for (;;)
        {
          tt = next_token_type(false);
          if (tt == TT_End_Of_List)
            break;
          ++id;
          parse_one_name(fsr,id);
          if (!skip_comma())
            break;
        }
      }
      else
      {
        for (;;)
        {
          tt = next_token_type(false);
          if (tt == TT_End_Of_List)
            break;
          expect(TT_List,tt);
          eat("[");
          expect(TT_Natural,next_token_type(false));
          Element_ID id = parse_natural();
          eat(",");
          parse_one_name(fsr,id);
          eat("]");
          if (!skip_comma())
            break;
        }
      }
      eat("]");
    }

    void parse_one_name(FSR & fsr,Element_ID id)
    {
      switch (fsr.type)
      {
        case FSR_Identifiers:
          fsr.names[id] = parse_identifier(1,true).clone();
          break;
        case FSR_Strings:
          fsr.names[id] = parse_string().clone();
          break;
        case FSR_Words:
          {
            expect(TT_Expression,next_token_type(false));
            String expression = parse_expression();
            Ordinal_Word * word = fsr.alphabet->parse(expression,WHOLE_STRING,0,*this);
            fsr.names[id] = word->extra_packed_data();
            delete word;
            break;
          }
        case FSR_List_Of_Words:
          {
            expect(TT_List,next_token_type(false));
            eat("[");
            Word_List words(*fsr.alphabet);
            Token_Type tt = next_token_type(false);
            if (tt != TT_End_Of_List)
              for (;;)
              {
                expect(TT_Expression,next_token_type(false));
                String expression = parse_expression();
                Ordinal_Word * word = fsr.alphabet->parse(expression,WHOLE_STRING,0,*this);
                words.add(*word);
                delete word;
                if (!skip_comma())
                  break;
              }
            eat("]");
            fsr.names[id] = words.packed_data();
          }
          break;
        case FSR_List_Of_Integers:
          {
            expect(TT_List,next_token_type(false));
            eat("[");
            State_List numbers;
            Token_Type tt = next_token_type(false);
            if (tt != TT_End_Of_List)
              for (;;)
              {
                expect(TT_Natural,next_token_type(false));
                Element_ID value = parse_natural();
                numbers.append_one(value);
                if (!skip_comma())
                  break;
              }
            eat("]");
            fsr.names[id] = numbers.packed_data();
          }
          break;

        case FSR_Simple:
        case FSR_Labelled:
        case FSR_Labeled:
        case FSR_Product:
          MAF_INTERNAL_ERROR(container,
                             ("Unexpected FSR type %d in "
                              "FSA_Reader::parse_one_name()\n",fsr.type));
          break;
      }
    }

    void parse_set_to_labels(FSR & fsr)
    {
      Token_Type tt;
      fsr.set_to_labels = new Label_ID[fsr.size+1];
      for (State_Count i = 0; i <= fsr.size;i++)
        fsr.set_to_labels[i] = 0;
      eat("[");
      for (;;)
      {
        tt = next_token_type(false);
        if (tt == TT_End_Of_List)
          break;
        expect(TT_List,tt);
        parse_one_set_to_label(fsr);
        if (!skip_comma())
          break;
      }
      eat("]");
    }

    void parse_one_set_to_label(FSR & fsr)
    {
      eat("[");
      expect(TT_Natural,next_token_type(false));
      State_ID state_number = parse_natural();
      if (state_number <= 0 || state_number > fsr.size)
        parse_error("State number in setToLabels is too large");
      if (fsr.set_to_labels[state_number] != 0)
        parse_error("Label already specified for this state");
      eat(",");
      expect(TT_Natural,next_token_type(false));
      Label_ID label_number = parse_natural();
      if (label_number <= 0 || label_number > fsr.labels->size)
        parse_error("Label number in setToLabels is too large");
      fsr.set_to_labels[state_number] = label_number;
      eat("]");
    }


    void parse_table(GAP_FSA & gap_fsa,FSA_Simple * fsa)
    {
      static String const expected_words[] =
      {
        "format",
        "numTransitions",
        "transitions",
        0
      };

      static String const formats[] =
      {
        "sparse",
        "dense deterministic",
        "dense nondeterministic",
        0
      };

      const int count = sizeof(expected_words)/sizeof(expected_words[0]);
      bool found[count];
      Token_Type token_type;
      int i;

      for (i = 0;i < count;i++)
        found[i] = 0;
      eat("(");
      for (;;)
      {
        int i = parse_member(expected_words,&token_type);
        if (i != -1)
        {
          if (found[i])
            parse_error("Value already specified for ",expected_words[i]);
          found[i] = true;
          if (!found[0])
            parse_error("First member of table must specify format");
        }
        switch (i)
        {
          case -1:
            container.error_output("Ignoring unknown member %s\n",
                                   identifier[0]);
            parse_unknown_value(token_type);
            break;
          case 0: /* format */
            expect(TT_String,token_type);
            gap_fsa.table_format = (GAP_Table_Format) enum_value(formats,parse_string());
            if (gap_fsa.table_format == TF_Dense_Nondeterministic)
              parse_error("Sorry. MAF does not support non deterministic FSAs\n");
            break;
          case 1: /* numTransitions */
            expect(TT_Natural,token_type);
            gap_fsa.transition_count = parse_natural();
            break;
          case 2: /* transitions */
            expect(TT_List,token_type);
            parse_transitions(gap_fsa,fsa);
            break;
        }
        if (!skip_comma())
          break;
      }
      eat(")");
    }
    void parse_transitions(GAP_FSA & gap_fsa,FSA_Simple * fsa)
    {
      Token_Type tt;
      eat("[");
      State_ID s = 1;
      for (;;)
      {
        tt = next_token_type(false);
        if (tt == TT_End_Of_List)
          break;
        expect(TT_List,tt);
        State_ID * transitions = fsa->state_lock(s);
        parse_one_state(transitions,gap_fsa.alphabet.size,
                        gap_fsa.table_format,(gap_fsa.flags & GFF_RWS)!=0);
        fsa->state_unlock(s++);
        if (!skip_comma())
          break;
      }
      eat("]");
    }

    void parse_one_state(State_ID * transition,Transition_ID alphabet_size,
                         GAP_Table_Format table_format,bool is_rws)
    {
      eat("[");
      Token_Type token_type = next_token_type(false);
      if (is_rws && (token_type == TT_Integer || token_type == TT_Natural))
      {
        transition[0] = parse_integer();
        for (Transition_ID ti = 1; ti < alphabet_size;ti++)
        {
          eat(",");
          expect(TT_Integer,next_token_type(false));
          transition[ti] = parse_integer();
        }
      }
      else if (token_type == TT_Natural)
      {
        transition[0] = parse_natural();
        for (Transition_ID ti = 1; ti < alphabet_size;ti++)
        {
          eat(",");
          expect(TT_Integer,next_token_type(false));
          transition[ti] = parse_natural();
        }
      }
      else if (token_type == TT_List)
      {
        if (table_format == TF_Dense_Deterministic)
          container.error_output("Sparse format state encountered in "
                                     "dense table!\nOther programs might not"
                                     " cope with this.\n");
        memset(transition,0,sizeof(State_ID)*alphabet_size);
        for (;;)
        {
          eat("[");
          expect(TT_Natural,next_token_type(false));
          Transition_ID ti = parse_natural();
          if (ti < 1 || ti > alphabet_size)
          {
            String_Buffer sb;
            sb.format(FMT_TID,ti);
            parse_error("Invalid transition index\n",sb.get().string());
          }
          eat(",");
          expect(is_rws ? TT_Integer : TT_Natural,next_token_type(false));
          transition[ti-1] = parse_integer();
          eat("]");
          if (!skip_comma())
            break;
        }
      }
      else if (token_type == TT_End_Of_List)
      {
        if (table_format == TF_Dense_Deterministic)
          container.error_output("Empty sparse format state encountered in "
                                     "dense table!\nOther programs might not"
                                     " cope with this.\n");
      }
      else
        expect(table_format == TF_Dense_Deterministic ?
               (is_rws ? TT_Integer : TT_Natural) : TT_List,token_type);
      eat("]");
    }


};

/**/

Subalgebra_Descriptor * Subalgebra_Descriptor::create(String filename,
                                                      MAF & maf)
{
  Subalgebra_Descriptor & sub = *new Subalgebra_Descriptor(maf);
  sub.filename = filename.clone();
  Input_Stream *stream = maf.container.open_input_file(sub.filename);
  Subalgebra_Reader reader(stream,sub);
  reader.parse_sub();
  maf.container.close_input_file(stream);
  return &sub;
}

/**/

MAF * MAF::create_from_rws(String filename,Container * container,unsigned flags,const Options * options)
{
  MAF & maf = *create(container,options);
  container = &maf.container;
  maf.filename = filename.clone();
  bool ok = false;
  String_Buffer sb;
  /* If the user is trying to open a .pbprog or a .kbprog file directly
     then strip the suffix out of the name, and treat the open as a restart */
  String suffix = filename.suffix();
  if (!(flags & CFI_RAW))
    if (strcmp(suffix,".kbprog")==0 || strcmp(suffix,".pkbprog")==0)
    {
      maf.filename = sb.make_filename("",filename,"",false,String::MFSF_Replace).clone();
      flags |= CFI_RESUME;
    }

  String open_filename = maf.filename;
  int pass = 0;
  for (;;)
  {
    unsigned open_flags = flags & CFI_ALLOW_CREATE || pass!=0 ? 0 :
                                            OIF_MUST_SUCCEED|OIF_REPORT_ERROR;
    Input_Stream *stream = container->open_input_file(open_filename,open_flags);
    if (stream)
    {
      RWS_Reader reader(stream,maf);
      reader.parse_rws(pass == 0 ? flags & CFI_CREATE_RM : flags);
      container->close_input_file(stream);
      ok = true;
    }
    if (!(flags & CFI_RESUME))
      break;
    if (pass == 0)
    {
      open_filename = sb.make_filename("",maf.filename,".pkbprog");
      pass = 1;
    }
    else if (pass == 1 && !stream)
    {
      open_filename = sb.make_filename("",maf.filename,".kbprog");
      pass = 2;
    }
    else
      break;
  }
  if (ok)
  {
    if (flags & CFI_CREATE_RM)
      maf.realise_rm(); /* We do this in case there were no axioms at all */
    return &maf;
  }
  delete &maf;
  return 0;
}

/**/

FSA_Simple * FSA_Factory::create(String filename,Container * container,
                                 bool must_succeed,MAF * maf)
{
  GAP_FSA gap_fsa;
  unsigned flags = (must_succeed ? OIF_MUST_SUCCEED+OIF_REPORT_ERROR : 0) |
                   OIF_NULL_IS_STDIN;

  Input_Stream *stream = container->open_input_file(filename,flags);
  if (stream)
  {
    FSA_Reader reader(stream,*container);
    FSA_Simple * fsa = reader.parse_fsa(gap_fsa,maf);
    container->close_input_file(stream);
    return fsa;
  }
  return 0;
}

/**/

void MAF::read_word_list(Word_List *wl,String filename) const
{
  wl->empty();
  Input_Stream *stream = container.open_input_file(filename);
  Words_Reader reader(stream,*this);
  reader.parse_words(wl);
  container.close_input_file(stream);
}
