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
$Log: awwin.h $
Revision 1.3  2010/06/10 13:57:55Z  Alun
All tabs removed again
Revision 1.2  2009/11/10 08:53:45Z  Alun
pragmas added to allow C12 to compile with 2008 SDK header files
Revision 1.1  2009/09/14 10:31:32Z  Alun
New file.
*/
/* The purpose of this header file is to wrap windows.h to achieve the following
   1) Arrange for it to work whatever packing option is chosen
   2) Turn off the useless warnings that including it typically generates
*/
#pragma warning(disable: 4514) // we seem to have to disable this permanently
#pragma warning(push)
#pragma pack(push,8)
#pragma warning(disable: 4668 4820)
#pragma warning(disable: 4255)
#if _MSC_VER==1200
#pragma warning(disable: 4035 4068)
#endif
#include <windows.h>
#pragma pack(pop)
#pragma warning(pop)

