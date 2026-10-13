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
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <Tools_Core.h>

struct c_loc {
    struct {
        struct {
            char Layer,Block,Stack;
        } Type;
    } Stack;
};

#define MemMinBlockSize 0x100
#define MemAtomShift 6
#define MemAtomAlignVal (1<<MemAtomShift)
#define MemAtomAlign(v) (((v)+MemAtomAlignVal-1)&(-MemAtomAlignVal))
#define NodAlign 8      /* internal nodes (layer/block/stack) are 8 aligned */

static int CritErrorExit(TlsSignalString *this,char *Msg) {
    (void)this;
    fprintf(stderr,"Critical Error: %s\n",Msg);
    exit(1);
}
static struct TlsSignalString CritErrorExitStatic = {CritErrorExit};
TlsSignalString TlsCritErrorShutdown = {&CritErrorExitStatic};

/*_____________________________________________________________________________
 |
 | no MemPools
 | noPool is the bootstrap pool: plain malloc/free, one bucket per exact size.
 | It follows the TlsMemPool contract, minimally:
 |    - the root is itself a bucket, of the size of the first block of the
 |      stack it serves;
 |    - SizeSelect finds the bucket of exactly that size (0 included), creating it (a small
 |      malloc'd handle, kept until Close) the first time;
 |    - Allocate/Free are malloc/free plus the accounting.
 | There is no reuse of freed blocks: this is a device to build the rest on
 | (and to test it), not a performance pool.
 |
 | A noPool lives at the start of the first block of the stack it serves (see
 | TlsStackOpen), after the room reserved for the stack's first chunk header.
 | Closing the pool frees that block, and so the stack itself.
 |_____________________________________________________________________________
*/

struct noPool;
typedef struct noBucket {
    TlsMemPool TlsMemPool;
    struct noPool *Pool;
    unsigned int sz;
    struct noBucket *next;
} noBucket;
typedef struct noPool {
    noBucket Root;
    noBucket *Buckets;
    TlsIntCounter *SzCount;
    struct {
        void *Chunk;
        int sz;
    } Clean;
} noPool;
static void *noPoolAllocate(TlsMemPool *this) {
    TlsGetThat(noBucket,TlsMemPool);
    void *r = malloc(that->sz?that->sz:1);      /* a 0 size still gets a valid pointer */
    TlsIntCounter *SzCount = that->Pool->SzCount;
    if (!r) CritErrorExit(0,"Out of memory.");
    x_(SzCount,Add,(int)that->sz);
    return r;
}
static void noPoolFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(noBucket,TlsMemPool);
    TlsIntCounter *SzCount = that->Pool->SzCount;
    if (that->sz!=sz)                           /* not the bucket that allocated */
        that = v_(noBucket,TlsMemPool,x_(this,SizeSelect,sz));
    x_(SzCount,Add,-(int)that->sz);
    free(mem);
}
static TlsMemPool *noPoolSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(noBucket,TlsMemPool);
    noPool *p = that->Pool;
    noBucket *b;
    if (p->Root.sz==sz) return &p->Root.TlsMemPool;
    for (b=p->Buckets;b;b=b->next) if (b->sz==sz) return &b->TlsMemPool;
    b = malloc(sizeof(*b));
    if (!b) CritErrorExit(0,"Out of memory.");
    b->TlsMemPool.Static = p->Root.TlsMemPool.Static;
    b->Pool = p; b->sz = sz; b->next = p->Buckets;
    p->Buckets = b;
    return &b->TlsMemPool;
}
static unsigned int noPoolBucketSize(TlsMemPool *this) {
    TlsGetThat(noBucket,TlsMemPool);
    return that->sz;
}
static void noPoolClose(TlsMemPool *this) {
    TlsGetThat(noBucket,TlsMemPool);
    noPool *p = that->Pool;
    noBucket *b,*n;
    for (b=p->Buckets;b;b=n) { n=b->next; free(b); }
    p->Buckets = 0;
    if (p->Clean.Chunk) {
        x_(p->SzCount,Add,-p->Clean.sz);
        free(p->Clean.Chunk);
    }
}
static TlsIntCounter *noPoolSizeCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsIntCounter *r;
    int val=0;
    TlsGetThat(noBucket,TlsMemPool);
    noPool *p = that->Pool;
    if (p->SzCount) {
        val = x_(p->SzCount,Add,0);
        x_(p->SzCount,Add,-val);
    }
    if (!Swp) Swp=p->SzCount;
    r = p->SzCount;
    if (Swp) { x_(Swp,Add,val); }
    p->SzCount = Swp;
    return r;
}
/* Allocate the first block, of size sz. The first hdr bytes are left to the
 * caller (the header of the first chunk); then come the pool, its counter,
 * and *base is set to the first free byte after them. */
