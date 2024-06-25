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
$Log: arraybox.h $
Revision 1.6  2010/06/10 13:57:53Z  Alun
All tabs removed again
Revision 1.5  2010/06/10 10:38:38Z  Alun
count() and grow_capacity() methods added. Comments changed
Revision 1.4  2009/09/12 18:48:22Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.3  2009/04/25 21:41:44Z  Alun
reverse() method added
Revision 1.2  2008/10/01 06:55:43Z  Alun
Changes to improve CPU cache utilisation
Revision 1.1  2008/09/30 10:16:58Z  Alun
New file.
*/
#pragma once
#ifndef ARRAYBOX_INCLUDED
#define ARRAYBOX_INCLUDED
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/* Header file for classes dealing with resizable arrays.

   The main class for resizable arrrays is the template Array_Of.
   So far this is fairly rudimentary compared to something like STL's
   std::vector. Its main feature is that it makes it easy to manage a
   collection of arrays which should all have the same capacity. Though such
   arrays are a bit unusual (because one would typically create a single
   array of an aggregate/class type instead), they are useful in situations
   where not all the data is needed all the time, or when alignment
   restrictions would make an aggregate type either waste space or be slow.
   Another consideration is that where the pattern of usage is very
   non-uniform, CPU cache utilisation will be better with this type of
   organisation.

   This management is done through deriving Array_Of from Unknown_Collection,
   which is an interface any variable sized collection can easily implement.

   Unknown_Collection is a base class for collections of unknown objects
   the elements of which can be referred to with an index value.
   A class derived from this must do the following in implementation
   functions:

   The destructor must delete any objects owned by the collection.

   set_capacity() must be capable of changing the number of elements in
   the collection keeping the current contents. Note that at the time
   set_capacity() is called the specified capacity is only a new upper
   limit: i.e. if the new capacity is greater than the old one new items
   should not be created yet. However if there is a meaningful 0 value it
   is OK to create all the new items with this value.

   delete_item() must delete any data associated with the specified item
   within the collection, or set it to 0.

   class Collection_Manager provides an easy way to manage a number
   of related Unknown_Collection instances. The Unknown_Collection items to
   be managed should be added to the Collection_Manager with add().
   Arrays can be add to and extracted from the collection at any time.
   They are only managed while in the Collection_Manager.
*/

class Collection_Manager;

class Unknown_Collection
{
  friend class Collection_Manager;
  private:
    Collection_Manager * manager;
  public:
    Unknown_Collection() :
      manager(0)
    {}
    virtual ~Unknown_Collection()
    {}
    virtual void set_capacity(Element_Count new_capacity,bool keep = true) = 0;
    virtual void delete_item(Element_ID id) = 0;
    virtual Element_Count capacity() const = 0;
    void empty()
    {
      set_capacity(0,false);
    }
    Collection_Manager * get_manager()
    {
      return manager;
    }
};

class Collection_Manager : public Unknown_Collection
{
  private:
    struct Managed_Collection
    {
      Managed_Collection * next;
      Element_ID id;
      Unknown_Collection *uc;
      bool owned;
      Managed_Collection(Element_ID id_,Unknown_Collection &uc_,bool owned_) :
        next(0),
        id(id_),
        uc(&uc_),
        owned(owned_)
      {}
      ~Managed_Collection()
      {
        if (uc && owned)
          delete uc;
      }
    };
  private:
    Managed_Collection * head;
    Managed_Collection ** tail;
    Element_ID next_id;
    Element_ID nr_collections;
  public:
    Collection_Manager() :
      next_id(0),
      nr_collections(0),
      head(0),
      tail(&head)
    {}
    ~Collection_Manager()
    {
      erase();
    }

    Element_ID add(Unknown_Collection &uc,bool owned = true)
    {
      if (uc.manager)
        uc.manager->extract(uc);
      Managed_Collection * mc = new Managed_Collection(++next_id,uc,owned);
      uc.manager = this;
      *tail = mc;
      tail = &mc->next;
      nr_collections++;
      if (head != mc)
      {
        Element_Count capacity = head->uc->capacity();
        uc.set_capacity(capacity,true);
      }
      return mc->id;
    }

    bool extract(Unknown_Collection & uc)
    {
      Managed_Collection **prev = &head;
      while (*prev)
      {
        Managed_Collection * item = *prev;
        if (item->uc == &uc)
        {
          item->uc = 0;
          if (tail == &item->next)
            tail = prev;
          *prev = item->next;
          nr_collections--;
          uc.manager = 0;
          item->owned = false;
          delete item;
          return true;
        }
        prev = &item->next;
      }
      return false;
    }

