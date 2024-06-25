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


// $Log: rubik.cpp $
// Revision 1.5  2010/06/10 13:57:48Z  Alun
// All tabs removed again
// Revision 1.4  2009/10/12 17:37:10Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.3  2009/09/12 18:47:59Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.2  2007/12/20 23:25:42Z  Alun
//

#include <stdlib.h>
#include <string.h>
#include "awcc.h"
#include "rubik.h"
#include "container.h"

/*

The Rubik class models the Rubik cube as a subgroup of S48.
The faces are denoted, by R,L,T,D,F,B (right,left,top,down,front,back)
The central squares are kept in a fixed orientation, so that the
only permitted moves are rotations of a face about its central square.
Squares on most faces are numbered clockwise from RTF or LBD, counting first
the corner pieces and then edge pieces of each face. The faces are numbered
in the order R,L,T,D,F,B.

This gives the following scheme

R  F  Z  B    T  R  X  L   F  T  Y  D
T  0  4  1    F 16 20 17   R 32 36 33
Y  7     5    Z 23    21   X 39    37
D  3  6  2    B 19 22 18   L 35 38 34

L  D  Y  T    D  B  Z  F   B  L  X  R
B  8 12  9    L 24 28 25   D 40 44 41
Z 15    13    X 31    29   Y 47    45
F 11 14 10    R 27 30 26   T 43 46 42

The final arrangement can be visualised from the following net:

           T
          18 22 19
          21    23
          17 20 16  -> f action
 L         F        R         B
 9 13 10  35 39 32  0  4  1  42 46 43 <- t action
12    14  38    36  7     5  45    47
 8 15 11  34 37 33  3  6  2  41 44 40 -> d action
           D
          25 29 26
          28    30
          24 31 27
          B         \/ b action
       8  40 44 41  2
      12  47    45  5
       9  43 46 42  1
          T
          18 22 19
         \/     /\
    l action     r action

Any particular arrangement of the cube can thus be described by a 48 byte
string, indicating the current occupant of a particular location. This may seem
rather wasteful of memory as there are only 20 pieces that actually move,
but each of the 20 needs a state variable to describe its orientation, so
you would be hard put not to use at least 40 bytes to describe a cube. In fact
in order to compare whether two permutations are the same we only
need to compare 20 faces: all 8 faces on left and right, and the edge pieces
wrapped around the X axis (20,22,29,31 in numbering above). (And in fact only
18 ignoring inaccesible positions, since the contents of one corner and one
edge piece are fixed once we know the contents of the others.)

Permutations are implemented by translation matrices, so that the value at
a particular offset, indicates the source of the object that it is moved to
that location. This makes both applying a transform to the cube, and composing
permutations fairly simple.
*/

typedef char Perm_String[48];

Perm_String IdWordstr =
{
   0, 1, 2, 3, 4, 5, 6, 7,
   8, 9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,
  24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,
  40,41,42,43,44,45,46,47
};

const Perm_String rstr =
{
   3, 0, 1, 2, 7, 4, 5, 6,
   8, 9,10,11,12,13,14,15,
  33,17,18,32,20,21,22,36,
  24,25,41,42,28,29,45,31,
  26,27,34,35,30,37,38,39,
  40,19,16,43,44,23,46,47
};

const Perm_String lstr =
{
   0, 1, 2, 3, 4, 5, 6, 7,
  11, 8, 9,10,15,12,13,14,
  16,43,40,19,20,47,22,23,
  34,35,26,27,38,29,30,31,
  32,33,17,18,36,37,21,39,
  25,41,42,24,44,45,46,28
};

const Perm_String tstr =
{
  42,43, 2, 3,46, 5, 6, 7,
   8,35,32,11,12,39,14,15,
  19,16,17,18,23,20,21,22,
  24,25,26,27,28,29,30,31,
   1,33,34, 0,36,37,38, 4,
  40,41,9,10,44,45,13,47
};

const Perm_String dstr =
{
   0, 1, 33, 34, 4, 5, 37, 7,
  41, 9,10,40,12,13,14,44,
  16,17,18,19,20,21,22,23,
  27,24,25,26,31,28,29,30,
  32,11, 8,35,36,15,38,39,
   2, 3,42,43, 6,45,46,47
};

const Perm_String fstr =
{
  17, 1, 2, 16, 4, 5, 6, 20,
   8, 9,25,26,12,13,29,15,
  10,11,18,19,14,21,22,23,
  24,3, 0,27,28,7,30,31,
  35,32,33,34,39,36,37,38,
  40,41,42,43,44,45,46,47
};