static TlsMemPool *NoMemPoolNew(void **chunk,void **base,int sz,int hdr) {
    static struct TlsMemPool Static = {
        noPoolAllocate,noPoolFree,noPoolSizeSelect,noPoolBucketSize,noPoolClose,noPoolSizeCountSwitch
    };
    noPool *r;
    void *b,*e,*p;
    TlsHeap *heap;
    p = b = malloc(sz);
    if (!b) CritErrorExit(0,"Out of memory.");
    e = b+sz;
    p += hdr;
    r = p; p=r+1;
    r->Root.TlsMemPool.Static = &Static;
    r->Root.Pool = r;
    r->Root.sz = sz;
    r->Root.next = 0;
    r->Buckets = 0;
    r->Clean.Chunk = b;
    r->Clean.sz = sz;
    heap  = TlsBufferHeap(p,e);
    TlsHeapIn(heap)
         r->SzCount = TlsIntCounterDirect(sz);
         TlsHeapAlign(16);
         *base = TlsHeap_.p;    /* read before Out: the buffer heap is synced there */
    TlsHeapOut
    *chunk = b;
    return &r->Root.TlsMemPool;
}

/*_____________________________________________________________________________
 |
 | TlsStackNod
 |
 | A stack is a list of chunks of size "growth", each one starting with a
 | StackChunk header. Nodes (layers, blocks, sub-stacks) are pushed in the
 | chunks as they come; those of a chunk are chained from the newest to the
 | oldest starting at Elts, each link holding the offset of its target in the
 | chunk and, in its 2 low bits, the type of its target. 0 ends the chain.
 | The chunks are chained from the newest to the oldest too, thru p.
 |_____________________________________________________________________________
*/

#ifndef NDEBUG
/* Debug: fibers launched and not yet ended or abandoned on this thread (see
 * Tools_Fiber.c). A layer records it, and popping the layer checks that no
 * fiber launched since is still alive: its stack and heap would be freed. */
__thread int TlsFiberLive_;
#endif
struct TlsStackNod {
    unsigned int next;
#ifndef NDEBUG
    int Live;
#endif
};
typedef struct {
    unsigned int type;
    unsigned int next;
} xpdStackNod;
#define StackNodExpand(tgt,src) { unsigned int vl_=(src).next; (tgt).type=vl_&3; (tgt).next=vl_>>2; }
#define StackNodPack(tgt,src) { (tgt).next = ((src).next<<2)+((src).type); }
typedef struct {
    TlsList p;
    TlsStackNod Elts;
} StackChunk;
static StackChunk StackChunkEnd = {{&TlsListEnd},{0}};
struct TlsStack {
    TlsHeap TlsHeap;
    unsigned int growth;        /* bucket size of the chunks (the effective one) */
    TlsMemPool *Pool;           /* root pool */
    TlsMemPool *Chunks;         /* the bucket of the chunks */
    struct { unsigned int sz; TlsMemPool *h; } Blk;   /* last bucket used for blocks */
    TlsIntCounter *MemWatch;
    struct {
        StackChunk *Chunk;
        unsigned int offset;
    } Top;
};
#define StackNodOffset(s,n) ((unsigned int)(((char *)(n))-((char *)(s)->Top.Chunk)))

typedef struct {
    TlsStackNod next;
    void *base;
    unsigned int sz;
} ChunkPartBlock;
typedef struct {
    TlsStackNod next;
    TlsStack *Stack;
} ChunkPartStack;

