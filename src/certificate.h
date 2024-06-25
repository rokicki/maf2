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
$Log: certificate.h $
Revision 1.3  2010/01/26 08:10:10Z  Alun
renamed node_removed to node_freed. removed node_added
Unused state_doubtful() method removed
Revision 1.2  2009/09/14 10:32:07Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


*/
#pragma once
#ifndef CERTIFICATE_INCLUDED
#define CERTIFICATE_INCLUDED 1

/* Because the reduction machine changes over time reduction information
   can become invalid. Certificates are used to check whether a word
   that is supposedly reduced or has a state actually does so. */

enum Certificate_Validity
{
  CV_INVALID,
  CV_CHECKABLE,
  CV_VALID
};

class Certificate
{
  friend class Node_Manager;
  private:
    unsigned long reduction_added;
    unsigned long node_freed;
    Certificate_Validity check(const Certificate & other) const
    {
      /* Return value indicates whether "other" certificate is still valid
         it only makes sense to check against the master certificate in
         the Node_Manager instance */
      if (reduction_added == other.reduction_added)
        return CV_VALID;
      if (node_freed > other.node_freed)
        return CV_INVALID;
      return CV_CHECKABLE;
    }
  public:
    Certificate() :
      reduction_added(0),
      node_freed(0)
    {}
};

#endif

