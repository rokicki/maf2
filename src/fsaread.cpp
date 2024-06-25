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


// $Log: fsaread.cpp $
// Revision 1.5  2010/06/10 13:57:07Z  Alun
// All tabs removed again
// Revision 1.4  2010/05/11 08:31:18Z  Alun
// help changed
// Revision 1.3  2009/09/12 18:47:25Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2008/11/04 08:43:52Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/11/04 09:43:52Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/10/12 18:48:15Z  Alun
// Kludge removed
// Revision 1.2  2008/10/01 01:32:04Z  Alun
// stdio.h removed
// Revision 1.1  2008/07/13 23:14:40Z  Alun
// New file.
// Revision 1.3  2007/12/20 23:25:43Z  Alun
//

#include "fsa.h"
#include "container.h"
#include "maf_so.h"
#include "mafword.h"

/**/

#define cprintf container.error_output

int inner(Container & container,String filename,String word1,String word2)
{
  bool retcode = true;
  FSA & fsa = * FSA_Factory::create(filename,&container);
  Ordinal_Word * ow1 = fsa.base_alphabet.parse(word1,word1.length(),0,container);
  if (fsa.is_product_fsa())
  {
    Ordinal_Word * ow2 = fsa.base_alphabet.parse(word2,word2.length(),0,container);
    State_ID state = fsa.initial_state();
    Word_Length w1_length = ow1->length();
    Word_Length w2_length = ow2->length();
    const Ordinal * lvalues = ow1->buffer();
    const Ordinal * rvalues = ow2->buffer();
    Word_Length m = max(w1_length,w2_length);
    container.progress(1,FMT_ID "\n",state);
    for (Word_Length i = 0;i < m;i++)
    {
      Ordinal g1 = i < w1_length ? lvalues[i] : PADDING_SYMBOL;
      Ordinal g2 = i < w2_length ? rvalues[i] : PADDING_SYMBOL;
      state = fsa.new_state(state,fsa.base_alphabet.product_id(g1,g2),false);
      container.progress(1,"^[%s,%s]=" FMT_ID "\n",
                         fsa.base_alphabet.glyph(g1).string(),
                         fsa.base_alphabet.glyph(g2).string(),state);
      if (!state)
        break;
    }
    retcode = fsa.is_accepting(state);
    delete ow2;
  }
  else
  {
    State_ID state = fsa.initial_state();
    Word_Length w1_length = ow1->length();
    const Ordinal * lvalues = ow1->buffer();
    container.progress(1,FMT_ID "\n",state);
    for (Word_Length i = 0;i < w1_length;i++)
    {
      Ordinal g1 = lvalues[i];
      state = fsa.new_state(state,g1,false);
      container.progress(1,"^%s=" FMT_ID "\n",fsa.base_alphabet.glyph(g1).string(),state);
      if (!state)
        break;
    }
    retcode = fsa.is_accepting(state);
  }
  delete ow1;
  delete &fsa;
  if (retcode)
    container.progress(1,"Input is accepted\n");
  else
    container.progress(1,"Input is not accepted\n");
  return retcode ? 0 : 1;
}

int main(int argc,char ** argv)
{
  Container & container = * Container::create();
  Standard_Options so(container,SO_STDIN);
  String input_file = 0;
  String word1 = 0;
  String word2 = 0;
  int i = 0;
  bool bad_usage = false;

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (input_file == 0)
      input_file = argv[i++];
    else if (word1 == 0)
      word1 = argv[i++];
    else if (word2 == 0)
      word2 = argv[i++];
    else
      bad_usage = true;
  }
  if (input_file && word1 && !bad_usage)
    inner(container,input_file,word1,word2);
  else
    cprintf("Usage: fsaread [loglevel] filename word [word2]\nwhere filename contains a GASP FSA.\n");
  delete &container;
  return 0;
}