static void StackHeapSet(TlsHeap *this) {
    TlsGetThat(TlsStack,TlsHeap);
    void *b=that->Top.Chunk;
    TlsHeap_.p = b+that->Top.offset;
    TlsHeap_.e = b+that->growth;
    TlsHeap_.Tgt=this;
}
static void StackHeapSync(TlsHeap *this) {
    TlsGetThat(TlsStack,TlsHeap);
    void *b=that->Top.Chunk;
    that->Top.offset = TlsHeap_.p-b;
}
/* Data is the start of the failed request, the heap p is its end. */
static void *StackHeapOvf(TlsHeap *this,void *Data) {
    TlsGetThat(TlsStack,TlsHeap);
    unsigned int sz = TlsHeap_.p-Data;
    void *b = that->Top.Chunk;
    that->Top.offset = Data-b;      /* forget the failed request ... */
    Data = TlsPush(that,sz);        /* ... redo it: new chunk or block */
    StackHeapSet(this);             /* p and e from the new top */
    return Data;
}
static TlsStack *StackHeapGetStack(TlsHeap *this) {
    TlsGetThat(TlsStack,TlsHeap);
    return that;
}
static struct TlsHeap StackHeapStatic = {
    StackHeapSet,StackHeapSync,StackHeapOvf,StackHeapGetStack
};
TlsHeap *TlsStackHeap(TlsStack *s) { return &s->TlsHeap; }
TlsMemPool *TlsStackPool(TlsStack *s) { return s->Pool; }
/* SizeSelect is costly on real pools: blocks of a same size in a row (the usual
 * case) share the handle found last. */
static TlsMemPool *StackBlockBucket(TlsStack *s,unsigned int sz) {
    if (s->Blk.sz!=sz || !s->Blk.h) {
        TlsMemPool *Pool=s->Pool;
        s->Blk.h = x_(Pool,SizeSelect,sz);
        s->Blk.sz = sz;
    }
    return s->Blk.h;
}
static void StackInit(TlsStack *Stack,TlsMemPool *Pool,TlsMemPool *Chunks,unsigned int growth,TlsIntCounter *MemWatch) {
    Stack->TlsHeap.Static = &StackHeapStatic;
    Stack->growth = growth;
    Stack->Pool = Pool;
    Stack->Chunks = Chunks;
    Stack->Blk.sz = 0; Stack->Blk.h = 0;
    Stack->MemWatch = MemWatch;
    Stack->Top.Chunk = &StackChunkEnd;
    Stack->Top.offset = growth;
}

/* The chunks hold the stack's own structures (headers, links, nodes): the one
 * thing the stack asks of a pool is chunks aligned for them. Blocks, which only
 * hold user data, may have any alignment. */
static void *StackChunkCheck(void *b) {
    if (((uintptr_t)b)&(NodAlign-1)) CritErrorExit(0,"The pool serves stack chunks unaligned.");
    return b;
}
static void StackGrow(TlsStack *s);
static void *StackPushBlock(TlsStack *Stack,unsigned int sz);

void *TlsPush(TlsStack *that,unsigned int sz) {
    void *b,*e,*p;
    b = that->Top.Chunk; e = b+that->growth; p=b+that->Top.offset;
    if (sz<=(e-p)) {
        that->Top.offset += sz;
    } else {
        if (sz>=MemMinBlockSize) {
            p = StackPushBlock(that,sz);
        } else {
            StackGrow(that);
            p = TlsPush(that,sz);
        }
    }
    return p;
}
/* Alignment is on the real address: a pool is not required to serve aligned
 * blocks (its alignment policy may be any, down to bytes). */
static void StackAlignTop(TlsStack *s,unsigned int Align) {
    uintptr_t a = (uintptr_t)s->Top.Chunk+s->Top.offset;
    s->Top.offset += (unsigned int)((-a)&(Align-1));
}
/* Internal nodes are small, pushed aligned, and never go in a block. */
static void *PushNod(TlsStack *that,unsigned int sz) {
    void *p;
    StackAlignTop(that,NodAlign);
    if (that->Top.offset+sz>that->growth) {
        StackGrow(that);
        StackAlignTop(that,NodAlign);
    }
    p = (char *)that->Top.Chunk+that->Top.offset;
    that->Top.offset += sz;
    return p;
}
void TlsPushAlign(TlsStack *that,unsigned int Align) {
    if (Align<that->growth) {
        StackAlignTop(that,Align);
        if (that->Top.offset>=that->growth) {
            StackGrow(that);
            StackAlignTop(that,Align);
        }
    }
}
TlsStack *TlsStackFork(TlsStack *Root) {
    ChunkPartStack *r;
    xpdStackNod r_o;
    r = PushNod(Root,sizeof(*r));
    r_o.type = c_(loc,Stack.Type,Stack);
    r_o.next = StackNodOffset(Root,&r->next);
    r->Stack = TlsStackOpen(Root->Pool,Root->growth,Root->MemWatch);
    r->next.next = Root->Top.Chunk->Elts.next;
    StackNodPack(Root->Top.Chunk->Elts,r_o);
    return r->Stack;
}

