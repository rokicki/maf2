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


// $Log: equation.cpp $
// Revision 1.7  2010/06/10 13:56:54Z  Alun
// All tabs removed again
// Revision 1.6  2010/03/23 08:02:25Z  Alun
// June 2010 - changes due to new style Node_Manager interface
// Revision 1.5  2009/09/12 18:47:13Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.4  2008/11/02 17:57:14Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/11/02 18:57:14Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/08/25 06:34:52Z  Alun
// "Early Sep 2008 snapshot"
// Revision 1.2  2007/10/24 21:15:29Z  Alun
//

#include "mafnode.h"
#include "maf_nm.h"
#include "equation.h"
#include "container.h"

/**/

void Equation::print(Container & container,Output_Stream * stream) const
{
  String_Buffer sb1,sb2;

  get_lhs().format(&sb1);
  get_rhs().format(&sb2);
  container.output(stream,"%s=%s\n",sb1.get().string(),sb2.get().string());
}

/**/

Simple_Equation::Simple_Equation(const Node_Manager &nm,
                                 Equation_Handle node) :
  lhs_word(nm.maf.alphabet,lhs_length = node->length(nm)),
  rhs_word(nm.maf.alphabet,rhs_length = node->reduced_length(nm))
{
  node->read_word(&lhs_word,nm);
  node->reduced_node(nm,node)->read_word(&rhs_word,nm);
}

/**/

void Simple_Equation::read_equation(const Node_Manager & nm,Equation_Handle node)
{
  node->read_word(&lhs_word,nm);
  node->reduced_node(nm,node)->read_word(&rhs_word,nm);
  lhs_length = lhs_word.length();
  rhs_length = rhs_word.length();
}

/**/

bool Equation::relator(Ordinal_Word * answer,const Presentation & p) const
{
  const Word & lhs = get_lhs();
  const Word & rhs = get_rhs();
  Word_Length r = rhs.length();
  if (!r)
  {
    *answer = lhs;
    return true;
  }
  if (lhs.length() + r > MAX_WORD)
    return false;
  if (!p.invert(answer,rhs))
  {
    answer->set_length(0);
    return false;
  }
  *answer = lhs + *answer;
  return true;
}