const Perm_String bstr =
{
   0, 27, 24, 3, 4, 31, 6, 7,
   18,19,10,11,22,13,14,15,
  16,17, 1, 2,20,21, 5,23,
  9,25,26,8,28,29,30,12,
  32,33,34,35,36,37,38,39,
  43,40,41,42,47,44,45,46,
};

struct Perm
{
  Perm_String op;

  Perm operator *(const Perm & source) const
  {
    /* This multiplication is correct for right to left composition of permutations */
    Perm_String temp;
    size_t i;
    for (i = 0;i < sizeof(op);i++)
      temp[i] = source.op[op[i]];

    return Perm(temp);
  }

  Perm(const Perm_String s = IdWordstr)
  {
    memcpy(op,s,sizeof(op));
  }
};

typedef const Perm * cpperm;
const Perm Idword;
const Perm r(rstr);
const Perm l(lstr);
const Perm t(tstr);
const Perm d(dstr);
const Perm f(fstr);
const Perm b(bstr);
const Perm R(r*r*r);
const Perm L(l*l*l);
const Perm T(t*t*t);
const Perm D(d*d*d);
const Perm F(f*f*f);
const Perm B(b*b*b);

const Perm one(r*r);
const Perm two(l*l);
const Perm three(t*t);
const Perm four(d*d);
const Perm five(f*f);
const Perm six(b*b);

const Perm x(r*l);
const Perm y(t*d);
const Perm z(f*b);
const Perm X(r*r*r*l*l*l);
const Perm Y(t*t*t*d*d*d);
const Perm Z(f*f*f*b*b*b);
const Perm i(r*l*l*l);
const Perm j(t*d*d*d);
const Perm k(f*b*b*b);
const Perm I(l*r*r*r);
const Perm J(d*t*t*t);
const Perm K(b*f*f*f);

cpperm stdops[]={&r,&l,&t,&d,&f,&b,&R,&L,&T,&D,&F,&B,&one,&two,&three,&four,&five,&six,&x,&y,&z,&X,&Y,&Z,&i,&j,&k,&I,&J,&K};
const char * xlat = "rltdfbRLTDFB123456xyzXYZijkIJK";

/*
R  F  Z  B    T  R  X  L   F  T  Y  D
T  0  4  1    F 16 20 17   R 32 36 33
Y  7     5    Z 23    21   X 39    37
D  3  6  2    B 19 22 18   L 35 38 34

L  D  Y  T    D  B  Z  F   B  L  X  R
B  8 12  9    L 24 28 25   D 40 44 41
Z 15    13    X 31    29   Y 47    45
F 11 14 10    R 27 30 26   T 43 46 42
*/

/* First 48 are for the moving parts that move. Last 6 are for the non moving
   centre squares */
static char colour[]="RRRRRRRRLLLLLLLLTTTTTTTTDDDDDDDDFFFFFFFFBBBBBBBBTLFRBD";

void Rubik::start()
{
  state = Idword;
}

bool Rubik::operator==(const Rubik & other) const
{
  return memcmp(&state,&other.state,sizeof(state))==0;
}

bool Rubik::operator!=(const Rubik & other) const
{
  return memcmp(&state,&other.state,sizeof(state))!=0;
}

const Perm & Rubik::transform(char operation) const
{
  const char * p = strchr(xlat,operation);
  if (p)
    return *stdops[p-xlat];
  return Idword;
}

void Rubik::apply(const char * operation)
{
  if (read_left_to_right)
  {
    while (*operation)
    {
      if (*operation != '*')
        state = transform(*operation)*state;
      operation++;
    }
  }
  else
  {
    size_t i;
    for (i = strlen(operation);i > 0;i--)
      state = transform(operation[i-1])*state;
  }
}

Rubik::Rubik(int read_left_to_right_) :
 state(* new Perm)
{
  read_left_to_right = read_left_to_right_;
}

Rubik::Rubik(const char *operation,int read_left_to_right_) :
 state(* new Perm)
{
  read_left_to_right = read_left_to_right_;
  apply(operation);
}

Rubik::~Rubik()
{
  delete &state;
}

void Rubik::print(Container & container,Output_Stream * stream)
{
   static const int order[9][12] =
   {
    {-1,-1,-1,18,22,19,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,21,48,23,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,17,20,16,-1,-1,-1,-1,-1,-1},
    { 9,13,10,35,39,32, 0, 4, 1,42,46,43},
    {12,49,14,38,50,36, 7,51, 5,45,52,47},
    { 8,15,11,34,37,33, 3, 6, 2,41,44,40},
    {-1,-1,-1,25,29,26,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,28,53,30,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,24,31,27,-1,-1,-1,-1,-1,-1}
  };
  int i,j;
  for (i = 0;i < 9;i++)
  {
    for (j = 0;j < 12;j++)
    {
      int k = order[i][j];
      if (k == -1)
        container.output(stream," ");
      else if (k >= 48)
        container.output(stream,"%c",colour[k]);
      else
        container.output(stream,"%c",colour[state.op[k]]);
    }
    container.output(stream,"\n");
  }
}