typedef struct StackRoot {
    TlsStack TlsStack;
    void (*PoolClose)(struct StackRoot *Rt);
    struct {
        StackChunk *Chunk;
        TlsStackNod *Nod;
    } Bottom;
} StackRoot;
static void StackIntrnPoolClose(struct StackRoot *s) { 
    TlsMemPool *Pool=s->TlsStack.Pool; x_(Pool,Close); 
}
static void StackExtrnPoolClose(struct StackRoot *s) {
    TlsMemPool *Chunks=s->TlsStack.Chunks;
    x_(Chunks,Free,s->Bottom.Chunk,s->TlsStack.growth);
}
/* The first chunk, the root structure (and the pool when there is none) are
 * all in the first block:  [chunk header][noPool, counter][StackRoot] ... */
TlsStack *TlsStackOpen(TlsMemPool *Pool,unsigned int growth,TlsIntCounter *Watch) {
     TlsStack *r;
     StackRoot *R;
     StackChunk *c;
     void *b,*p;
     TlsMemPool *Chunks;
     void (*PoolClose)(struct StackRoot *s);
     if (growth<4*MemMinBlockSize) CritErrorExit(0,"Stack growth is too small.");
     if (!Watch) Watch =  &TlsIntCounterNull;
     if (!Pool) {
         PoolClose = StackIntrnPoolClose;
         Pool = NoMemPoolNew(&b,&p,growth,sizeof(StackChunk));
         Chunks = x_(Pool,SizeSelect,growth);    /* the pool root itself */
     } else {
         PoolClose = StackExtrnPoolClose;
         Chunks = x_(Pool,SizeSelect,growth);
         growth = x_(Chunks,BucketSize);         /* the chunk is the whole bucket */
         b = StackChunkCheck(x_(Chunks,Allocate));
         p = b+sizeof(StackChunk);
     }
     x_(Watch,Add,growth);
     c = b;
     p = (void *)((((uintptr_t)p)+(NodAlign-1))&(-(uintptr_t)NodAlign));
     R = p; p = R+1;
     r = &R->TlsStack;
     StackInit(r,Pool,Chunks,growth,Watch);
     c->p.n = &StackChunkEnd.p;
     c->Elts.next = 0;
     r->Top.Chunk = c;
     r->Top.offset = p-b;
     R->PoolClose = PoolClose;
     R->Bottom.Chunk = c;
     R->Bottom.Nod = TlsStackLayer(r);
     return r;
}
void TlsStackClose(TlsStack *Stck) {
    StackRoot *Rt;
    TlsIntCounter *MemWatch=Stck->MemWatch;
    Rt = v_(StackRoot,TlsStack,Stck);
    TlsStackLayerPop(Stck,Rt->Bottom.Nod);
    x_(MemWatch,Add,-(int)Stck->growth);
    Rt->PoolClose(Rt);
}

static void StackGrow(TlsStack *s) {
    TlsIntCounter *MemWatch=s->MemWatch;
    x_(MemWatch,Add,s->growth>>MemAtomShift);
    void *p,*b;
    StackChunk *c;
    TlsMemPool *Chunks=s->Chunks;
    b = StackChunkCheck(x_(Chunks,Allocate));
    c = b; p = c+1;
    c->p.n = &s->Top.Chunk->p;
    c->Elts.next = 0;
    s->Top.Chunk = c;
    s->Top.offset = p-b;
}

/* Release the nodes of the top chunk down to End. Returns 1 if End is not in
 * this chunk (everything in it has been released, the chunk itself is left to
 * the caller), 0 if End was in it (it is then the new top, End included, popped). */
