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
$Log: heap.h $
Revision 1.6  2010/03/19 07:53:02Z  Alun
Comments changed. Mythical try_malloc() method added
Revision 1.5  2009/09/12 18:48:27Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.4  2008/10/17 08:14:34Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.4  2008/10/17 09:14:34Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.3  2008/08/03 10:34:26Z  Alun
"Aug 2008 snapshot"
Revision 1.2  2007/10/24 21:15:32Z  Alun
*/
/* This is the header file for the public interface, such as it is, for a
   heap manager that can be used to replace the C/C++ heap manager.
   The implementation, which is in heap.cpp, relies on malloc() and free() on
   most platforms, but on Windows platforms uses Win32 APIs HeapAlloc()
   and HeapFree(). However these underlying implementations are only used to
   obtain very large memory blocks, very occasionally, and the use of them
   could be replaced by other allocators (such as VirtualAlloc() on Windows
   or brk() on Unix).  The global C++ new() and delete() are replaced by
   functions that call the methods of this heap manager. A Heap instance will
   be created as soon as new() is called for the first time.

   The reason for replacing new() is that it is typically very inefficient at
   allocating small amounts of memory. This implementation is designed to have
   the smallest possible overhead. Roughly speaking there is a 1 bit overhead
   per item, for as many items that will fit into the chosen page size, plus
   a twenty byte overhead per page. For items over a certain size the twenty
   byte penalty makes the traditional "secret header" style of allocation more
   efficient, and this heap manager makes a good decision about when to do
   this. The implementation also tries to efficiently merge adjacent free
   blocks and to keep fragmentation to a minimum when breaking a big block
   into a smaller one, and to optimise performance by keeping to a minimum
   the frequency with which its internal structures need to be searched.

   Only the fact that the global new/delete operator have no parameter to say
   what heap to use makes it necessary for this heap manager to have any
   global variables. Therefore it can be used as a private heap manager, to
   implement class level new() and delete() funcionality. However, it should
   be noted that each heap starts off quite big, so care should be taken not
   to proliferate heaps with no good reason.

   Surprise:

   1) Any kind of invalid free, including double deletion and freeing of
   a null pointer will result in instant death of your program. A hard
   exception is generated. This is because the author does not want any
   such program errors to be silently ignored in his own code.

   2) Unless you install a new handler an allocation that is about to return
   0 will cause your program to call exit(3) and abort with an error message.

   Limitations:
   1) Currently this heap manager should not be used in a DLL that is
   loaded and unload within the lifetime of a process. This might well cause
   a memory leak.
   2) There is no support for buffer overrun/underrun detection.
   3) The _set_new_handler() function is not replaced, and therefore not
   supported, since this might cause a conflict with the CRT.
*/

#include "awcc.h"

const size_t HUGE_SIZE = 0x400000;

struct Heap_Status
{
  size_t total_allocation;
  size_t os_allocation;
  int nr_allocations;
  bool needed; // ignore this member - it is used internally
               // and reset when you read the status
};

/* we would like class Heap to be a pure virtual class with "virtual = 0"
for all its non-static methods. That is you you should think of it. However it
is extremely awkward to implement it like that, because it can't be constructed
in the normal way (because it is itself responsible for implementing new).
Not only that, but at least its malloc() method will be called at least once
with a null this pointer and has to work! So the non static methods just cast
the this pointer to Implementation * and call a method in Implementation. This
is OK because there is no way you will be able to create any other kind of
Heap object. With the global heap there is no real issue because the real
declaration declares it as a Heap::Implementation, so there is no inefficiency.
With a private heap there is the minor inefficiency of an extra call, though
hopefully the compiler will optimise this into a jump. In any case I do not
want to expose the implementation in a header file */

class Heap
{
  public:
    class Implementation;
  private:
    Heap();
    friend class Implementation;
  public:
    // "constructor"
    static Heap * create(void *mem_area,size_t size_area = HUGE_SIZE,
                         int power = 13);
    static Heap * get_global_heap();
    // Method that works like like C malloc()
    void * malloc(size_t nr_bytes); /* Terminates program if allocation cannot be honoured */
    void * try_malloc(size_t nr_bytes); /* Returns 0 if allocation cannot be honoured */

    /* returns size of block freed. In theory 0 is returned if the address is
       invalid. However the function causes a protection fault deliberately in
       this case, because it is a serious program bug. */
    size_t free(void * mem);
    /* Call status() to retrieve information about heap utilisation.
       Pass status_only = true, if the method is allowed to return 0
       if it thinks there is no need to report the heap status at the moment.
       Pass status_only = false, if the method must return valid status
       information */
    const Heap_Status * status(bool status_only);
    /* walk() will produce a list of memory blocks in the heap that have
       outstanding allocations. The most likely reason you would call this
       function is to assist with the diagnosis of memory leaks */
    void walk(bool crash);
    static void prevent_leak_dump();
};
