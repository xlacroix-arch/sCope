/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Dict_h_
#define _Tools_Dict_h_

#include <Tools_Core.h>

/*_____________________________________________________________________________
 |
 | Dict: gives a number to every label met, so that what uses them deals with
 | numbers. The same label always gets the same number: 1 for the first one met,
 | then 2, 3 ... (0 is never a label, it is "none"). A label is any sequence of
 | bytes that holds no 0 (it may be empty).
 |
 |   Id(d,s,n)     the number of the label s[0..n[, created if it is new
 |   Find(d,s,n)    its number, or 0 if it was never met (nothing is created)
 |   Name(d,id,&n)  the label of a number, 0 terminated, and its length in n (n
 |                  may be NULL); NULL if id is not a number of this dictionary.
 |                  The address stays valid as long as the dictionary does.
 |   Count(d)       how many labels there are
 |
 | Everything the dictionary owns (its tables, its labels) is pushed on ONE heap,
 | the one given to TlsDictNew (NULL: a fork of the current heap, which lives as
 | long as the layer that holds the call). The dictionary pushes there whatever
 | heap is current when it grows, and puts the current one back. It only grows: a
 | label is never forgotten, and the room of the hash table and of the numbers
 | that was left behind when they grew (less than what is in use) stays until the
 | heap goes. There is no lock: it is for one thread at a time, wrap it in a
 | filter if that changes.
 |_____________________________________________________________________________
*/

TlsClassDef(TlsDict) {
    int (*Id)(TlsDict *this,unsigned char *s,int n);
    int (*Find)(TlsDict *this,unsigned char *s,int n);
    unsigned char *(*Name)(TlsDict *this,int id,int *n);
    int (*Count)(TlsDict *this);
};

TlsDict *TlsDictNew(TlsHeap *Where);

/*_____________________________________________________________________________
 |
 | IdMap: what a Dict number stands for, for one user (a macro library, a table
 | of keywords ...): Get(m,id) gives the pointer Set(m,id,v) stored, or NULL.
 | It grows with the numbers, in pages, on the heap given to TlsIdMapNew (NULL:
 | a fork of the current heap).
 |_____________________________________________________________________________
*/
TlsClassDef(TlsIdMap) {
    void *(*Get)(TlsIdMap *this,int id);
    void (*Set)(TlsIdMap *this,int id,void *v);
};
TlsIdMap *TlsIdMapNew(TlsHeap *Where);

#endif