static int StackChunkReleased(TlsStack *s,struct TlsStackNod *End) {
    void *b,*e;
    unsigned int end;
    struct { TlsStackNod v; xpdStackNod x; } elt;
    StackChunk *chunk;
    chunk = s->Top.Chunk;
    b = chunk;
    e = b+s->growth;
    end = 0;
    { void *ee = End; if (((ee-b)>0) && ((e-ee)>0)) end = ee-b; }
    elt.v = chunk->Elts;
    StackNodExpand(elt.x,elt.v);
    while (elt.x.next!=end) {
        struct TlsStackNod *n=b+elt.x.next;
        switch (elt.x.type) {
        case c_(loc,Stack.Type,Layer): break;
        case c_(loc,Stack.Type,Block): {
            TlsMemPool *MemPool;
            ChunkPartBlock *p = v_(ChunkPartBlock,next,n);
            TlsIntCounter *MemWatch=s->MemWatch;
            x_(MemWatch,Add,-(int)(p->sz>>MemAtomShift));
            MemPool=StackBlockBucket(s,p->sz);
            x_(MemPool,Free,p->base,p->sz);
        } break;
        case c_(loc,Stack.Type,Stack): {
            ChunkPartStack *ps = v_(ChunkPartStack,next,n);
            TlsStackClose(ps->Stack);
        } break;
        default: x_(&TlsCritErrorShutdown,Signal,"Stack unalloc: unknown type."); break;
        }
        elt.v=*n;
        StackNodExpand(elt.x,elt.v);
    }
    if (elt.x.next!=0) {
        struct TlsStackNod *n=b+elt.x.next;
        s->Top.offset = elt.x.next;
        chunk->Elts.next = n->next;
    }
    return (end==0);
}
int TlsStackLayerPop(TlsStack *s,TlsStackNod *End) {
#ifndef NDEBUG
    /* <= and not ==: a fiber launched before the layer may end inside it */
    assert(TlsFiberLive_<=End->Live && "popping a layer that still holds a live fiber");
#endif
    while (s->Top.Chunk!=&StackChunkEnd) {
        StackChunk *chunk;
        TlsIntCounter *MemWatch=s->MemWatch;
        TlsMemPool *Chunks=s->Chunks;
        if (!StackChunkReleased(s,End)) return 1;
        chunk = s->Top.Chunk;
        x_(MemWatch,Add,-(int)(s->growth>>MemAtomShift));
        s->Top.Chunk = v_(StackChunk,p,chunk->p.n);
        x_(Chunks,Free,chunk,s->growth);
    }
    s->Top.offset = s->growth;
    return 0;
}

TlsStackNod *TlsStackLayer(TlsStack *Stack) {
    struct TlsStackNod *r;
    xpdStackNod n;
    n.type = c_(loc,Stack.Type,Layer);
    r = PushNod(Stack,sizeof(*r));
    r->next = Stack->Top.Chunk->Elts.next;
#ifndef NDEBUG
    r->Live = TlsFiberLive_;
#endif
    n.next = StackNodOffset(Stack,r);
    StackNodPack(Stack->Top.Chunk->Elts,n);
    return r;
}

static void *StackPushBlock(TlsStack *Stack,unsigned int sz) {
    ChunkPartBlock *r;
    xpdStackNod n;
    TlsMemPool *MemPool;
    TlsIntCounter *MemWatch=Stack->MemWatch;
    n.type = c_(loc,Stack.Type,Block);
    sz = MemAtomAlign(sz);
    r = PushNod(Stack,sizeof(*r));
    x_(MemWatch,Add,sz>>MemAtomShift);
    MemPool = StackBlockBucket(Stack,sz);
    r->base = x_(MemPool,Allocate);
    r->sz = sz;
    r->next.next = Stack->Top.Chunk->Elts.next;
    n.next = StackNodOffset(Stack,&r->next);
    StackNodPack(Stack->Top.Chunk->Elts,n);
    return r->base;
}

/*_____________________________________________________________________________
 |
 | Heap Buffer
 |_____________________________________________________________________________
*/
typedef struct {
    TlsHeap TlsHeap;
    void *p,*e;
} HBuffer;
static void HBufferSet(TlsHeap *this) {
    TlsGetThat(HBuffer,TlsHeap);
    TlsHeap_.Tgt = this;
    TlsHeap_.p = that->p;
    TlsHeap_.e = that->e;
}
static void HBufferSync(TlsHeap *this) {
    TlsGetThat(HBuffer,TlsHeap);
    that->p = TlsHeap_.p;
    that->e = TlsHeap_.e;
}
static void *HBufferOvf(TlsHeap *this,void *Data) {
    (void)this;
    CritErrorExit(0,"Heap Overflow as it is set as a local Buffer.");
    return Data;
}
static TlsStack *HBufferGetStack(TlsHeap *this) {
    (void)this;
    CritErrorExit(0,"Trying to cast a simple buffer as a stack.");
    return 0;
}
TlsHeap *TlsBufferHeap(void *b,void *e) {
    HBuffer *r;
    static struct TlsHeap Static = {HBufferSet,HBufferSync,HBufferOvf,HBufferGetStack};
    r = b; b = r+1;
    r->TlsHeap.Static = &Static;
    r->p = b; r->e = e;
    return &r->TlsHeap;
}

