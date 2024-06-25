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


// $Log: example.cpp $
// Revision 1.7  2010/06/10 13:56:55Z  Alun
// All tabs removed again
// Revision 1.6  2009/09/12 18:47:13Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.5  2008/12/09 10:55:38Z  Alun
// Can no longer call set_word_ordering() method of Alphabet without getting a
// writable Alphabet object
// Revision 1.4  2008/10/01 01:37:06Z  Alun
// stdio.h removed
// Revision 1.3  2007/12/20 23:25:43Z  Alun
//

#include <time.h>
#include "maf.h"
#include "mafword.h"
#include "heap.h"
#include "container.h"

/**/

int inner()
{
  time_t now = time(0);
  MAF & maf = * MAF::create(0,0,AT_Char,PT_Rubik);
#if 0
  /* D6 (2,3,2) */
  maf.set_generators("abB");
  maf.set_inverses("aabB");
  maf.add_axiom("bbb","");
  maf.add_axiom("abab","");
#endif

#if 0
  /* A4 (2,3,3) */
  maf.set_generators("abB");
  maf.set_inverses("aabB");
  maf.add_axiom("bbb","");
  maf.add_axiom("ababab","");
#endif

#if 0
  /* S4 (2,3,4)*/
  maf.set_generators("abB");
  maf.set_inverses("aabB");
  maf.add_axiom("bbb","");
  maf.add_axiom("abababab","");
#endif

#if 0
  /* A5 (2,3,5) */
  maf.set_generators("abB");
  maf.set_inverses("aabB");
  maf.add_axiom("bbb","");
  maf.add_axiom("ababababab","");
#endif

#if 0
  /* P6 (2,3,6) */
  maf.set_generators("abB");
  maf.set_inverses("aabB");
  maf.add_axiom("bbb","");
  maf.add_axiom("abababababab","");
#endif

  /* 2,2,2 */
//  maf.set_generators("ab");
//  maf.set_inverses("aabb");
//  maf.add_axiom("abab","");

  /* 2,2,3 */
//  maf.set_generators("ab");
//  maf.set_inverses("aabb");
//  maf.add_axiom("ababab","");

  /* 2,2,4 */
//  maf.set_generators("ab");
//  maf.set_inverses("aabb");
//  maf.add_axiom("abababab","");

  /* D8 2,4,2 */
//  maf.set_generators("abB");
//  maf.set_inverses("aabB");
//  maf.add_axiom("bbbb","");
//  maf.add_axiom("abab","");

  /* S4 2,4,3 */
//  maf.set_generators("abB");
//  maf.set_inverses("aabB");
//  maf.add_axiom("bbbb","");
//  maf.add_axiom("ababab","");

  /* P4 2,4,4 */
//  maf.set_generators("abB");
//  maf.set_inverses("aabB");
//  maf.add_axiom("bbbb","");
//  maf.add_axiom("abababab","");

  /* 3,3,2 */
//  maf.set_generators("aAbB");
//  maf.set_inverses("aAbB");
//  maf.add_axiom("aaa","");
//  maf.add_axiom("bbb","");
//  maf.add_axiom("abab","");

#if 0
  /* 3,3,3 */
  maf.set_generators("aAbB");
  maf.set_inverses("aAbB");
  maf.add_axiom("aaa","");
  maf.add_axiom("bbb","");
  maf.add_axiom("ababab","");
#endif

#if 0
  /* P4MM more intuitive presentation */
  maf.set_generators("xcChHvV");
  maf.set_inverses("xxcChHvV");
  maf.add_axiom("chchchch","");
  maf.add_axiom("cccc","");
  maf.add_axiom("CHCCHC","");
  maf.add_axiom("xCxc","cc");
  maf.add_axiom("ccvccv","");
  maf.add_axiom("vh","hv");
  maf.add_axiom("xh","hx");
  maf.add_axiom("v","cHC");
//  maf.add_axiom("vx","xV");
#endif

  /* P4  more intuitive presentation */
//  maf.set_generators("cChHvV");
//  maf.set_inverses("cChHvV");
//  maf.add_axiom("chchchch","");
//  maf.add_axiom("cccc","");
//  maf.add_axiom("CHCCHC","");
//  maf.add_axiom("ccvccv","");
//  maf.add_axiom("vh","hv");
//  maf.add_axiom("v","cHC");

    /* Trivial group 1 */
//    maf.set_generators("a");
//    maf.add_axiom("a","");

    /* Trivial group 2 */
//    maf.set_generators("aAbB");
//    maf.set_inverses("aAbB");
//    maf.add_axiom("ab","");
//    maf.add_axiom("abb","");

    /* Trivial group 3 */
//    maf.set_generators("aAbBcC");
//    maf.set_inverses("aAbBcC");
//    maf.add_axiom("Aba","bb");
//    maf.add_axiom("Bcb","cc");
//    maf.add_axiom("Cac","aa");

#if 0
 /* Trivial group 4 */
  maf.set_generators("xXyYzZ");
  maf.set_inverses("xXyYzZ");
  maf.add_axiom("yyXYxYzyZZXyxYYzzYZyzzYZy","");
  maf.add_axiom("zzYZyZxzXXYzyZZxxZXzxxZXz","");
  maf.add_axiom("xxZXzXyxYYZxzXXyyXYxyyXYx","");
#endif

#if 0
  /* Trivial group 5 */
  maf.set_generators("rRsStT");
  maf.set_inverses("rRsStT");
  maf.add_axiom("StsTTStsTTssRSrttSTsRsrSSttSTsTrtRRStsTTrrTRtrrTRtssRSrStsTTR"
                "srSSttSTsttSTsTrtRRTrtRRttSTsrrTRtStsTTTrtRRTrtRRttSTsrrTRtSt"
                "sTT","");
  maf.add_axiom("TrtRRTrtRRttSTsrrTRtStsTTrrTRtRsrSSTrtRRssRSrssRSrttSTsTrtRRS"
                "tsTTrrTRtrrTRtRsrSSRsrSSrrTRtssRSrTrtRRRsrSSRsrSSrrTRtssRSrTr"
                "tRR","");
  maf.add_axiom("RsrSSRsrSSrrTRtssRSrTrtRRssRSrStsTTRsrSSttSTsttSTsrrTRtRsrSST"
                "rtRRssRSrssRSrStsTTStsTTssRSrttSTsRsrSSStsTTStsTTssRSrttSTsRs"
                "rSS","");
#endif

  /* F5 Monoid */
#if 0
  maf.set_generators("abcde");
  maf.add_axiom("ab","c");
  maf.add_axiom("bc","d");
  maf.add_axiom("cd","e");
  maf.add_axiom("de","a");
  maf.add_axiom("ea","b");
#endif

#if 0
  /* Rubik */
  maf.set_generators("rRlLtTdDfFbB");
  maf.set_inverses("rRlLtTdDfFbB");
  maf.add_permutation("trrfftlbbddlTRRFFTLBBDDL");
  maf.add_permutation("lbbrrfflLBBRRFFL");
  maf.add_permutation("RllRfFFfbBBbrLLrtTTtdDDd");

  /* Basic Moves */
  maf.add_axiom("tttt","");
  /* Opposite sides commute */
  maf.add_axiom("td","dt");

  /* Five to turn other */
  maf.add_axiom("rlffbbRLtrlffbbRLD","");

  /* two turn commutators */
  maf.add_axiom("rrffrrffrrffrrffrrffrrff","");

  /* single turn commutators */
  maf.add_axiom("RTrtRTrtRTrtRTrtRTrtRTrt","");
  /* Pons Asinorum */
  maf.add_axiom("ttddllrrffbbttddllrrffbb","");
  /* Anti slice ditto */
  maf.add_axiom("rlfbtdrlfbtdrlfbtdrlfbtd","");
  /* more two stuff */
  maf.add_axiom("rrbbllrrbbllrrbbllrrbbll","");
  /* corner movers */
  maf.add_axiom("rflFRfLFrflFRfLFrflFRfLF","");
  maf.add_axiom("rfLFRflFrfLFRflFrfLFRflF","");
  maf.add_axiom("RdlDrdLDRdlDrdLDRdlDrdLD","");
  maf.add_axiom("RdlDrdLDbDFdBDfd","");
  maf.add_axiom("rfLFRflFRBLbrBlb","");
  maf.add_axiom("RbblrbbLRbblrbbL","");

//maf.add_axiom("rffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRffrffRff","");
#endif

#if 0
  /* Rubik - two adjacent faces */
  maf.set_generators("rRfF");
  maf.set_inverses("rRfF");
  maf.add_permutation("rffrRFFR");
  maf.add_permutation("rFFrRffR");
  maf.add_axiom("rrrr","");
  maf.add_axiom("rrfrfRfRFRffrfrFrFRF","");
  maf.add_axiom("rfrFrfrFrfrFrfrFrfrF","");
  maf.add_axiom("rrfrrfrFrrFrrfrrfrFrrF","");
  maf.add_axiom("rrffrrffrrffrrffrrffrrff","");
  maf.add_axiom("rfRFrfRFrfRFrfRFrfRFrfRF","");
  maf.add_axiom("rrfrrfrrFRfrffRffRffrfRF","");
  maf.add_axiom("rrfrfrfrfrffRFRfrrffrFRF","");
  maf.add_axiom("rrfrfrfrFRFRffRFRFRFrfrf","");
  maf.add_axiom("rrfrfrfRfrFRFRFrfrrfRFRF","");
  maf.add_axiom("rrfrfrfRFRFrffrFRFRFrfrF","");
  maf.add_axiom("rrfrfrffRfRFrrfrfrffRfRF","");
  maf.add_axiom("rrfrfrffRffRffrfrfrrFrrF","");
  maf.add_axiom("rrfrfrffRFrFrrfrfrffRFrF","");
  maf.add_axiom("rrfrfrffRFRfrrFRFRffrfrF","");
  maf.add_axiom("rrfrFRFRFrfRfrfrfrrFRFRf","");
  maf.add_axiom("rrfRFRFrfrfrffRfrfRFRFRf","");
  maf.add_axiom("rrFrfrfRFRFRffrFRFrfrfrF","");
#endif

#if 0
  /* Rubik slice group */
  maf.set_generators("iIjJkK"); /* i = rL j = tD k =fB */
  maf.set_inverses("iIjJkK");
  maf.add_permutation("kiijjkKIIJJK");
  maf.add_permutation("ikkIIKKi");
  maf.add_permutation("jJJjkKKk");

  maf.add_axiom("iiii","");
  maf.add_axiom("ijKikJIk","");
  maf.add_axiom("ijKiijKiijKi","");
  maf.add_axiom("ikikikikikik","");
#endif

#if 0
  /* Rubik anti-slice group */

  maf.set_generators("xXyYzZ"); /* z = rl y = td z =fb */
  maf.set_inverses("xXyYzZ");
  maf.add_permutation("zxxyyzZXXYYZ");
  maf.add_permutation("xzzxXZZX");
  maf.add_permutation("xXXxyYYyzZZz");

/* As usual There are possible several choices of axioms. */
#if 1
  maf.add_axiom("xxxx","");
  maf.add_axiom("xxyyxxyy","");
  maf.add_axiom("xyXzxyXz","");
  maf.add_axiom("xyzxyz","zyxzyx");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("xzXZxzXZxzXZ","yxyyxyzz");
#endif

#if 0
  // With this set we have to work out xxxx=Idword
  maf.add_axiom("xyXzxyXz","");
  maf.add_axiom("xxzXY","zXYxx");
  maf.add_axiom("xyzxyz","zyxzyx");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("xzXZxzXZxzXZ","yxyyxyzz");
#endif

#if 0
  maf.add_axiom("xxxx","");
  maf.add_axiom("xyxzxyxz","");
  maf.add_axiom("xxzXY","zXYxx");
  maf.add_axiom("xyzxyz","zyxzyx");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("xzXZxzXZxzXZ","yxyyxyzz");
#endif

#if 0
  maf.add_axiom("xxxx","");
  maf.add_axiom("xyxzxyxz","");
  maf.add_axiom("xxyyxxyy","");
  maf.add_axiom("xyXzxyXz","");
  maf.add_axiom("xxzXY","zXYxx");
  maf.add_axiom("xyzxyz","zyxzyx");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("xzXZxzXZxzXZ","yxyyxyzz");
#endif

#endif

#if 0
  /* Rubik 2 anti-slice group */

  maf.set_generators("xXzZ"); /* x = rl z =fb */
  maf.set_inverses("xXzZ");
  maf.add_permutation("xzzxXZZX");
  maf.add_permutation("xXXxzZZz");

  maf.add_axiom("xxxx","");
  maf.add_axiom("xxzzxxzz","");
  maf.add_axiom("xxzxxzxxzxxz","");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("xzxzxZxzxzxZ","");
#endif

#if 0
  /* Rubik half turn */
  maf.set_generators("123456");/*1=rr.2=ll,3=tt,4=dd,5=ff,6=bb*/
  maf.set_inverses("112233445566");
  maf.add_permutation("155331266442");
  maf.add_permutation("26611552");
  maf.add_permutation("1221");
  maf.add_axiom("1212","");
  maf.add_axiom("12341234","");
  maf.add_axiom("1231312414","");
  maf.add_axiom("1235312464","");
  maf.add_axiom("131313131313","");
  maf.add_axiom("1313151315151315","");
  maf.add_axiom("123123513456235635","");
  maf.add_axiom("131535131535131535","");
#endif

#if 0
  /* Rubik half turn + 1 anti-slice */
  maf.set_generators("123456yY");/*1=rr.2=ll,3=tt,4=dd,5=ff,6=bb*/
  maf.set_inverses("112233445566yY");
  maf.add_permutation("155331266442");
  maf.add_permutation("26611552");
  maf.add_permutation("1221");
  maf.add_axiom("21","12");
  maf.add_axiom("3412","1234");
  maf.add_axiom("5126351264","");
  maf.add_axiom("151515151515","");
  maf.add_axiom("231231","132132");
  maf.add_axiom("313414","132132");
  maf.add_axiom("16261626","5151");
  maf.add_axiom("31353135","13531353");
  maf.add_axiom("135153135153135153","");
  maf.delete_permutations();
  maf.add_permutation("26611552");
  maf.add_permutation("1221yYYy");
  maf.add_permutation("12213443");
  maf.add_axiom("y3","3y");
  maf.add_axiom("y4","4y");
  maf.add_axiom("yy","34");
  maf.add_axiom("1y145y53","");
  maf.add_axiom("y242Y45454","");
  maf.add_axiom("13y6153Y25","");
  maf.add_axiom("y1Y1Y1y1","354354");
  maf.add_axiom("y12Y1561512512312412","");
#endif

#if 0
  /* Rubik half turn + 2 anti-alice*/
  maf.set_generators("123456xXzZ");/*1=rr.2=ll,3=tt,4=dd,5=ff,6=bb*/
  maf.set_inverses("112233445566xXzZ");
  maf.add_permutation("155331266442");
  maf.add_permutation("26611552");
  maf.add_permutation("1221");
  maf.add_axiom("1212","");
  maf.add_axiom("12341234","");
  maf.add_axiom("1231312414","");
  maf.add_axiom("1235312464","");
  maf.add_axiom("131313131313","");
  maf.add_axiom("31353135","13531353");
  maf.add_axiom("135153135153135153","");
  maf.delete_permutations();
  maf.add_permutation("26611552xzzxXZZX");
  maf.add_permutation("1221xXXxzZZz");
  maf.add_permutation("12213443");
  maf.add_axiom("xxxx","");
  maf.add_axiom("xxzzxxzz","");
  maf.add_axiom("xxzxxzxxzxxz","");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("xzxzxZxzxzxZ","");
  maf.add_axiom("12xx","");
  maf.add_axiom("1x1X","");
  maf.add_axiom("1z1z2z2z","");
  maf.add_axiom("3x3x4x4x","");
  maf.add_axiom("3x4X3x4X","");
  maf.add_axiom("5x6X5x6X","");
  maf.add_axiom("13x326x6","");
  maf.add_axiom("15x523x3","");
  maf.add_axiom("3xzX4zxZ","");
  maf.add_axiom("13131x515X","");
  maf.add_axiom("15151x414X","");
  maf.add_axiom("1325z4235Z","");
  maf.add_axiom("3245z1425Z","");
  maf.add_axiom("Z23Zx53x","z13zX53X");
  maf.add_axiom("XZ3XZ3XZ3","xz4xz4xz4");
#endif

#if 0
  /* Rubik slice anti-slice group */

  maf.set_generators("xyzXYZ135246");/*1=rr.2=ll,3=tt,4=dd,5=ff,6=bb*/
  maf.set_inverses("xXyYzZ112233445566");
  maf.add_permutation("155331266442xzzyyxXZZYYX");
  maf.add_permutation("26611552xzzxXZZX");
  maf.add_permutation("1221xXXxyYYyzZZz");
  maf.add_axiom("xxxx","");
  maf.add_axiom("12","xx");
  maf.add_axiom("X2","1x");
  maf.add_axiom("1x","x1");
  maf.add_axiom("xxyyxxyy","");
  maf.add_axiom("xyxzxyxz","");
  maf.add_axiom("xyXzxyXz","");
  maf.add_axiom("x3X3x3X3","");
  maf.add_axiom("x3x3x4x4","");
  maf.add_axiom("524X415X","");
  maf.add_axiom("xxzXY","zXYxx");
  maf.add_axiom("xzzX4xzzX3","");
  maf.add_axiom("5126351264","");
  maf.add_axiom("151515151515","");
  maf.add_axiom("xyzxyz","zyxzyx");
  maf.add_axiom("xzxzxzxzxzxz","");
  maf.add_axiom("16261626","5151");
  maf.add_axiom("x3X3X3x3","152152");
  maf.add_axiom("135313531y45y3","");
  maf.add_axiom("x2y4Z6x2z6Y4X2z6","");
  maf.add_axiom("31353135","13531353");
  maf.add_axiom("x3x3x3x3x3x3x3x3","");
  maf.add_axiom("Z23Zx53x","z13zX53X");
  maf.add_axiom("Z23Zx53xZ23Zx53x","");
  maf.add_axiom("1315131525232523","");
  maf.add_axiom("135153135153135153","");
  maf.add_axiom("xy5xy5xy5xy5xy5xy5","");
  maf.add_axiom("xy5xy5xy5","XY6XY6XY6");
  maf.add_axiom("13251351351325135135","");
  maf.add_axiom("xzXZxzXZxzXZ","yxyyxyzz");
  maf.add_axiom("x2y4Z6xxy4Z6xxy4Z6x2","");
  maf.add_axiom("x35X53x35X5z61Z65y42y3","");
  maf.add_axiom("x2z6x2z6x2z6x2z6x2z6x2z6","");
#endif

#if 0
  /* Rubik half turns on four adjacent faces */
  maf.set_generators("1235");/*1=rr.2=ll,3=tt,5=ff*/
  maf.set_inverses("11223355");
  maf.add_permutation("12213553");
  maf.add_permutation("1221");
  maf.add_axiom("123123123123","");
  maf.add_axiom("131323131323","");
  maf.add_axiom("353535353535","");
  maf.add_axiom("12315131232523","");
  maf.add_axiom("12351531235253","");
  maf.add_axiom("1313151315151315","");
  maf.add_axiom("1313531353531353","");
  maf.add_axiom("131535131535131535","");
  maf.add_axiom("13251351351325135135","");
#endif
#if 1
  /* Rubik half turns on four adjacent faces - axioms without permutations*/
  maf.set_generators("1235");/*1=rr.2=ll,3=tt,5=ff*/
  maf.get_alphabet()->set_word_ordering(WO_Recursive);
//  maf.alphabet.set_weight(2,2);
//  maf.alphabet.set_weight(3,2);
  maf.set_inverses("11223355");
  maf.add_axiom("21","12");
  maf.add_axiom("151515151515","");
  maf.add_axiom("131313131313","");
  maf.add_axiom("131323131323","");
  maf.add_axiom("252525252525","");
  maf.add_axiom("232323232323","");
  maf.add_axiom("535353535353","");
  maf.add_axiom("2515251","1515252");
  maf.add_axiom("2525151","1525152");
  maf.add_axiom("2313231","1313232");
  maf.add_axiom("2323131","1323132");
  maf.add_axiom("3512531235125312","");
  maf.add_axiom("31353135","13531353");
  maf.add_axiom("32353235","23532353");
  maf.add_axiom("51535153","15351535");
  maf.add_axiom("52535253","25352535");
  maf.add_axiom("53515351","35153515");
  maf.add_axiom("53525352","35253525");
  maf.add_axiom("53135313","35313531");
  maf.add_axiom("53235323","35323532");
  maf.add_axiom("51315131","15131513");
  maf.add_axiom("52325232","25232523");
  maf.add_axiom("31513151","13151315");
  maf.add_axiom("32523252","23252325");
  maf.add_axiom("135153135153135153","");
  maf.add_axiom("235253235253235253","");
  maf.add_axiom("135213521352","153215321532");
  maf.add_axiom("351315351315351315","");
  maf.add_axiom("352325352325352325","");
  maf.add_axiom("531513531513531513","");
  maf.add_axiom("532523532523532523","");
  maf.add_axiom("152152152152","");
  maf.add_axiom("132132132132","");
  maf.add_axiom("1315131525232523","");
  maf.add_axiom("2325232515131513","");
  maf.add_axiom("1513151323252325","");
  maf.add_axiom("2523252313151315","");
  maf.add_axiom("13251351351325135135","");
  maf.add_axiom("23152352352315235235","");
  maf.add_axiom("15231531531523153153","");
  maf.add_axiom("25132532532513253253","");
  maf.add_axiom("12315131232523","");
  maf.add_axiom("21325232131513","");
  maf.add_axiom("12513151252325","");
  maf.add_axiom("21523252151315","");
  maf.add_axiom("12351531235253","");
#endif
#if 0
  /* Rubik half turns round axis */
  maf.set_generators("1256");/*1=rr.2=ll,5=ff,6=bb*/
  maf.set_inverses("11225566");
  maf.add_permutation("26611552");
  maf.add_permutation("1221");
  maf.add_axiom("21","12");
  maf.add_axiom("5612","1256");
  maf.add_axiom("151515151515","");
  maf.add_axiom("16261626","5151");
#endif

#if 0
  /* Rubik half turns round corner */
  maf.set_generators("135");/*1=rr,3=tt,5=ff*/
  maf.set_inverses("113355");
  maf.add_permutation("155331");
  maf.add_axiom("151515151515","");
  maf.add_axiom("31353135","13531353");
  maf.add_axiom("135153135153135153","");
#endif

#if 0
  maf.set_generators("aAbBcC");
  maf.set_inverses("AaBbCc");
  maf.add_axiom("ABAba","cBAb");
  maf.add_axiom("BCBcb","aCBc");
  maf.add_axiom("CACac","bACa");
#endif

#if 0
  maf.set_generators("aAbBcCdDeEfF");
  maf.set_inverses("aAbBcCdDeEfF");
  maf.add_axiom("cB","bA");
  maf.add_axiom("dC","bA");
  maf.add_axiom("eD","bA");
  maf.add_axiom("fE","bA");
  maf.add_axiom("aF","bA");
  maf.add_axiom("bAbAbAbAbAbA","");
#endif

#if 0
  // allegedly S9, but too hard for MAF as yet.
  maf.set_generators("aAbBcdef");
  maf.set_inverses("aAbBccddeeff");
  maf.add_axiom("aaa","");
  maf.add_axiom("bbb","");
  maf.add_axiom("acacac","");
  maf.add_axiom("adadad","");
  maf.add_axiom("aeaeae","");
  maf.add_axiom("afafaf","");
  maf.add_axiom("bcbcbc","");
  maf.add_axiom("bdbdbd","");
  maf.add_axiom("bebebe","");
  maf.add_axiom("bfbfbf","");
  maf.add_axiom("abAcabAc","");
  maf.add_axiom("abAdabAd","");
  maf.add_axiom("AbaeAbae","");
  maf.add_axiom("AbafAbaf","");
  maf.add_axiom("baBcbaBc","");
  maf.add_axiom("BabdBabd","");
  maf.add_axiom("baBebaBe","");
  maf.add_axiom("BabfBabf","");
#endif

#if 0
  //S9
  maf.set_generators("abcdefgh");
  maf.set_inverses("aabbccddeeffgghh");

  maf.add_axiom("bab","aba");
  maf.add_axiom("ca","ac");
  maf.add_axiom("da","ad");
  maf.add_axiom("ea","ae");
  maf.add_axiom("fa","af");
  maf.add_axiom("ga","ag");
  maf.add_axiom("ha","ah");

  maf.add_axiom("cbc","bcb");
  maf.add_axiom("db","bd");
  maf.add_axiom("eb","be");
  maf.add_axiom("fb","bf");
  maf.add_axiom("gb","bg");
  maf.add_axiom("hb","bh");

  maf.add_axiom("dcd","cdc");
  maf.add_axiom("ec","ce");
  maf.add_axiom("fc","cf");
  maf.add_axiom("gc","cg");
  maf.add_axiom("hc","ch");

  maf.add_axiom("ede","ded");
  maf.add_axiom("fd","df");
  maf.add_axiom("gd","dg");
  maf.add_axiom("hd","dh");

  maf.add_axiom("fef","efe");
  maf.add_axiom("ge","eg");
  maf.add_axiom("he","eh");

  maf.add_axiom("gfg","fgf");
  maf.add_axiom("hf","fh");

  maf.add_axiom("hgh","ghg");

#endif

#if 0
//Trefoil
  maf.set_generators("xXyY");
  maf.set_inverses("xXyY");
  maf.add_axiom("xx","YYY");
#endif

#if 0
//Z w C2
  maf.set_generators("aAtbB");
  maf.set_inverses("bBaAtt");
  maf.add_axiom("ba","ab");
  maf.add_axiom("tat","b");
#endif

#if 0
  //JohnsonB
  maf.set_generators("xyz");
  maf.set_inverses("xxyyzz");
  maf.add_axiom("xyzyxyzyzyxyzyxz","");
  maf.add_axiom("zyzxzxyzyxzxzyzx","");
#endif

#if 0
  //JohnsonC
  maf.set_generators("xXyY");
  maf.set_inverses("xXyY");
  maf.add_axiom("xyxYXyyXY","");
#endif

#if 0
  //F28
  maf.set_generators("aAbBcCdDeEfFgGhH");
  maf.set_inverses("aAbBcCdDeEfFgGhH");
  maf.add_axiom("ab","c");
  maf.add_axiom("bc","d");
  maf.add_axiom("cd","e");
  maf.add_axiom("de","f");
  maf.add_axiom("ef","g");
  maf.add_axiom("fg","h");
  maf.add_axiom("gh","a");
  maf.add_axiom("ha","b");
#endif

#if 0
  //F28 -h -b -g -c
  maf.set_generators("aAdDeEfF");
  maf.set_inverses("aAdDeEfF");
  maf.add_axiom("AeDeD","d");
  maf.add_axiom("de","f");
  maf.add_axiom("fefa","AeD");
  maf.add_axiom("efAe","aad");
#endif

#if 0
  //F29
  maf.set_generators("aAbBcCdDeEfFgGhHiI");
  maf.set_inverses("aAbBcCdDeEfFgGhHiI");
  maf.add_axiom("ab","c");
  maf.add_axiom("bc","d");
  maf.add_axiom("cd","e");
  maf.add_axiom("de","f");
  maf.add_axiom("ef","g");
  maf.add_axiom("fg","h");
  maf.add_axiom("gh","i");
  maf.add_axiom("hi","a");
  maf.add_axiom("ia","b");
#endif
#if 0
  //F2-10
  maf.set_generators("aAbBcCdDeEfFgGhHiIjJ");
  maf.set_inverses("aAbBcCdDeEfFgGhHiIjJ");
  maf.add_axiom("ab","c");
  maf.add_axiom("bc","d");
  maf.add_axiom("cd","e");
  maf.add_axiom("de","f");
  maf.add_axiom("ef","g");
  maf.add_axiom("fg","h");
  maf.add_axiom("gh","i");
  maf.add_axiom("hi","j");
  maf.add_axiom("ij","a");
  maf.add_axiom("ja","b");
#endif

#if 0
  maf.set_generators("aAbB");
  maf.set_inverses("aAbB");
  maf.add_axiom("ab","ba");
#endif

#if 0
  /* 5-5-5 */
  maf.set_generators("aAbBcCeEfg");
  maf.set_inverses("aAbBcCeEffgg");
  maf.add_axiom("aaaaa","");
  maf.add_axiom("bbbbb","");
  maf.add_axiom("ccccc","");
//  maf.add_axiom("ddddd","");
  maf.add_axiom("eee","");
  maf.add_axiom("abA","c");
  maf.add_axiom("bcB","a");
  maf.add_axiom("caC","b");
//  maf.add_axiom("acA","d");
  maf.add_axiom("ebE","c");
  maf.add_axiom("ecE","a");
  maf.add_axiom("eaE","b");
  maf.add_axiom("fbf","a");
  maf.add_axiom("af","E");
  maf.add_axiom("gf","fg");
  maf.add_axiom("gag","A");
#endif

#if 0
  /* icosahedron */
  maf.set_generators("aAbBcd");
  maf.set_inverses("aAbBccdd");
  maf.add_axiom("aaaaa","");
  maf.add_axiom("bbb","");
  maf.add_axiom("dc","cd");
  maf.add_axiom("dad","A");
  maf.add_axiom("ac","B");
#endif


#if 0
  /* 7-2-3 with reflections */
  maf.set_generators("1aA2b3cC");
  maf.set_inverses("112233aAbbcC");
  maf.add_axiom("23","a");
  maf.add_axiom("31","b");
  maf.add_axiom("12","c");
  maf.add_axiom("aaaaaaa","");
  maf.add_axiom("ccc","");
#endif

#if 0
  /* 7-2-3 without reflections */
  maf.set_generators("aAbcC");
  maf.set_inverses("aAbbcC");
  maf.add_axiom("aaaaaaa","");
  maf.add_axiom("ccc","");
  maf.add_axiom("abc","");
#endif

#if 0
  /* 7-3-2 without reflections */
  maf.set_generators("aAbBc");
  maf.set_inverses("aAbBcc");
  maf.add_axiom("aaaaaaa","");
  maf.add_axiom("bbb","");
  maf.add_axiom("abc","");
#endif

#if 0
  /* (7)-2-3 without reflections and minimal set of generators - not confluent*/
  maf.set_generators("bcC");
  maf.set_inverses("bbcC");
  maf.add_axiom("bcbcbcbcbcbcbc","");
  maf.add_axiom("ccc","");
#endif

#if 0
  /* (10)-4-2 without reflections and minimal set of generators - does not terminate*/
  maf.set_generators("bcC");
  maf.set_inverses("bbcC");
  maf.add_axiom("bcbcbcbcbcbcbcbcbcbc","");
  maf.add_axiom("cccc","");
#endif

#if 0
  /* 8-2-3 without reflections - does not terminate */
  maf.set_generators("aAbcC");
  maf.set_inverses("aAbbcC");
  maf.add_axiom("aaaaaaaa","");
  maf.add_axiom("ccc","");
  maf.add_axiom("abc","");
#endif

#if 0
  /* (8)-2-3 without reflections and minimal set of generators - does not terminate*/
  maf.set_generators("bcC");
  maf.set_inverses("bbcC");
  maf.add_axiom("bcbcbcbcbcbcbcbc","");
  maf.add_axiom("ccc","");
#endif

#if 0
  /* 8-2-3 with reflections -does not terminate */
  maf.set_generators("1aA2b3cC");
  maf.set_inverses("112233aAbbcC");
  maf.add_axiom("23","a");
  maf.add_axiom("31","b");
  maf.add_axiom("12","c");
  maf.add_axiom("aaaaaaaa","");
  maf.add_axiom("ccc","");
#endif

#if 0
  /* 8-2-3 with reflections and translation*/
  maf.set_generators("hHvVaAbcC3");
  maf.set_inverses("hHvVaAbbcC33");
  maf.add_axiom("3a3","A");
  maf.add_axiom("aah","vaa");
  maf.add_axiom("3b","b3");
  maf.add_axiom("3baaaa3","h");
  maf.add_axiom("aaaaaaaa","");
  maf.add_axiom("ccc","");
  maf.add_axiom("abc","");
#endif

#if 0
  /* 6-4-4 with reflections - does not terminate */
  maf.set_generators("1aA2bB3cC");
  maf.set_inverses("112233aAbBcC");
  maf.add_axiom("32","a");
  maf.add_axiom("13","b");
  maf.add_axiom("21","c");
  maf.add_axiom("aaaaaa","");
  maf.add_axiom("bbbb","");
  maf.add_axiom("cccc","");
#endif

#if 0
  /* 6-4-4 without reflections - does not terminate */
  maf.set_generators("aAbBcC");
  maf.set_inverses("aAbBcC");
  maf.add_axiom("aaaaaa","");
  maf.add_axiom("bbbb","");
  maf.add_axiom("cccc","");
  maf.add_axiom("abc","");
#endif

#if 0
  /* 2-4-12 without reflections*/
  maf.set_generators("aAcCb");
  maf.set_inverses("aAbbcC");
  maf.add_axiom("aaaaaaaaaaaa","");
  maf.add_axiom("cccc","");
  maf.add_axiom("abc","");
#endif

#if 0
  /* 6-4-4 from 2_4_12 without reflections*/
  maf.set_generators("dDeEcCaAb");
  maf.set_inverses("aAbbcCdDeE");
  maf.add_axiom("aaaaaaaaaaaa","");
  maf.add_axiom("aa","d");
  maf.add_axiom("Aca","e");
  maf.add_axiom("cccc","");
  maf.add_axiom("abc","");
#endif

#if 0
  //7-7-7
  maf.set_generators("aAbB");
  maf.set_inverses("aAbB");
  maf.add_axiom("aaaaaaa","");
  maf.add_axiom("bbbbbbb","");
  maf.add_axiom("ababababababab","");
#endif

#if 0
  /* cox5335 */
  maf.set_generators("abcde");
  maf.set_inverses("aabbccddee");
  maf.add_axiom("babab","ababa");
  maf.add_axiom("ca","ac");
  maf.add_axiom("da","ad");
  maf.add_axiom("ea","ae");
  maf.add_axiom("cbc","bcb");
  maf.add_axiom("db","bd");
  maf.add_axiom("eb","be");
  maf.add_axiom("dcd","cdc");
  maf.add_axiom("ec","ce");
  maf.add_axiom("edede","deded");
#endif

#if 0
  /* cox33334c */
  maf.set_generators("abcde");
  maf.set_inverses("aabbccddee");
  maf.add_axiom("bab","aba");
  maf.add_axiom("ca","ac");
  maf.add_axiom("da","ad");
  maf.add_axiom("eae","aea");
  maf.add_axiom("cbc","bcb");
  maf.add_axiom("db","bd");
  maf.add_axiom("eb","be");
  maf.add_axiom("dcdc","cdcd");
  maf.add_axiom("ec","ce");
  maf.add_axiom("ede","ded");
#endif


#if 0
  /* Picard */
  maf.set_generators("aAtTuUlLj");
  maf.set_inverses("aAtTuUlLjj");
  maf.add_axiom("aa","j");
  maf.add_axiom("ll","j");
  maf.add_axiom("Atatat","");
  maf.add_axiom("Alal","");
  maf.add_axiom("tLtl","");
  maf.add_axiom("uLul","");
  maf.add_axiom("uAlualual","");
  maf.add_axiom("ut","tu");
  maf.add_axiom("tj","jt");
  maf.add_axiom("uj","ju");
#endif

#if 0
  /* Picard */
  maf.set_generators("aAtTuUlL");
  maf.set_inverses("aAtTuUlL");
  maf.add_axiom("ll","aa");
  maf.add_axiom("Atatat","");
  maf.add_axiom("Alal","");
  maf.add_axiom("tLtl","");
  maf.add_axiom("uLul","");
  maf.add_axiom("uAlualual","");
  maf.add_axiom("ut","tu");
  maf.add_axiom("taa","aat");
  maf.add_axiom("uaa","aau");
#endif
  maf.grow_automata();
  maf.container.progress(1,"Elapsed time %ld\n",long(time(0) - now));
  delete &maf;
  return 0;
}

int main(int,unsigned char **)
{
  inner();
  return 0;
}
