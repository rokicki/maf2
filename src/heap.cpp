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


// $Log: heap.cpp $
// Revision 1.8  2010/06/10 13:57:23Z  Alun
// All tabs removed again
// Revision 1.7  2010/03/22 23:25:05Z  Alun
// Comments added. members of Heap_Node reoordered to save space in 64-bit
// version.
// Revision 1.6  2009/11/07 23:57:14Z  Alun
// Comment had got detached from the code it refers to
// Revision 1.5  2009/09/13 20:31:16Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// use of windows.h wrapped by awwin.h so we can clean up warnings and packing
// Revision 1.4  2008/11/02 19:00:48Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/11/02 20:00:48Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2007/10/24 21:15:37Z  Alun
//

/*
This module contains a heap manager which can be used to replace the
implementation of new() and delete() provided by the CPP runtime. It should
be noted that except for very large allocations greater than some fixed
size (4MB by default), once memory is in this heap manager it tends to stays
there and never get freed. (It will be freed by the OS when the program
terminates, or by this heap manager when its enclosing 4MB block is entirely
unused). Therefore this code should not be used in a shared library/DLL which
is intended to be dynamically loaded and unloaded by a process. (This defect
could be remedied, but it would require some additional housekeeping).

It is designed to optimise heap usage for programs that perform a vast number
of small memory allocations. In typical memory managers every allocation
carries a penalty for house keeping information - typically at least 8 bytes,
and often 16 bytes. This heap manager is different. Most of the time
allocations even of 1 byte will be contiguous. The penalty per allocation
for small allocations is typically 1 bit. (There is also a 20 byte overhead
over every 8Ks worth of such allocations)

Memory is organised into a series of nodes, most of which are subdivided into
a fixed number of items of fixed size (indicated by the item_size member).
For each node there is a status area which indicates which items in the node
are available. Nodes which do not have a predetermined size have item size 0.
Such nodes will periodically have new chunks taken out of them, either to
create a new fixed size node, or to satisfy memory allocations above the
maximum size managed using the fixed size item nodes.

The heap manager organises memory into two trees, one organised by address,
and one by item size. When a program requests memory of a size below the
maximum size managed using fixed nodes the allocator looks for an existing
node of the right item size, and returns the next available slot in it to the
program. If no node of the right size is available allocate() will try
to create one. If it can't then it returns 0, and, if the request came via
Heap_Implementation::malloc(), that will try to get more memory from the OS,
grow the heap, and retry.

Larger memory allocations are handled by returning a pointer to memory just
past a descriptor (in typical malloc fashion)

When memory is freed, the heap manager will look for a block that that the
address belongs to. If it finds one then it knows how much to free, and
whether the program is supposed to have the item it is freeing or not. If
the memory does not currently appear in the address tree, then it looks for a
descriptor below the freed address. If it finds one that looks valid, then it
assumes the program is not lying, and really did get the memory from this
manager, and returns it to the heap. A more robust scheme would be for the
header only to contain some pointer to some internal data, so that we could
more reliably detect a spurious free, by validating that the pointer and
the address are both valid and belong to each other. However any such scheme
involves a penalty of 12 bytes (and therefore 16 bytes if we want to keep
alignment) per allocation.

The heap manager has a cache of memory blocks both by size and by address.
This dramatically reduces the number of times the tree has to be searched.
Typically less than 1 in 500 calls will do a search. The main reson for doing
this caching is that searching the tree will at least reduce RAM cache
effectiveness, and in the worst case cause a lot of additional page faulting.
Probably these problems would be reduced by not putting the tree structure
into the nodes themselves. However this would increase the penalty per node
and complicate the code quite a bit.
*/

//#undef DEBUG
#ifdef WIN32
#include "awwin.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"

#define align_type double

#if _MSC_VER
/* Warning below is turned off because Microsoft compiler produces it for
   assignment of almost any expression to short and char data types, even
   when it should be completely obvious to it that there is no problem */
#pragma warning(disable: 4244) // type conversion possible data overflow
#endif

enum Tree_Type
{
  By_Address,
  By_Size
};

const unsigned HASH_SIZE = 2048;
const int RECENT = 8;
/* Our heap is going to contain pointers to both of the following
aggregates, which we can partially tell apart by the value of
size[0]/block_size, but for which we mostly rely on the size[1]/item_size
parameter. When a Memory_Header node is in our heap at all its size[1] is
0. If it is allocated it is removed from the heap altogether and the size[1]
parameter will be set to the requested, as opposed to actual, block size.
If the user frees a Heap_Node it will be in our "By_Size" list of nodes,
in which Memory_Header nodes never appear.
*/

union Memory_Header
{
  size_t size[2];
  align_type alignment;
};

struct Heap_Node
{
  size_t block_size;
  /* item_size must come next because insert_node() assumes that
     after any memory block there is another one and hopes to find one
     with item_size 0.

     In 32-bit world Memory_Header has room for item_size and bf part of
     Heap_Node. In 64-bit world it goes up to used.
     No space gets wasted in either model. */

  unsigned short item_size;
  /*
       bf takes following values -1 left subtree is taller,
                                 0  subtrees are same height
                                 1  right subtree taller
                                 2  the node is not in the relevant tree
       It is used to stop height of left and right subtrees differing by more
       than 1
  */
  signed char bf[2];
  short available;
  short used;
  Heap_Node *left[2];
  Heap_Node *right[2];
  size_t usable_size;

