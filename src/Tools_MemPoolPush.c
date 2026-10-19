/*  Fondement Michtam
 *  Copyright (C) 2024 Xavier Lacroix
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <Tools_Core.h>
 
/*_____________________________________________________________________________
 |
 | TlsMemPool *TlsMemPoolPush(void)
 |
 | A local MemPool on the current heap, for the punctual moments when the
 | stack discipline is not enough (data that dies in any order).
 |
 | It forks the current heap and lives entirely in that fork:
 |
 |     TlsStackNod *l = TlsHeapLayer();
 |     TlsMemPool *p = TlsMemPoolPush();
 |     ... x_(h,Allocate) / x_(h,Free,...) ...
 |     TlsHeapLayerPop(l);          // everything is gone, the pool included
 |
 | Popping the layer that precedes the call is the normal way to end it: the
 | fork is closed with its parent layer, and all its memory with it. Calling
 | x_(p,Close) is allowed too: it releases everything the pool holds at once
 | (the pool must not be used afterwards), and the small remains (the fork
 | itself) go with the parent layer. Without a layer before, the pool lives as
 | long as the heap it was created on.
 |
 | Layout. Small data (up to PushMiniMax bytes) goes thru a TlsMemPoolMini
 | (tight) that takes its pages from a stack backed pool, bigger sizes are
 | served by that pool directly. The stack backed pool only ever grows: the
 | blocks given back are recycled within the pool, never returned to the heap
 | before the layer pop. That is the intended trade: simple, fast, and a clean
 | slate in bulk.
 |
 | The stack backed pool follows the TlsMemPool contract and the alignment
 | rule of Tools_Core.h. Its sizes are rounded up to a multiple of 16 (8 up to
 | 8). The counter (CountSwitch) tracks the bytes held by the pool; it is
 | zeroed when the pool ends, by Close or by the layer pop.
 |
 | Not thread safe: it lives in the heap of one thread.
 |_____________________________________________________________________________
*/
 
#ifndef PushMiniMax
#define PushMiniMax 256
#endif
 
TlsMemPool *TlsMemPoolMini(TlsMemPool *Parent,int MaxSize,int tight);
 
struct SPool;
typedef struct SBucket {
    TlsMemPool TlsMemPool;
    struct SPool *P;
    unsigned int Size;
    void *Free;                 /* recycled blocks, linked thru their first word */
    struct SBucket *Next;
} SBucket;
typedef struct SPool {
    SBucket Root;               /* size 0 */
    SBucket *Buckets,*Last;
    TlsStack *S;
    TlsStackNod *Layer;
    TlsIntCounter *Count;
    long Held;                  /* bytes held, for the counter */
    char Dummy;                 /* address given for 0 sized data */
} SPool;
 
static struct TlsMemPool SStatic;
 
/* The rule: powers of 2 on themselves, otherwise on their lowest bit, at most 64 */
static unsigned int AlignOf(unsigned int sz) {
    unsigned int low = sz&-sz;
    if (low==sz) return sz;
    return low>64?64:low;
}
static unsigned int RoundUp(unsigned int sz) {
    if (!sz) return 0;
    if (sz<=8) return 8;
    return (sz+15)&~15u;
}
 
static void *PushAligned(SPool *P,unsigned int sz) {
    unsigned int al = AlignOf(sz);
    void *p;
    if (al>1) TlsPushAlign(P->S,al);
    p = TlsPush(P->S,sz);
    if (((uintptr_t)p)&(al-1)) {            /* the stack could not align it (big alignments) */
        p = TlsPush(P->S,sz+al);
        p = (void *)((((uintptr_t)p)+al-1)&~(uintptr_t)(al-1));
    }
    return p;
}
static void *SAllocate(TlsMemPool *this) {
    TlsGetThat(SBucket,TlsMemPool);
    SPool *P = that->P;
    void *r;
    if (!that->Size) return &P->Dummy;
    r = that->Free;
    if (r) that->Free = *(void **)r;
    else r = PushAligned(P,that->Size);
    P->Held += that->Size;
    x_(P->Count,Add,(int)that->Size);
    return r;
}
static TlsMemPool *SSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(SBucket,TlsMemPool);
    SPool *P = that->P;
    SBucket *b;
    sz = RoundUp(sz);
    if (!sz) return &P->Root.TlsMemPool;
    if (P->Last && P->Last->Size==sz) return &P->Last->TlsMemPool;
    for (b=P->Buckets;b;b=b->Next) if (b->Size==sz) goto found;
    b = TlsPush(P->S,sizeof(*b)+8);
    b = (SBucket *)((((uintptr_t)b)+7)&~(uintptr_t)7);
    b->TlsMemPool.Static = &SStatic;
    b->P = P; b->Size = sz; b->Free = 0;
    b->Next = P->Buckets; P->Buckets = b;
found:
    P->Last = b;
    return &b->TlsMemPool;
}
static void SFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(SBucket,TlsMemPool);
    SPool *P = that->P;
    if (RoundUp(sz)!=that->Size) {
        TlsMemPool *h = x_(this,SizeSelect,sz);
        that = v_(SBucket,TlsMemPool,h);
    }
    if (!that->Size) return;
    *(void **)mem = that->Free;
    that->Free = mem;
    P->Held -= that->Size;
    x_(P->Count,Add,-(int)that->Size);
}
static unsigned int SBucketSize(TlsMemPool *this) {
    TlsGetThat(SBucket,TlsMemPool);
    return that->Size;
}
static void SEnd(void *arg) {               /* hook: runs when the layer is popped */
    SPool *P = arg;
    x_(P->Count,Add,-(int)P->Held);
    P->Held = 0;
}
static void SClose(TlsMemPool *this) {
    TlsGetThat(SBucket,TlsMemPool);
    SPool *P = that->P;
    TlsStackLayerPop(P->S,P->Layer);        /* runs SEnd, frees all the pool's memory */
}
static TlsIntCounter *SCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(SBucket,TlsMemPool);
    SPool *P = that->P;
    TlsIntCounter *r = P->Count;
    int val = (int)P->Held;
    x_(r,Add,-val);
    if (!Swp) Swp = r;
    x_(Swp,Add,val);
    P->Count = Swp;
    return r;
}
static struct TlsMemPool SStatic = {
    SAllocate,SFree,SSizeSelect,SBucketSize,SClose,SCountSwitch
};
 
TlsMemPool *TlsMemPoolPush(void) {
    TlsHeap *fh = TlsHeapFork();
    TlsStack *S = x_(fh,GetStack);
    SPool *P;
    TlsStackNod *lay;
    TlsPushAlign(S,8);
    P = TlsPush(S,sizeof(*P));
    memset(P,0,sizeof(*P));
    P->Root.TlsMemPool.Static = &SStatic;
    P->Root.P = P;
    P->S = S;
    P->Count = &TlsIntCounterNull;
    lay = TlsStackLayer(S);                 /* all the pool's memory is above this layer */
    P->Layer = lay;
    TlsStackHook(S,SEnd,P);
    return TlsMemPoolMini(&P->Root.TlsMemPool,PushMiniMax,1);
}
 