bool Rubik::compare(const  char * s1,const char * s2,int left_to_right)
{
  Rubik cube1(s1,left_to_right);
  Rubik cube2(s2,left_to_right);
  return cube1==cube2;
}

#ifdef TEST_RUBIK

void rubik_check(const char *a,const char *b)
{
  if (!Rubik::compare(a,b))
  {
    container.error_output("Identity %s=%s computed incorrectly!\n",a,b);
    * (char *) 0 = 0;
  }
}

int main()
{
  rubik_check("rrrr","");
  rubik_check("llll","");
  rubik_check("tttt","");
  rubik_check("dddd","");
  rubik_check("ffff","");
  rubik_check("bbbb","");
  rubik_check("rl","lr");
  rubik_check("td","dt");
  rubik_check("fb","bf");
  rubik_check("xX","");
  rubik_check("yY","");
  rubik_check("zZ","");
  rubik_check("rR","");
  rubik_check("lL","");
  rubik_check("tT","");
  rubik_check("dD","");
  rubik_check("fF","");
  rubik_check("bB","");
  rubik_check("rr","1");
  rubik_check("ll","2");
  rubik_check("tt","3");
  rubik_check("dd","4");
  rubik_check("ff","5");
  rubik_check("bb","6");
  rubik_check("xx","12");
  rubik_check("yy","34");
  rubik_check("zz","56");
  rubik_check("xxyy","yyxx");
  rubik_check("xyzxyz","zyxzyx");
  rubik_check("xxyyxxyy","");
  rubik_check("xzxzxzxzxzxz","");
  rubik_check("xzXZxzXZxzXZ","yxyyxyzz");
  rubik_check("xyxzxyxz","");
  rubik_check("xyXzxyXz","");

  rubik_check("3412","1234");
  rubik_check("5126351264","");
  rubik_check("151515151515","");
  rubik_check("16261626","5151");
  rubik_check("31353135","13531353");
  rubik_check("135153135153135153","");

  rubik_check("x3x3x3x3x3x3x3x3","");
  rubik_check("x3X3x3X3","");
  rubik_check("x3X3X3x3","152152");
  rubik_check("xzzX4xzzX3","");
  rubik_check("x2y4Z6x2z6Y4X2z6","");
  rubik_check("x2y4Z6xxy4Z6xxy4Z6x2","");
  rubik_check("x2z6x2z6x2z6x2z6x2z6x2z6","");
  rubik_check("xy5xy5xy5xy5xy5xy5","");
  rubik_check("xy5xy5xy5","XY6XY6XY6");
  rubik_check("xz4xz4xz4","XZ3XZ3XZ3");
  rubik_check("x35X53x35X53x35X53x35X53x35X53x35X53","");
  rubik_check("iiii","");
  rubik_check("ijKikJIk","");
  rubik_check("ijKiijKiijKi","");
  rubik_check("ikikikikikik","");
  /* Five to turn other */
  rubik_check("rlffbbRLtrlffbbRLD","");

  /* two turn commutators */
  rubik_check("rrffrrffrrffrrffrrffrrff","");

  /* single turn commutators */
  rubik_check("RTrtRTrtRTrtRTrtRTrtRTrt","");
  /* Pons Asinorum */
  rubik_check("ttddllrrffbbttddllrrffbb","");
  /* Anti slice ditto */
  rubik_check("rlfbtdrlfbtdrlfbtdrlfbtd","");
  /* more two stuff */
  rubik_check("rrbbllrrbbllrrbbllrrbbll","");
  /* corner movers */
  rubik_check("rflFRfLFrflFRfLFrflFRfLF","");
  rubik_check("rfLFRflFrfLFRflFrfLFRflF","");
  rubik_check("RdlDrdLDRdlDrdLDRdlDrdLD","");
  rubik_check("RdlDrdLDbDFdBDfd","");
  rubik_check("rfLFRflFRBLbrBlb","");
  rubik_check("RbblrbbLRbblrbbL","");

  rubik cube1("r",1);
  cube1.print();
  cube1.apply("f");
  cube1.print();
  cube1.start();
  cube1.apply("rf");
  cube1.print();

  rubik cube2("r",0);
  cube2.print();
  cube2.apply("f");
  cube2.print();
  cube2.start();
  cube2.apply("fr");
  cube2.print();
}

#endif