  unsigned char * data()
  {
    return (unsigned char *) (this + 1);
  }
  unsigned char * first_available()
  {
    unsigned char * s = status();
    unsigned char * d = data()+item_size*available*CHAR_BIT;
    int i = available;

    while (s[i] == UCHAR_MAX)
    {
      i++;
      d += item_size * CHAR_BIT;
      if (d >= s)
        return 0;
    }
    available = short(i);
    for (int j = 1;j;j *= 2, d += item_size)
    {
      if (d >= s)
        return 0;
      if (!(s[i] & j))
      {
        s[i] |= j;
        used++;
        return d;
      }
    }
    return 0;
  }
  int index(unsigned char * data)
  {
    return (data - this->data())/item_size;
  }
  unsigned char *status() const
  {
    return (unsigned char *) this + usable_size;
  }
};

class Heap::Implementation;
static Heap::Implementation * global_heap;
static bool leak_dump_allowed = true;

static void detect_leak()
{
  if (leak_dump_allowed)
  {
    const Heap_Status * s = Heap::get_global_heap()->status(false);
    if (s->nr_allocations != 0)
    {
      printf("Memory leak detected!\n");
      Heap::get_global_heap()->walk(false);
    }
  }
}

class Heap::Implementation : public Heap
{
  private:
    Heap_Node *root[2];
    size_t size_heap;
    size_t page_size;
    size_t critical_size;
    Heap_Node **use;
    Heap_Node **address_hash;
    Heap_Node *merged_node;
    void *spare_block;
    Memory_Header *recent[RECENT];
    Heap_Status status;
    int power;
    int recent_nr;
    char taller;
    char shorter;
    char found;
    char merged;
  public:
    // "constructor"
    static Heap::Implementation * create(void *mem_area,size_t size_area,int power)
    {
      /* create a new heap */
      size_t page_size = 1 << power;

      trim(&mem_area,&size_area);
      if (size_area >= sizeof(Heap::Implementation) + sizeof(Heap_Node))
      {
        Heap::Implementation * heap = (Heap::Implementation *) mem_area;
        void * base = (void *) (heap+1);
        size_t c = page_size/CHAR_BIT;
        heap->root[By_Address] = heap->root[By_Size] = 0;
        heap->critical_size = page_size/CHAR_BIT;
        heap->power = power;
        heap->page_size = page_size;
#ifdef DEBUG
        heap->size_heap = 0;
#endif
        size_t usable_size,true_block_size;
        do
        {
          heap->critical_size++;
          c = heap->block_count(heap->critical_size);
          heap->calc_node_size(heap->critical_size,c,&usable_size,
                               &true_block_size);
        }
        while (true_block_size <= headed_size(heap->critical_size)*c);
        size_area -= sizeof(Heap::Implementation);
        heap->add(base,size_area,false);
        size_t temp = heap->critical_size*sizeof(Heap_Node*);
        size_t temp2 = temp;
        heap->use = (Heap_Node **) heap->allocate(temp2);
        heap->merged_node = 0;
        memset(heap->use,0,temp);
        memset(heap->recent,0,sizeof(heap->recent));
        heap->recent_nr = 0;
        temp = HASH_SIZE*sizeof(Heap_Node*);
        temp2 = temp;
        heap->address_hash = (Heap_Node **) heap->allocate(temp2);
        memset(heap->address_hash,0,temp);
        return heap;
      }
      return 0;
    }

    static void * malloc(Heap::Implementation * heap,size_t size)
    {
      /* return pointer to block of memory of requested size, remembering
         size for subsequent use by free() */
      size_t save_size = size;
      void * ptr = heap ? heap->allocate(size) : 0;
      while (!ptr)
      {
        size = save_size;
        size_t nsize = headed_size(size);
        if (nsize < HUGE_SIZE)
          nsize = HUGE_SIZE;
        if (heap && nsize == HUGE_SIZE && heap->spare_block)
        {
          ptr = heap->spare_block;
          heap->spare_block = 0;
        }
        else
        {
          /* we increase the size of our large allocation a little
             because the code of insert_node() assumes it can look past the
             end of every heap node to find another one */
#ifdef WIN32
          ptr = HeapAlloc(GetProcessHeap(),0,nsize + sizeof(Memory_Header));
#else
          ptr = ::malloc(nsize + sizeof(Memory_Header));
#endif
          ((Heap_Node *) ((char *) ptr + nsize))->item_size = USHRT_MAX;
        }

        if (ptr)
        {
          if (heap)
            heap->add(ptr,nsize,false);
          else
          {
            atexit(detect_leak);
            heap = global_heap = Heap::Implementation::create(ptr,nsize,13);
          }
          if (heap->status.os_allocation && size < heap->critical_size)
            heap->status.needed = true;
          heap->status.os_allocation += nsize + sizeof(Memory_Header);
        }
        else
        {
          heap->walk(true);
          return 0;
        }
        ptr = heap->allocate(size);
      }
      heap->status.total_allocation += size;
      heap->status.nr_allocations++;
      return ptr;
    }

    size_t free(void * ptr)
    {
      status.nr_allocations--;
      size_t retcode = heap_free(ptr);
      status.total_allocation -= retcode;
      return retcode;
    }

