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
$Log: maf_sub.h $
Revision 1.7  2010/06/17 11:13:37Z  Alun
Changes to allow better management of filenames for coset systems
Revision 1.6  2010/05/18 11:07:14Z  Alun
Lots of methods added to facilitate construction of substructure files by
utilities such as gpsublowindex and gpsubmake
Revision 1.5  2009/09/13 09:05:07Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.4  2008/12/24 17:20:46Z  Alun
Support for normal subgroup coset systems added
Revision 1.3  2008/11/03 01:04:25Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.2  2008/09/30 06:57:32Z  Alun
"Aug 2008 snapshot"
*/
#pragma once
#ifndef MAF_SUB_INCLUDED
#define MAF_SUB_INCLUDED 1

#ifndef MAFWORD_INCLUDED
#include "mafword.h"
#endif

#ifndef MAF_INCLUDED
#include "maf.h"
#endif

class Subalgebra_Descriptor
{
  friend class Subalgebra_Reader;
  public:
    MAF & maf; /*the MAF object of which this describes a subgroup/submonoid*/
  private:
    Owned_String filename;   /* The name of the file the object was read from */
    Owned_String name;       /* The internal name of the object */
    Word_List generators;/* A list of words in the alphabet of the MAF object
                            which is a generating set for the subalgebra */
    Alphabet * alphabet;/* The alphabet object that describes the names given
                           to the generators if these have been named */
    Ordinal * inverses;/* Any inverse pairs amongst these generators */
    bool is_normal;
  public:
    Subalgebra_Descriptor(MAF & maf_,bool is_normal_ = false) :
      maf(maf_),
      generators(maf_.alphabet),
      alphabet(0),
      inverses(0),
      is_normal(is_normal_)
    {}
    APIMETHOD ~Subalgebra_Descriptor()
    {
      if (alphabet)
        alphabet->detach();
      if (inverses)
        delete [] inverses;
    }
    void add_generator(String expression)
    {
      Ordinal_Word * word = maf.parse(expression);
      generators.add(*word);
      delete word;
    }
    void add_generator(const Word &expression)
    {
      generators.add(expression);
    }
    void create_alphabet();
    Element_Count generator_count() const
    {
      return generators.count();
    }
    const Entry_Word sub_generator(Ordinal sg) const
    {
      return Entry_Word(generators,sg);
    }
    /* Do not call add_generator_name until you have made all necessary
       calls to add_generator(). It is not possible to interleave calls
       to add_generator() and add_generator_name() */
    bool add_generator_name(Glyph glyph);
    void set_inverse(Ordinal g,Ordinal ig);
    MAF * create_coset_system(bool name_generators,bool no_inverses);
    static Subalgebra_Descriptor * create(String filename,MAF & maf);
    bool has_named_generators() const
    {
      return alphabet != 0;
    }
    const Alphabet &sub_alphabet() const
    {
      return *alphabet;
    }
    bool is_normal_closure() const
    {
      return is_normal;
    }
    APIMETHOD void print(Output_Stream * stream) const;
    APIMETHOD void save(String filename);
    APIMETHOD void save_as(String basefilename,String suffix)
    {
      String_Buffer sb;
      save(sb.make_filename("",basefilename,suffix));
    }
    String get_filename() const
    {
      return filename.string();
    }
    static Letter * coset_system_suffix(bool * is_subgroup_suffix,String subgroup_suffix);
};

#endif
