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
$Log: rubik.h $
Revision 1.3  2009/10/12 17:38:57Z  Alun
Compatibility of FSA utilities with KBMAG improved.
Compatibility programs produce fewer FSAs KBMAG would not (if any)
Revision 1.2  2009/09/14 10:32:07Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


*/
/* class Rubik models 3*3*3 rubik cube */

/* apply() understands the following operation codes:
r=right,l=left,t=top,d=bottom (down),f=front,b=back
Lower case is quarter turn clockwise about the specified face,with that
face directly in front of you: i.e. td represents an "anti-slice" move, not
a "slice" move.
Uppercase letters are inverses of these.

1,2,3,4,5,6 are r*r,l*l,t*t,d*d,f*f,b*b respectively
x,y,z are anti-slice moves
x=r*l
y=t*d
z=f*b

XYZ are inverses.

Finally i,j,k are the slice moves, rL,tD,fB, and IJK are the inverses of these.

It is important to note that since the group of the Rubik cube is non-abelian,
that the order of the transforms in apply() is significant. In particular
you need decide whether you want to apply transformations from left to right
or from right to left. The class supports both.

Thus, in a "left to right" cube the string "fr" means first do f then do r,
whereas in a "right to left" cube it means first do r then do f.

The left to right reading convention is more natural for most people for
following a sequence of moves, but the right to left convention is used
more often by mathematicians. At first sight the mathematician's way of doing
things is perverse, but there is some logic to it: when composing transformations
the last transformation you apply is usually the most significant, so in the
same way as the most significant part of a number appears on the left, so it
should in a "word" describing a sequence of transformations.
*/

// Classes referred to but defined elsewhere
class Container;
class Output_Stream;

struct Perm;

class Rubik
{
  private:
    Perm & state;
    int read_left_to_right;
  public:
    Rubik(int read_left_to_right=1);
    Rubik(const char * operation,int read_left_to_right=1);
    ~Rubik();
    void start(); /* Puts cube back to solved position */
    bool operator==(const Rubik & other) const;
    bool operator!=(const Rubik & other) const;
    void apply(const char * operation);
    /* Print out a net describing a cube's current appearance. */
    void print(Container & container,Output_Stream *stream);
    /* Compare two operator strings and return true if they have the same effect */
    static bool compare(const  char * s1,const char * s2,int left_to_right=1);
  private:
     const Perm & transform(char opcode) const;
     Rubik & operator=(const Rubik& other);
     Rubik(const Rubik & model);
};