    const Heap_Status * read_status(bool status_only)
    {
      if (!status_only || status.needed)
      {
        if (status_only)
          status.needed = false;
        return &status;
      }
      return 0;
    }

    void walk(int error_found)
    {
      weed();
      printf("Heap walk by address\n");
      walk_inner(root[By_Address],By_Address);
      printf("Heap walk by size\n");
      walk_inner(root[By_Size],By_Size);
      printf("End heap walk\n");

      if (error_found)
      {
        * (char *) 0 = 0;
        for (;;)
          ;
      }
    }

  private:
    void add(void * base,size_t size_area,bool allow_free)
    {
      /* add a new area of memory to a heap. This also gets called
         when an area that was removed from the heap entirely is returned
         to it. In this case we might be able to free an entire 4MB block */

      trim(&base,&size_area); /* This will only ever do something on a
                                 genuinely new block */
      if (size_area >= sizeof(Heap_Node))
      {
        Heap_Node * h = (Heap_Node *)base;
#ifdef DEBUG
        size_heap += size_area;
#endif
        h->block_size = size_area;
        h->item_size = 0;
        h->usable_size = 0;
        merged = shorter = taller = 0;
        insert_node(root[By_Address],h,By_Address);
        if (!merged)
          insert_node(root[By_Size],h,By_Size);
      }
      if (allow_free && merged_node && merged_node->block_size >= HUGE_SIZE)
      {
        delete_node(root[By_Address],merged_node,By_Address);
        delete_node(root[By_Size],merged_node,By_Size);
#ifdef DEBUG
        size_heap -= size_area;
#endif
        size_t size_freed = merged_node->block_size + sizeof(Memory_Header);
        status.os_allocation -= size_freed;
        if (!spare_block && merged_node->block_size == HUGE_SIZE)
          spare_block = merged_node;
        else
        {
#ifdef WIN32
          if (!HeapFree(GetProcessHeap(),0,merged_node))
          {
            printf("Heap corruption!\n",merged_node,GetLastError());
            * (char *) 0 = 0;
          }
#else
          ::free(merged_node);
#endif
        }
      }
      merged_node = 0;
    }

    static size_t aligned_size(size_t size)
    {
      return (size + sizeof(align_type)-1)/sizeof(align_type)*sizeof(align_type);
    }

    void * allocate(size_t &size)
    {
      Heap_Node * h;
      if (!size)
        size = 1;

      int count = size >= critical_size ? 0 : block_count(size);
      if (count)
      {
        unsigned char * answer;
        do
        {
          if (use[size])
            h = use[size];
          else
          {
            for (h = root[By_Size];h && h->item_size != size;)
            {
              if (size < h->item_size)
                h = h->left[By_Size];
              else
                h = h->right[By_Size];
            }
          }

          if (!h)
          {
            h = node_create(size,count);
            if (!h)
              return 0;
          }
          use[size] = h;
          answer = h->first_available();
          if (!answer)
          {
            // we remove a fully used node from the By_Size tree
            // since no more allocations from it are possible. It
            // will get put back as soon as an item is freed.
            use[size] = 0;
            delete_node(root[By_Size],h,By_Size);
            h->bf[By_Size] = 2;
          }
        }
        while (!answer);
        return answer;
      }
      else
      {
        size = headed_size(size);
        for (int i = 0; i < RECENT;i++)
          if (recent[i] && recent[i]->size[0]==size)
          {
            void * answer = recent[i] + 1;
            recent[i] = 0;
            recent_nr = i;
            return answer;
          }
        return alloc(size,true);
      }
    }

#ifdef DEBUG
    void check(int checktype)
    {
      int j = 0;

      if (check1_inner(root[By_Address],0,0,checktype,&j) != size_heap)
      {
        printf("heap corruption 3 in checktype %d\n",checktype);
        walk(1);
      }
      check2_inner(root[By_Size],0,0,checktype,&j);
    }
#endif

    /**/

    size_t heap_free(void * mem)
    {
      Heap_Node * h;
      h = quick_find(mem);
      if (!h)
      {
        for (h = root[By_Address];h;)
          if (mem < (void *) h)
            h = h->left[By_Address];
          else if (mem >= (void *) h->status())
            h = h->right[By_Address];
          else
            break;
      }

      if (h)
      {
        /* In this case we are freeing a block with items */
        int i = h->index((unsigned char *) mem);
        int j = i/CHAR_BIT;
        int k = (1 << (i % CHAR_BIT));
        if ((unsigned char  *) mem != h->data() + i*h->item_size || i < 0)
        {
          printf("Invalid free %p\n",mem);
          * (char *) 0 = 0;
          return 0;
        }
        unsigned char &s = h->status()[j];
        if (s & k)
        {
#ifdef DEBUG
          memset(mem,0xee,h->item_size);
#endif
          s &= ~k;
          if (j < h->available)
            h->available = short(j);
          h->used--;
        }
        else
        {
          printf("Invalid free %p\n",mem);
          * (char *) 0 = 0;
          return 0;
        }
        if (!h->used && use[h->item_size] != h)
        {
          /* we unformat a completely empty node unless it is the one that will
             be allocated from */
          return erase_node(h);
        }
        else if (h->bf[By_Size] == 2)
        {
          /* this was a fully allocated node, which now has a free slot and needs
             to go back in the By_Size tree */
          insert_node(root[By_Size],h,By_Size);
        }
        address_hash[hash_value(h+1)] = h;
        return h->item_size;
      }
      else
      {
        Memory_Header * m = (Memory_Header *) mem - 1;
        size_t retcode = m->size[0];
        if ((size_t) m != aligned_size((size_t) m) ||
            retcode != aligned_size(retcode) ||
            m->size[1] > retcode )
        {
          printf("Invalid free %p\n",mem);
          * (char *) 0 = 0;
          return 0;
        }
        if (retcode > HUGE_SIZE)
        {
          status.os_allocation -= retcode - sizeof(Memory_Header);
#ifdef WIN32
          HeapFree(GetProcessHeap(),0,m);
#else
          ::free(m);
#endif
        }
        else if (m->size[1]==m->size[0])
        {
          if (recent[recent_nr])
            add(recent[recent_nr],recent[recent_nr]->size[0],true);
          recent[recent_nr++] = m;
          if (recent_nr==RECENT)
            recent_nr = 0;
        }
        else
          add(m,retcode,true);
        return retcode;
      }
    }

