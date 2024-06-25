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


// $Log: ltfsa.cpp $
// Revision 1.6  2010/06/10 13:57:27Z  Alun
// All tabs removed again
// Revision 1.5  2010/01/21 21:40:37Z  Alun
// Code made less generic, as it now needs to know about Node_ID
// Revision 1.4  2009/09/13 12:35:12Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.3  2008/09/30 10:15:40Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
//

#include <limits.h>
#include <string.h>
#include "awcc.h"
#include "ltfsa.h"

class Label_Compressor
{
  private:
    Transition_ID nr_symbols;
    const size_t mask_size;
  public:
    Node_ID * new_labels;
    const size_t buffer_size;
    Byte * const cdata;
  public:
    Label_Compressor(Transition_ID nr_symbols);
    ~Label_Compressor();
    size_t compress(const Node_ID * buffer) const;
    void decompress(Node_ID * buffer,const Byte * cdata) const;
};

Label_Compressor::Label_Compressor(Transition_ID nr_symbols_) :
  nr_symbols(nr_symbols_),
  mask_size((nr_symbols+2+CHAR_BIT-1)/CHAR_BIT),
  new_labels(new Node_ID[nr_symbols_]),
  buffer_size(mask_size+sizeof(Node_ID)*nr_symbols_),
  cdata(new Byte[buffer_size])
{
}

/**/

Label_Compressor::~Label_Compressor()
{
  delete [] new_labels;
  delete [] (Byte *) cdata;
}

/**/

void Label_Compressor::decompress(Node_ID * buffer,
                                  const Byte * cdata) const
{
  /* We support three formats for storing labels, stored in
     the lowest two bits of the first byte of the compressed
     label information.

     format 0  - a dense row of labels.

     format 1  - a bitmask beginning in the third bit of the first byte
     with a 1 for non null labels and a 0 for null labels.
     The non null labels are listed after the bit mask.

     format 2 - an array of byte/Node_ID pairs terminated with byte value
     255. The byte indicates a following number of null labels that
     are followed by a single void *. The state might be zero in the
     case where there is a gap of more than 254 labels between non
     null labels.

     Another possible format would be byte byte void **n where the first
     byte is interpreted as in format 2 and the second byte is n, a number
     of consecutive labels. */

  if (!cdata)
  {
    for (Transition_ID ti = 0;ti < nr_symbols;ti++)
      buffer[ti] = 0;
    return;
  }
  int format = cdata[0] & 3;

  if (format == 1)
  {
    Transition_ID j = 0;
    for (Transition_ID i = 0; i < nr_symbols;i++)
      if (cdata[(i+2)/CHAR_BIT] & (1 << ((i+2) % CHAR_BIT)))
        memcpy(buffer+i,cdata+mask_size+j++*sizeof(Node_ID),sizeof(Node_ID));
      else
        buffer[i] = 0;
  }
  else if (format == 2)
  {
    Transition_ID j = 0;
    cdata++;
    while (*cdata != UCHAR_MAX)
    {
      for (Transition_ID ti = 0;ti < *cdata;ti++)
        buffer[j++] = 0;
      cdata++;
      memcpy(buffer+j,cdata,sizeof(Node_ID));
      j++;
      cdata += sizeof(Node_ID);
    }
    memset(buffer+j,0,(nr_symbols-j)*sizeof(Node_ID));
  }
  else
    memcpy(buffer,cdata+1,sizeof(Node_ID)*nr_symbols);
}

/**/

size_t Label_Compressor::compress(const Node_ID * buffer) const
{
  Transition_ID j = 0;
  int count = 0;
  int gap = 0;
  Node_ID zero = 0;

  *cdata = 0;
  for (Transition_ID i = 0; i < nr_symbols;i++)
    if (buffer[i])
    {
      cdata[ (i+2)/CHAR_BIT] |= 1 << ((i+2) % CHAR_BIT);
      new_labels[j++] = buffer[i];
      while (gap >= UCHAR_MAX)
      {
        count++;
        gap -= UCHAR_MAX;
      }
      gap = 0;
      count++;
    }
    else
    {
      cdata[ (i+2)/CHAR_BIT] &= ~(1 << ((i+2) % CHAR_BIT));
      gap++;
    }
  size_t size = mask_size + j*sizeof(Node_ID);
  if (1 + count*(1+sizeof(Node_ID)) + 1 < size)
  {
    Byte * t = cdata;
    *t++ = 2;
    gap = 0;
    for (Transition_ID i = 0; i < nr_symbols;i++)
      if (buffer[i])
      {
        while (gap >= UCHAR_MAX)
        {
          *t = UCHAR_MAX-1;
          t++;
          memcpy(t,&zero,sizeof(Node_ID));
          t += sizeof(Node_ID);
          gap -= UCHAR_MAX;
        }
        *t++ = Byte(gap);
        memcpy(t,buffer+i,sizeof(Node_ID));
        t += sizeof(Node_ID);
        gap = 0;
      }
      else
        gap++;
    *t++ = UCHAR_MAX;
    size = t - cdata;
  }
  else if (1 + nr_symbols*sizeof(Node_ID) < size)
  {
    *cdata = 0;
    memcpy(cdata+1,buffer,size = nr_symbols*sizeof(Node_ID));
    size++;
  }
  else
  {
    *cdata |= 1;
    memcpy(cdata+mask_size,new_labels,j*sizeof(Node_ID));
  }
  return size;
}

/**/

LTFSA::LTFSA(Container & container,const Alphabet & alphabet,
             Transition_ID nr_symbols_,int hash_size,size_t key_size,
             bool compressed, size_t cache_size) :
  Keyed_FSA(container,alphabet,nr_symbols_,hash_size,key_size,compressed,
            cache_size),
  label_state(0)
{
  compressor = new Label_Compressor(nr_symbols_);
  current_labels = new Node_ID [nr_symbols_];
  manage(transition_labels);
}

/**/

LTFSA::~LTFSA()
{
  delete compressor;
  delete current_labels;
}

/**/

Node_ID LTFSA::get_transition_label(State_ID state,Transition_ID transition) const
{
  if (label_state != state)
  {
    label_state = state;
    compressor->decompress(current_labels,(const Byte *) transition_labels.get(state));
  }
  return current_labels[transition];
}

/**/

bool LTFSA::set_transition_label(State_ID state,Transition_ID transition,
                                 Node_ID label)
{
  if (label_state != state)
  {
    label_state = state;
    compressor->decompress(current_labels,(const Byte *) transition_labels.get(state));
  }
  current_labels[transition] = label;
  size_t size = compressor->compress(current_labels);
  return transition_labels.set_data(state,compressor->cdata,size);
}