    void erase()
    {
      while (head)
      {
        Managed_Collection * mc = head;
        head = mc->next;
        nr_collections--;
        mc->uc->manager = 0;
        delete mc;
      }
    }

    Element_Count capacity() const
    {
      if (head)
        return head->uc->capacity();
      return 0;
    }

    void set_capacity(Element_Count capacity,bool keep = true)
    {
      for (Managed_Collection * mc = head;mc;mc = mc->next)
        mc->uc->set_capacity(capacity,keep);
    }

    void grow_capacity(bool keep = true)
    {
      Element_Count new_capacity = capacity();
      if (new_capacity/8 + 256 > MAX_STATES - new_capacity)
        new_capacity = MAX_STATES;
      else
        new_capacity = (new_capacity + new_capacity/8 + 256) & ~255;
      for (Managed_Collection * mc = head;mc;mc = mc->next)
        mc->uc->set_capacity(new_capacity,keep);
    }

    void delete_item(Element_ID item_id)
    {
      for (Managed_Collection * mc = head;mc;mc = mc->next)
        mc->uc->delete_item(item_id);
    }
};

/* The class below is not generic, but is suitable for any type to which
   zero can be assigned */

template<class T> class Array_Of : public Unknown_Collection
{
  private:
    T * data;
    Element_Count nr_allocated;
  public:
    Array_Of():
      data(0),
      nr_allocated(0)
    {}
    ~Array_Of()
    {
      Collection_Manager * manager = get_manager();
      if (manager)
        manager->extract(*this);
      set_capacity(0,false);
    }
    virtual void delete_item(Element_ID id)
    {
      if (id >= 0 && id < nr_allocated)
        data[id] = 0;
    }

    void resize(Element_Count new_capacity,bool keep,T default_value)
    {
      if (data && !keep)
      {
        /* delete first, to give heap manager best chance of giving us
           the same area of memory */
        delete [] data;
        data = 0;
      }
      T * new_data = new_capacity ? new T[new_capacity] : 0;
      Element_Count common = keep ? new_capacity : 0;
      if (common > nr_allocated)
        common = nr_allocated;
      Element_ID i;
      for (i = 0; i < common;i++)
        new_data[i] = data[i];
      for (i = common; i < new_capacity;i++)
        new_data[i] = default_value;
      if (data)
        delete [] data;
      data = new_data;
      nr_allocated = new_capacity;
    }

    void reverse()
    {
      for (Element_ID i = 0; i*2 < nr_allocated;i++)
      {
        T temp(data[i]);
        data[i] = data[nr_allocated-1-i];
        data[nr_allocated-1-i] = temp;
      }
    }
    void set_capacity(Element_Count new_capacity,bool keep = true)
    {
      T t(0);
      resize(new_capacity,keep,t);
    }

    void take(Array_Of & other)
    {
      if (this != &other)
      {
        set_capacity(0,false);
        nr_allocated = other.nr_allocated;
        data = other.data;
        other.data = 0;
        other.nr_allocated = 0;
      }
    }

    Element_Count capacity() const
    {
      return nr_allocated;
    }

    Element_Count count() const
    {
      /* I rather regret not separating count() and capacity() for this class
         as I have done for other collection type classes. This is because
         this class was originally designed for use with Hash where the
         eventual count is unknown, but the Hash knows the current count. So
         possibly count() should ask the manager to report the answer, if there
         is one. However, since I want this function to be very fast, it seems
         better to make it a synonym for capacity(). The caller should resize
         the array if need be when it is fully constructed. */
      return nr_allocated;
    }

    T & operator[](Element_ID id)
    {
      return data[id];
    }
    const T & operator[](Element_ID id) const
    {
      return data[id];
    }

    T * buffer()
    {
      return data;
    }
};

class Array_Of_Data : public Array_Of<Byte_Buffer>
{
  public:
    bool set_data(Element_ID id,const void * buffer,size_t buffer_size)
    {
      operator[](id).clone(buffer,buffer_size);
      return true;
    }
    const void * get(Element_ID id) const
    {
      return operator[](id).look();
    }
    bool get_data(void * buffer,size_t buffer_size,Element_ID id,size_t offset = 0) const
    {
      const Byte * data = (const Byte *) get(id);
      if (data)
      {
        data += offset;
        Byte * dest = (Byte *) buffer;
        for (size_t i = 0; i < buffer_size;i++)
          dest[i] = data[i];
        return true;
      }
      return false;
    }
};

#endif