    static size_t headed_size(size_t size)
    {
      size += sizeof(Memory_Header);
      if (size < sizeof(Heap_Node))
        size = sizeof(Heap_Node);
      return aligned_size(size);
    }

    void * alloc(size_t size,bool offset)
    {
      /* If offset is false we are going to turn the returned block of
         memory into a Heap_Node. If offset is true then we want a
         malloc() style allocation, so the return value is past the header
         of the block */
      Heap_Node * h;
      /* Go down to the part of the tree where nodes that are not fixed in size
         can be found */
      for (h = root[By_Size];h && h->item_size;h = h->left[By_Size])
        ;
      // and get the node
      h = alloc_inner(h,size,1,1);
      if (h)
      {
        /* remove it from the size tree, and in the "malloc" case from the
           address tree as well, in which case we also set size[1] to
           the requested size */
        Memory_Header * m = (Memory_Header * )h;
        delete_node(this->root[By_Size],h,By_Size);
        if (offset)
        {
          delete_node(this->root[By_Address],h,By_Address);
          m->size[1] = size;
        }
        else
          h->item_size = 1;
#ifdef DEBUG
        size_heap -= h->block_size;
#endif
        if (h->block_size - size >= critical_size)
        {
          /* We got more than we asked for. Chop this block up into 2
             and return the remainder of the block to the heap as a new
             node */
          size_t remaining = h->block_size - size;
          if (!offset)
          {
            char * new_h = (char *) h + size;
#ifdef DEBUG
            if (h->block_size & (sizeof(align_type)-1))
              * (char *) 0 = 0;
#endif
            h->block_size = size;
            add(new_h,remaining,false);
          }
          else
          {
            char * new_h = (char *) h + remaining;
            m = (Memory_Header *)new_h;
            m->size[0] = m->size[1] = size;
            add(h,remaining,false);
          }
        }
        return (void *) (offset ? m+1 : m);
      }
      weed();
      return 0;
    }

    Heap_Node * alloc_inner(Heap_Node * root,size_t size_wanted,
                            bool exact_possible,bool divide_possible)
    {
      /* look for a block of memory of the requested size starting from
         the current possition in the tree. This function is recursive,
         and as we get deeper into the tree we have more knowledge about
         how far left or right we should look. Hence the exact_possible,
         divide_possible flags. */

      Heap_Node * best = 0;
      while (root && (exact_possible || divide_possible))
      {
        /* if this node is bigger than the one we want, but too small to be
           divided then we can look in the left subtree of the left subtree
           for a node the exact size we want,
           but it is not worth searching right for one truncatable.
         */
        if (root->item_size)
        {
          /* In this case we have wandered into part of the tree where already
             fixed nodes are managed, we need to head left again until we get
             back to unformatted blocks */
          root = root->left[By_Size];
        }
        else
        {
          if (root->block_size == size_wanted)
            return root;
          else if (root->block_size > size_wanted)
          {
            bool next_divide_possible = divide_possible &&
                              root->block_size - size_wanted >= page_size /*critical_size*/;
            if (root->left[By_Size] &&
                (exact_possible || next_divide_possible))
            {
              Heap_Node * ptr = alloc_inner(root->left[By_Size],size_wanted,
                                            exact_possible,
                                            next_divide_possible);
              if (ptr)
              {
                if (ptr->block_size == size_wanted)
                  return ptr;
                if (!best)
                  best = ptr;
              }
            }
            exact_possible = 0;
          }

          if (best && best->block_size - size_wanted >= page_size)
            break;

          if (root->block_size >= size_wanted &&
              (!best || root->block_size-size_wanted >= critical_size))
            best = root;
          root = root->right[By_Size];
        }
      }

      return best;
    }

    size_t erase_node(Heap_Node * h)
    {
      unsigned retcode = h->item_size;
      address_hash[hash_value(h+1)] = 0; // it is about 99.9% certain that the
                                         // node being deleted has the hash
                                         // so we may as well just clear the entry

      delete_node(root[By_Address],h,By_Address);
      if (h->bf[By_Size] != 2)
        delete_node(root[By_Size],h,By_Size);
#ifdef DEBUG
      size_heap -= h->block_size;
#endif
      add(h,h->block_size,true);
      return retcode;
    }