/*_____________________________________________________________________________
 |
 | Null Heap: the heap installed when there is none, so that TlsHeap_.Tgt is
 | never NULL. Syncing it does nothing, allocating on it is a fatal error.
 |_____________________________________________________________________________
*/
static void NullHeapSet(TlsHeap *this) {
    TlsHeap_.Tgt = this;
    TlsHeap_.p = TlsHeap_.e = 0;
}
static void NullHeapSync(TlsHeap *this) { (void)this; }
static void *NullHeapOvf(TlsHeap *this,void *Data) {
    (void)this;
    CritErrorExit(0,"Allocation while no heap is installed.");
    return Data;
}
static TlsStack *NullHeapGetStack(TlsHeap *this) {
    (void)this;
    CritErrorExit(0,"No heap is installed.");
    return 0;
}
static struct TlsHeap NullHeapStatic = {
    NullHeapSet,NullHeapSync,NullHeapOvf,NullHeapGetStack
};
static TlsHeap NullHeap = {&NullHeapStatic};

/*_____________________________________________________________________________
 |
 | Heap
 |_____________________________________________________________________________
*/

__thread struct TlsHeapHead TlsHeap_={&NullHeap,0,0};

static void HeapAlignP(unsigned int Align) {
    uintptr_t v = (uintptr_t)TlsHeap_.p;
    v = (v+(Align-1))&~(uintptr_t)(Align-1);
    TlsHeap_.p = (void *)v;
}
/* Align must be a power of 2, well below MemMinBlockSize. */
void TlsHeapAlign(unsigned int Align) {
    HeapAlignP(Align);
    if ((TlsHeap_.e-TlsHeap_.p)<=0) {       /* no byte left once aligned: next chunk */
        void *e = TlsHeap_.e;
        TlsHeap_.p = e+1;                   /* a 1 byte request at the very end ... */
        x_(TlsHeap_.Tgt,Ovf,e);             /* ... moves on to a new chunk, unaligned */
        HeapAlignP(Align);
    }
}

TlsStackNod *TlsHeapLayer(void) {
    TlsStack *s = x_(TlsHeap_.Tgt,GetStack);
    x_(TlsHeap_.Tgt,Sync);
    TlsStackNod *r = TlsStackLayer(s);
    x_(TlsHeap_.Tgt,Set);
    return r;
}
int TlsHeapLayerPop(TlsStackNod *l) {
    TlsStack *s = x_(TlsHeap_.Tgt,GetStack);
    x_(TlsHeap_.Tgt,Sync);
    int r = TlsStackLayerPop(s,l);
    x_(TlsHeap_.Tgt,Set);
    return r;
}
/* The new heap is not installed: use x_(r,Set) or TlsHeapIn. */
TlsHeap *TlsHeapFork(void) {
    TlsStack *s = x_(TlsHeap_.Tgt,GetStack);
    x_(TlsHeap_.Tgt,Sync);
    TlsStack *n = TlsStackFork(s);
    TlsHeap *r = &n->TlsHeap;
    x_(TlsHeap_.Tgt,Set);
    return r;
}
/* Only for a heap that was opened as a root (not a forked one, which is closed
 * by the layer pop that pops its parent). */
void TlsHeapClose(TlsHeap *Heap) {
    TlsStack *s = x_(Heap,GetStack);
    if (TlsHeap_.Tgt==Heap) x_(&NullHeap,Set);
    TlsStackClose(s);
}

void TlsMainHeapOpen(int growth) {
    TlsStack *s = TlsStackOpen(0,growth,&TlsIntCounterNull);
    TlsHeap *hp = TlsStackHeap(s);
    x_(hp,Set);
}
void TlsMainHeapClose(void) {
    TlsStack *s;
    s = x_(TlsHeap_.Tgt,GetStack);
    x_(&NullHeap,Set);
    TlsStackClose(s);
}
