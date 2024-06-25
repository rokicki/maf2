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


// $Log: mafload.cpp $
// Revision 1.9  2010/06/14 22:46:57Z  Alun
// delete_fsa() bug fixed
// Revision 1.8  2010/06/10 13:57:32Z  Alun
// All tabs removed again
// Revision 1.7  2010/05/16 23:46:13Z  Alun
// Various new automata added to GAT enumeration
// Revision 1.6  2009/10/13 20:47:11Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.5  2009/09/12 18:47:44Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2008/08/22 07:32:50Z  Alun
// "Early Sep 2008 snapshot"
// Revision 1.3  2007/11/15 22:58:08Z  Alun
//

#include "container.h"
#include "maf_rws.h"
#include "maf.h"

String const FSA_Buffer::suffixes[] =
{
  ".reduce",
  ".fastreduce",
  ".preduce",
  ".gm",
  ".gm",
  ".gm2",
  ".gm2",
  ".wa",
  ".cosets",
  ".cclasses",
  ".conjugator",
  ".diff2",
  ".minred",
  ".minkb",
  ".diff1c",
  ".maxkb",
  ".rr",
  ".diff2c",
  ".pdiff1",
  ".pdiff2",
  0
};

String const FSA_Buffer::coset_suffixes[] =
{
  ".reduce",
  ".fastreduce",
  ".preduce",
  ".migm",
  ".gm",
  ".migm2",
  ".gm2",
  ".wa",
  ".cosets",
  ".conjugator",
  ".cclasses",
  ".midiff2",
  ".minred",
  ".minkb",
  ".midiff1c",
  ".maxkb",
  ".rr",
  ".midiff2c",
  ".pmidiff1",
  ".pmidiff2",
  ".wa",
  0
};

String const FSA_Buffer::rws_suffixes[] =
{
  ".kbprog",
  ".fastkbprog",
  ".pkbprog"
};

FSA_Buffer::FSA_Buffer() :
      min_rws(0),
      fast_rws(0),
      provisional_rws(0),
      gm(0),
      dgm(0),
      gm2(0),
      dgm2(0),
      wa(0),
      cosets(0),
      cclass(0),
      conjugator(0),
      diff2(0),
      minred(0),
      minkb(0),
      maxkb(0),
      rr(0),
      diff1c(0),
      diff2c(0),
      subwa(0),
      pdiff1(0),
      pdiff2(0)
{}

FSA_Buffer::~FSA_Buffer()
{
  FSA **first = (FSA **) &min_rws;
  FSA **last = (FSA **) &subwa;
  for (FSA ** fsa_ptr = first; fsa_ptr <= last; fsa_ptr++)
    if (*fsa_ptr)
      delete *fsa_ptr;
}

static Rewriting_System * load_rws(Container * container,
                                   Rewriting_System ** rws_ptr,
                                   String basename,
                                   String fsa_suffix,String kb_suffix)
{
  if (!*rws_ptr)
  {
    String_Buffer sb1;
    String kb_filename = sb1.make_filename("",basename,kb_suffix);
    String_Buffer sb2;
    String fsa_filename = sb2.make_filename("",basename,fsa_suffix);
    *rws_ptr = Rewriting_System::create(kb_filename,fsa_filename,container,false);
  }
  return *rws_ptr;
}

/**/

void MAF::delete_fsa(Group_Automaton_Type ga_type)
{
  String_Buffer sb;
  String const *suffix_set = is_coset_system ? FSA_Buffer::coset_suffixes : FSA_Buffer::suffixes;
  String filename;
  if (is_coset_system && ga_type == GAT_Subgroup_Word_Acceptor)
    filename = sb.make_filename("",original_filename,suffix_set[ga_type]);
  else
    filename = sb.make_filename("",this->filename,suffix_set[ga_type]);
  container.delete_file(filename);
  if (ga_type <= GAT_Provisional_RWS)
  {
    filename = sb.make_filename("",this->filename,FSA_Buffer::rws_suffixes[ga_type]);
    container.delete_file(filename);
  }
}

/**/

static FSA_Simple * load_fsa(Container * container,FSA_Simple **fsa_ptr,
                             String basename,String suffix,MAF * maf)
{
  if (!*fsa_ptr)
  {
    String_Buffer sb;
    String filename = sb.make_filename("",basename,suffix);
    *fsa_ptr = FSA_Factory::create(filename,container,false,maf);
  }
  return *fsa_ptr;
}

static General_Multiplier * load_multiplier(Container * container,
                                            General_Multiplier **gm_ptr,
                                            String basename,String suffix,
                                            MAF * maf)
{
  if (!*gm_ptr)
  {
    FSA_Simple * gm_fsa = 0;
    load_fsa(container,&gm_fsa,basename,suffix,maf);
    if (gm_fsa)
    {
      *gm_ptr = new General_Multiplier(*gm_fsa,true);
      if (!(*gm_ptr)->fsa())
      {
        delete *gm_ptr;
        *gm_ptr = 0;
      }
    }
  }
  return *gm_ptr;
}

/**/

FSA * FSA_Buffer::load(Container * container,String filename,
                       unsigned ga_flags,bool coset_system,MAF * maf)
{
  unsigned i;
  Rewriting_System ** rws_ptr = &min_rws;
  General_Multiplier ** gm_ptr = &gm;
  FSA_Simple ** fsa_ptr = &wa;
  FSA * answer = 0;
  String const *suffix_set = coset_system ? coset_suffixes : suffixes;
  for (i = GAT_Minimal_RWS; i <= GAT_Provisional_RWS; i++,rws_ptr++)
    if (ga_flags & (1 << i))
      answer = load_rws(container,rws_ptr,filename,suffix_set[i],rws_suffixes[i]);
  for (i = GAT_General_Multiplier;i <= GAT_DGM2; i++,gm_ptr++)
    if (ga_flags & (1 << i))
      answer = load_multiplier(container,gm_ptr,filename,suffix_set[i],maf);
  for (; i < GAT_Subgroup_Word_Acceptor; i++,fsa_ptr++)
    if (ga_flags & (1 << i))
      answer = load_fsa(container,fsa_ptr,filename,suffix_set[i],maf);
  if (ga_flags & (1 << i) && coset_system)
      answer = load_fsa(container,fsa_ptr,
                        maf->properties().original_filename,suffix_set[i],maf);
  return answer;
}