    unsigned hash_value(void * mem) const
    {
      return ( ((size_t) mem - sizeof(Heap_Node)) >> power) % HASH_SIZE;
    }

    int block_count(size_t item_size,size_t block_size) const
    {
      return (CHAR_BIT*(block_size - sizeof(Heap_Node))-(CHAR_BIT-1))/(CHAR_BIT*item_size+1);
    }

    int block_count(size_t item_size) const
    {
      return block_count(item_size,page_size);
    }

    void calc_node_size(size_t item_size,int nr_items,
                        size_t * usable_size,size_t * true_block_size)
    {
      if (!nr_items)
        nr_items = block_count(item_size);
      *usable_size = sizeof(Heap_Node) + nr_items * item_size;
      *true_block_size = *usable_size + (nr_items+CHAR_BIT-1)/CHAR_BIT;
      *true_block_size = aligned_size(*true_block_size);
    }

    Heap_Node * node_create(size_t item_size,int nr_items)
    {
      Heap_Node * h;
      size_t usable_size,true_block_size;

      calc_node_size(item_size,nr_items,&usable_size,&true_block_size);
      h = (Heap_Node *) alloc(true_block_size,false);

      if (h)
      {
        if (h->block_size > true_block_size+item_size)
        {
          nr_items = block_count(item_size,h->block_size);
          for (;;)
          {
            calc_node_size(item_size,nr_items,&usable_size,&true_block_size);
            if (true_block_size <= h->block_size)
              break;
            nr_items--;
          }
        }
        h->item_size = (unsigned short) item_size;
        h->usable_size = usable_size;
        h->available = 0;
        h->used = 0;
#ifdef DEBUG
        size_heap += usable_size;
#endif
        memset(h->status(),0,h->block_size-h->usable_size);
        insert_node(root[By_Size],h,By_Size);
      }
      return h;
    }

    Heap_Node * quick_find(void *mem)
    {
      /* Try to find a block of memory in the cache of recently used blocks */
      unsigned pos = hash_value(mem);
      Heap_Node* hashed = address_hash[pos];
      if (hashed && mem >= hashed && mem < hashed->status())
        return hashed;
      /* Because nodes can straddle an 8K boundary our hash value may
         be wrong. So we check the hash beneath as well */
      pos = pos ? pos-1 : HASH_SIZE-1;
      hashed = address_hash[pos];
      if (hashed && mem >= hashed && mem < hashed->status())
        return hashed;
      return 0;
    }

    static void trim(void ** mem_area,size_t * size_area)
    {
      size_t base = (size_t) *mem_area;
      char * s = (char *) *mem_area;
      size_t size = *size_area;
      size_t orig_size = size;

      while ((size_t) s != aligned_size(base))
      {
        if (!size)
          break;
        s++,base++,size--;
      }

      while (size != aligned_size(size))
        size--;
      *size_area = size;
      *mem_area = s;
      if (size != orig_size)
        printf("Lost bytes %zu %zu\n",size,orig_size);
    }


    size_t size(void * mem)
    {
      Memory_Header * m = (Memory_Header *) mem -1;
      return m->size[0];
    }
    void unhash(Heap_Node *h)
    {
      Heap_Node* &hashed = address_hash[hash_value(h+1)];
      if (hashed == h)
        hashed = 0;
    }
    void weed()
    {
      /* When an entire page of fixed size nodes of any particular size
         becomes free it is not immediately "unformatted" and returned to
         the resizable part of the heap if it was the page being used for
         allocations of that size. Instead it stays available for the
         next allocation that might want it. From time to time we call weed()
         to get rid of such blocks that are still entirely empty, on the basis
         that allocations of that size are probably not happening any more.*/
      for (size_t size = 1;size < critical_size;size++)
      {
        Heap_Node *h = use[size];
        if (h && !h->used)
        {
          use[size] = 0;
          erase_node(h);
        }
      }
      for (int i = 0;i < RECENT;i++)
        if (recent[i])
        {
          add(recent[i],recent[i]->size[0],true);
          recent[i] = 0;
        }
      recent_nr = 0;
    }

    /* AVL Node management stuff */
    void delete_node(Heap_Node * &root,Heap_Node * data,int index)
    {
      if (!root)
        return;

      if (root == data)
      {
        found = 1;
        if (!root->left[index])
        {
          /* No left tree so we can replace ourself with our right subtree */
          shorter = 1;
          root = root->right[index];
        }
        else if (!root->right[index])
        {
          /* No right tree so we can replace ourself with our left subtree */
          shorter = 1;
          root = root->left[index];
        }
        else /* Hard case! */
        {
          Heap_Node * next;
          /* Get the immediate succesor of this item - left most item in right subtree */
          delete_first_node(root->right[index],&next,index);

          /* Replace ourself with that node */
          next->left[index] = root->left[index];
          next->right[index] = root->right[index];
          next->bf[index] = root->bf[index];
          root = next;
          if (shorter)
            fix_right_shortening(root,index);
        }
      }
      else if (index == By_Address ? data > root :
               data->item_size > root->item_size ||
               data->item_size==root->item_size &&
               (data->block_size > root->block_size ||
                data->block_size == root->block_size && data > root))
      {
        delete_node(root->right[index],data,index);
        if (shorter)
          fix_right_shortening(root,index);
      }
      else
      {
        delete_node(root->left[index],data,index);
        if (shorter)
          fix_left_shortening(root,index);
      }
    }

    void delete_first_node(Heap_Node * &root,Heap_Node * * deleted_node,
                           int index)
    {
      /* pluck the left most element of the avl tree */

      if (root->left[index] == 0)
      {
        shorter = 1;
        *deleted_node = root;
        root = root->right[index];
      }
      else
      {
        delete_first_node(root->left[index],deleted_node,index);
        if (shorter)
          fix_left_shortening(root,index);
      }
    }

    void insert_node(Heap_Node * &root,Heap_Node * data,int index)
    {
      /* Inserts nodes into size or address tree. The address case is
         complicated by the fact that we try to merge adjacent nodes to
         unfragment memory. When we do this we handle most of the
         necessary fiddling with the size tree for the merged nodes. On
         return we arrange for data->size to be non zero if data still needs
         to be inserted into the size tree and zero if not */

      if (root == 0)
      {
        root = data;
        root->left[index] = root->right[index] = 0;
        root->bf[index] = 0;
        taller = 1;
      }
      else if (index == By_Address ? data > root :
               data->item_size > root->item_size ||
               data->item_size == root->item_size &&
               (data->block_size > root->block_size ||
                data->block_size == root->block_size && data > root))
      {
        if (index == By_Address && !root->item_size &&
            (char *) root + root->block_size == (char *) data)
        {
          /* grow the current node - delete and reinsert in size tree */
          delete_node(this->root[By_Size],root,By_Size);
          root->block_size += data->block_size;
          /* see if we can now join the next node in the tree */
          merged = 1;
          data = (Heap_Node *) ((char *) data + data->block_size);
          shorter = found = 0;
          if (!data->item_size) /* this test could GPF, but we make sure
                                   that any allocated block always has a
                                   slight over-allocation to stop this happening */
            delete_node(root->right[By_Address],data,By_Address); /* allowed to fail */
          if (found)
          {
            char i = shorter;
            shorter = 0;
            delete_node(this->root[By_Size],data,By_Size);
            root->block_size += data->block_size;
            insert_node(this->root[By_Size],root,By_Size);
            shorter = i;
            if (shorter)
              fix_right_shortening(root,By_Address);
          }
          else
            insert_node(this->root[By_Size],root,By_Size);
          merged_node = root;
          taller = 0;
        }
        else
        {
          insert_node(root->right[index],data,index);
          if (index == By_Address && shorter)
            fix_right_shortening(root,By_Address);
          else if (taller) /* right subtree increased in height */
          {
            if (root->bf[index] <= 0)
            {
              /* -1 : 1  ->  1     0: 1  -> 1    */
              /*     / \    / \      / \   / \   */
              /*    m+1 m  m+1 m+1  m   m m   m+1*/
              taller = ++root->bf[index];
            }
            else
            {
              taller = 0;
              if (root->right[index]->bf[index] > 0)
              {
                /* case 0 can't happen because if right subtree increased in
                   height so its left and right must have different heights */

                 /* in diagram below lhs*/
                 /* shows tree after insertion */
                 /*   1     ->   2       */
                 /*  / \        / \      */
                 /* m   2      1   m+1   */
                 /*    / \    /  \       */
                 /*   m  m+1 m    m      */
                 rotate_left(root,index);
                 root->bf[index] = root->left[index]->bf[index] = 0;
              }
#ifdef DEBUG
              else if (root->right[index]->bf[index] == 0)
                * (char *) 0 = 0; /* We really can't get here! */
#endif
              else /* -1 */
              {
                /*   1           ->     3       */
                /*  / \                /  \     */
                /* m   2 ->   3       1     2   */
                /*    / \    / \     / \   / \  */
                /*   3   m  a    2  m   a  b  m */
                /*  / \        /  \             */
                /* a   b      b    m            */
                /* a and b are both of height   */
                /* m or m-1 and not both m-1    */
                rotate_right(root->right[index],index);
                rotate_left(root,index);
                root->left[index]->bf[index] = -(root->bf[index] == 1);
                root->right[index]->bf[index] = root->bf[index] == -1;
                root->bf[index] = 0;
              }
            }
          }
        }
      }
      else
      {
#ifdef DEBUG
        if (index == By_Address)
        {
          if ((char *) data + data->block_size > (char *) root)
          {
            printf("bad call to free!\n");
            * (char *) 0 = 0;
            for (;;)
              ;
          }
        }
#endif
        if (index == By_Address &&
            (char *) data + data->block_size == (char *) root && !root->item_size)
        {
          shorter = taller = 0;
          delete_node(this->root[By_Size],root,By_Size);
          data->block_size += root->block_size;
          data->left[By_Address] = root->left[By_Address];
          data->right[By_Address] = root->right[By_Address];
          data->bf[By_Address] = root->bf[By_Address];
          root = data;
          shorter = taller = 0;
          if (root->left[By_Address])
          {
            Heap_Node * temp = root->left[By_Address];
            while (temp->right[By_Address])
              temp = temp->right[By_Address];
            if ((char *) temp + temp->block_size == (char *) root && !temp->item_size)
            {
              delete_node(this->root[By_Size],temp,By_Size);
              temp->block_size += data->block_size;
              merged = 1;
              shorter = 0;
              delete_node(root->left[By_Address],temp,By_Address);
              temp->left[By_Address] = root->left[By_Address];
              temp->right[By_Address] = root->right[By_Address];
              temp->bf[By_Address] = root->bf[By_Address];
              taller = 0;
              insert_node(this->root[By_Size],root = temp,By_Size);
              taller = 0;
              if (shorter)
                fix_left_shortening(root,By_Address);
            }
          }
          merged_node = root;
        }
        else
        {
          insert_node(root->left[index],data,index);
          if (index == By_Address && shorter)
            fix_left_shortening(root,By_Address);
          else if (taller) /* left subtree increased in height */
          {
            if (root->bf[index] >= 0)
              taller = --root->bf[index];
            else
            {
              taller = 0;
#ifdef DEBUG
              if (root->left[index]->bf[index]==0)
                 * (char *) 0 = 0;
#endif
              if (root->left[index]->bf[index] < 0)
              {
                /* case 0 can't happen because if right subtree increased in
                   height so its left and right must have different heights */
                rotate_right(root,index);
                root->bf[index] = root->right[index]->bf[index] = 0;
              }
              else /* 1 */
              {
                rotate_left(root->left[index],index);
                rotate_right(root,index);
                root->left[index]->bf[index] = -(root->bf[index] == 1);
                root->right[index]->bf[index] = (root->bf[index] == -1);
                root->bf[index] = 0;
              }
            }
          }
        }
      }
    }

    void fix_left_shortening(Heap_Node * &root,int index)
    {
      /* Performs necessary rotations after our left subtree got shorter */
      if (root->bf[index] <= 0)
      {
        /* 0:  1  ->   1   -1:  1   -> 1    */
        /*    / \     /  \     / \    /  \  */
        /*   m   m   m-1  m   m+1 m  m    m */
        shorter = root->bf[index]++;
      }
      else if (root->right[index]->bf[index] >= 0)
      {
        /* in diagram below rhs */
        /* shows tree after deletion */
        /* case 1 */
        /*   1  ->       2     */
        /*  / \         / \    */
        /* m   2       1   m+1 */
        /*    / \     / \      */
        /*   m   m+1 m   m     */
        /* case 0 */
        /*   1  ->       2     */
        /*  / \         / \    */
        /* m   2       1   m+1 */
        /*    / \     / \      */
        /*   m+1 m+1  m   m+1   */
        shorter = root->right[index]->bf[index]!=0;
        /* root becomes root->left[index] */
        root->bf[index] = !shorter;
        /* root right becomes root */
        root->right[index]->bf[index] = -!shorter;
        rotate_left(root,index);
      }
      else
      {
        /*   1           ->     3      */
        /*  / \                /  \    */
        /* m   2 ->   3       1     2  */
        /*    / \    / \     / \   / \ */
        /*   3   m  a    2  m   a b   m*/
        /*  / \        /  \            */
        /* a   b      b    m           */
        /* a and b are both of height  */
        /*  m or m-1 and not both m-1  */
        rotate_right(root->right[index],index);
        rotate_left(root,index);
        root->left[index]->bf[index] = -(root->bf[index] == 1);
        root->right[index]->bf[index] = root->bf[index] == -1;
        root->bf[index] = 0;
      }
    }

    /**/

    void fix_right_shortening(Heap_Node * &root,int index)
    {
      /* Performs necessary rotations after our right subtree got shorter.
         Should be "opposite" of previous function i.e. replace
         left by right,right by left,and change sign of all comparisons and
         assignments to bf */
      if (root->bf[index] >= 0)
      {
        /* 0:  1  ->   1   1:   1   -> 0   */
        /*    / \     /  \     / \    / \  */
        /*   m   m   m    m-1 m  m+1 m   m */
        shorter = root->bf[index]--;
      }
      else if (root->left[index]->bf[index] <= 0)
      {
        /* in diagram below rhs */
        /* shows tree after deletion */
        /* case -1 */
        /*     1  ->       2      */
        /*    / \         / \     */
        /*   2   m       m+1 1    */
        /*  / \             / \   */
        /* m+1 m           m   m  */
        /* case 0 */
        /*    1  ->       2       */
        /*   / \         / \      */
        /*  2   m     m+1   1     */
        /* / \             / \    */
        /*m+1 m+1        m+1  m   */
        /* root becomes root->right[index] */
        shorter = root->left[index]->bf[index] !=0;
        root->bf[index] = -!shorter;
        /* root->left[index] becomes root */
        root->left[index]->bf[index] = !shorter ;
        rotate_right(root,index);
      }
      else
      {
        /*    1           ->     3      */
        /*   / \                /  \    */
        /*  2   m   2->3       2    1   */
        /* / \        / \     / \  / \  */
        /* m   3     2   b   m   a b  m */
        /*    / \   / \                 */
        /*   a   b m   a                */
        /* a and b are both of height   */
        /*  m or m-1 and not both m-1 */
        rotate_left(root->left[index],index);
        rotate_right(root,index);
        root->right[index]->bf[index] = root->bf[index] == -1;
        root->left[index]->bf[index] = -(root->bf[index] ==1);
        root->bf[index] = 0;
      }
    }

    /**/

    /*
    Effect of rotate_left():

                    2               3
                   / \             / \
                  a   3           2   c
                     / \         / \
                    b   c       a   b

    and of rotate_right()

                    2               1
                   / \             / \
                  1   c           a   2
                 / \                 / \
                a   b               b   c

    */

    static void rotate_left(Heap_Node *&node,int index)
    {
      Heap_Node * temp = node->right[index];
      node->right[index] = temp->left[index];
      temp->left[index] = node;
      node = temp;
    }

    static void rotate_right(Heap_Node *&node,int index)
    {
      Heap_Node * temp = node->left[index];
      node->left[index] = temp->right[index];
      temp->right[index] = node;
      node = temp;
    }

    /* Debugging related methods */

    void walk_inner(Heap_Node * root,int index)
    {
      while (root)
      {
        if (index || root->left[index] < root)
          walk_inner(root->left[index],index);
        if (!root->item_size)
          printf("free node %p %zu\n",root,root->block_size);
        else if (root->used)
          printf("used node %p s %zu %u used %d\n",root,root->usable_size,root->item_size,root->used);
        if (!index && root >= root->right[index])
          break;
        root = root->right[index];
      }
    }

#ifdef DEBUG

    size_t check1_inner(Heap_Node * root,Heap_Node * low,Heap_Node * high,
                        int checktype,int * height)
    {
      size_t size = 0;
      int lh = 0,rh = 0;
      if (root)
      {
        if (root == root->left[By_Address] || root == root->right[By_Address])
        {
          printf("heap corruption 4 in check %d\n",checktype);
          walk(1);
        }

        if (root->left[By_Address])
          size += check1_inner(root->left[By_Address],low,root,checktype,&lh);

        size += root->block_size;

        if (root->block_size < sizeof(Heap_Node))
        {
          printf("heap corruption 1 in check %d\n",checktype);
          walk(1);
        }

        if (root < low || high && root > high)
        {
          printf("heap corruption 2 in check %d\n",checktype);
          walk(1);
        }

        if (root->right[By_Address])
          size += check1_inner(root->right[By_Address],root,high,checktype,&rh);
        switch (rh - lh)
        {
          case 1:
          case 0:
          case -1:
            if (root->bf[By_Address] == rh-lh)
              break;
          default:
            * (char *) 0 = 0;
         }
      }
      * height = lh > rh ? lh+1 : rh +1;
      return size;
    }

    /**/

    size_t check2_inner(Heap_Node * root,Heap_Node * low,Heap_Node * high,int checktype,int * height)
    {
      size_t size = 0;
      int lh = 0,rh = 0;
      if (root)
      {
        if (root == root->left[By_Size] || root == root->right[By_Size])
        {
          printf("heap corruption 6 in check %d\n",checktype);
          walk(1);
        }

        if (root->left[By_Size])
        {
          if (root->left[By_Size]->item_size > root->item_size)
            * (char *) 0 = 0;
          size += check2_inner(root->left[By_Size],low,root,checktype,&lh);
        }

        size += root->block_size;

        if (root->block_size < sizeof(Heap_Node))
        {
          printf("heap corruption 1 in check %d\n",checktype);
          walk(1);
        }

        if (low && (root->item_size < low->item_size ||
            root->item_size == low->item_size &&
            (root->block_size < low->block_size ||
             root->block_size == low->block_size && root < low)))
        {
          printf("heap corruption 5 in check %d\n",checktype);
          walk(1);
        }

        if (high && (root->item_size > high->item_size ||
            root->item_size == high->item_size &&
            (root->block_size > high->block_size ||
             root->block_size == high->block_size && root > high)))
        {
          printf("heap corruption 5 in check %d\n",checktype);
          walk(1);
        }

        if (root->right[By_Size])
        {
          if (root->right[By_Size]->item_size < root->item_size)
            * (char *) 0 = 0;
          size += check2_inner(root->right[By_Size],root,high,checktype,&rh);
        }
        switch (rh - lh)
        {
          case 1:
          case 0:
          case -1:
            if (root->bf[By_Size] == rh-lh)
              break;
          default:
            * (char *) 0 = 0;
        }
      }
      * height = lh > rh ? lh+1 : rh+1;
      return size;
    }
#endif
};

Heap::Heap()
{
}

Heap * Heap::create(void *mem_area,size_t size_area,int power)
{
  return Heap::Implementation::create(mem_area,size_area,power);
}

Heap * Heap::get_global_heap()
{
  return global_heap;
}

void * Heap::malloc(size_t nr_bytes)
{
  return Heap::Implementation::malloc((Heap::Implementation *)this,nr_bytes);
}

size_t Heap::free(void * mem)
{
  return ((Heap::Implementation *)this)->free(mem);
}

const Heap_Status * Heap::status(bool status_only)
{
  return ((Heap::Implementation *)this)->read_status(status_only);
};

void Heap::walk(bool crash)
{
  ((Heap::Implementation *)this)->walk(crash);
};

/**/

void * operator new(size_t block_size)
{
  return Heap::Implementation::malloc(global_heap,block_size);
}

void operator delete(void * address)
{
  global_heap->free(address);
}

void * operator new [](size_t block_size)
{
  return Heap::Implementation::malloc(global_heap,block_size);
}

void operator delete [](void * address)
{
  global_heap->free(address);
}

void Heap::prevent_leak_dump()
{
  leak_dump_allowed = false;
}
