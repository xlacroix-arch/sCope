/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix, Coded by Claude Sonnet 5.5
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
 
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <Tools_Core.h>
 
/*_____________________________________________________________________________
 |
 | MemPool filters: pools that sit on top of another TlsMemPool.
 |
 | Prototypes (to go in Tools_Core.h):
 |    TlsMemPool *TlsMemPoolShared(TlsMemPool *child);
 |    TlsMemPool *TlsMemPoolBuffered(TlsMemPool *child,unsigned int buffersize);
 | Introspection (to go in Tools_Core_Debug.h):
 |    unsigned long TlsMemPoolSharedLocks(TlsMemPool *Handle);
 |
 | Both return the root handle of a new pool whose buckets are handles wrapping
 | those of the child: same sizes, same BucketSize, SizeSelect keeps its
 | contract (stable handles, fixed point on BucketSize).
 |
 | SHARED. Makes the child usable by several threads: every call that reaches the
 | child (Allocate, Free, SizeSelect, CountSwitch) is made under one lock, a
 | futex mutex (Linux). The lock is per pool, not per bucket: the child is
 | free to share state between its buckets (the main pool does: its class
 | table, its chunk source, its counter). The counter of the child is updated
 | under the lock too, so it need not be atomic.
 |    - Close(root) closes the child: the shared pool owns it. Nobody may use
 |      the pool, or a handle of it, while it is closed.
 |    - Every Allocate/Free costs a lock round trip: that is what Buffered is
 |      for.
 |
 | BUFFERED. A front for ONE thread (it is not thread safe by itself: one
 | Buffered per thread, over a Shared) that keeps free blocks of the buckets
 | it has been asked for, so that most Allocate and Free are answered without
 | reaching the child. The child is reached
 |    - to refill an empty bucket, by a batch of blocks taken in a single
 |      access (one lock round trip when the child is a Shared pool);
 |    - to flush the buffer when it holds too much, in a single access too.
 | buffersize is the bytes of free blocks the buffer may hold, all buckets
 | counted together; it is never exceeded.
 |    - A batch is a quarter of the buffer (at least 2 blocks, at most
 |      BatchMax); a flush goes down to half of it: so a thread that allocates
 |      or frees in a steady state enters the child once every
 |      min(BatchMax,buffersize/4/size) operations, and never at a boundary
 |      crossing back and forth.
 |    - Blocks above a quarter of the buffer are not buffered: the call goes
 |      straight to the child. Same for the 0-size bucket.
 |    - Free blocks are linked in their first word: a bucket serves at least
 |      sizeof(void *) bytes. SizeSelect of 1..7 gives the bucket of 8 (its
 |      BucketSize is 8, SizeSelect(h,8)==h still holds).
 |    - The counter of a Buffered pool (CountSwitch) accounts what the
 |      application got thru it, not what the buffer holds. The child has its
 |      own.
 |    - Close(root) gives every held block back to the child, and frees the
 |      buffer. It does NOT close the child: it is shared with the other
 |      Buffered of the other threads. Close the buffer before the child, and
 |      only once its blocks are given back to it, or not at all.
 |    - The child may be any pool. When it is a Shared, the buffer sees it
 |      and brackets its batches with a single lock, and calls the unprotected
 |      handles inside; with another pool a batch is a series of plain calls.
 |    - Blocks held in a buffer are not free for the other threads, and they
 |      keep their chunks in use: size buffersize for the working set of a
 |      thread, not more.
 |_____________________________________________________________________________
 */
 
#define DirN 128                            /* hash of the wrappers, by child handle */
#define BatchMax 128                        /* blocks taken by one refill at most */
#define MinBlock sizeof(void *)             /* a free block holds a link */
 
static void Crit(char *Msg) {
    x_(&TlsCritErrorShutdown,Signal,Msg);
    abort();
}
 
/*_____________________________________________________________________________
 |
 | The lock: futex based, 0 free, 1 taken, 2 taken and somebody waits.
 | Taking it is counted (under the lock itself), for the tests.
 |_____________________________________________________________________________
*/
 
typedef struct {
    atomic_uint v;
    unsigned long n;
} Lock;
 
static void FutexWait(atomic_uint *a,unsigned val) { syscall(SYS_futex,a,FUTEX_WAIT_PRIVATE,val,0,0,0); }
static void FutexWake(atomic_uint *a) { syscall(SYS_futex,a,FUTEX_WAKE_PRIVATE,1,0,0,0); }
 
static void LockTake(Lock *l) {
    unsigned c = 0;
    int i;
    for (i=0;i<64;i++) {                    /* short critical sections: spin first */
        if (atomic_load_explicit(&l->v,memory_order_relaxed)==0) {
            c = 0;
            if (atomic_compare_exchange_weak_explicit(&l->v,&c,1,memory_order_acquire,memory_order_relaxed)) {
                l->n++;
                return;
            }
        }
    }
    c = atomic_exchange_explicit(&l->v,2,memory_order_acquire);
    while (c!=0) {
        FutexWait(&l->v,2);
        c = atomic_exchange_explicit(&l->v,2,memory_order_acquire);
    }
    l->n++;
}
static void LockDrop(Lock *l) {
    if (atomic_fetch_sub_explicit(&l->v,1,memory_order_release)!=1) {
        atomic_store_explicit(&l->v,0,memory_order_release);
        FutexWake(&l->v);
    }
}
 
/*_____________________________________________________________________________
 |
 | Wrappers: one per child handle, found by a hash of the handle.
 |_____________________________________________________________________________
*/
 
typedef struct FBucket FBucket;
struct FBucket {
    TlsMemPool TlsMemPool;
    TlsMemPool *Child;                      /* the handle this one stands for */
    uint32_t Size;                          /* BucketSize of the child */
    FBucket *Chain,*All;
};
typedef struct {
    FBucket *Tab[DirN];
    FBucket *All;
} Dir;
 
static unsigned DirHash(TlsMemPool *h) {
    uintptr_t v = (uintptr_t)h>>4;
    return (unsigned)((v^(v>>7)^(v>>14))%DirN);
}
static FBucket *DirFind(Dir *d,TlsMemPool *ch) {
    FBucket *b;
    for (b=d->Tab[DirHash(ch)];b && b->Child!=ch;b=b->Chain) ;
    return b;
}
static void DirAdd(Dir *d,FBucket *b) {
    unsigned h = DirHash(b->Child);
    b->Chain = d->Tab[h]; d->Tab[h] = b;
    b->All = d->All; d->All = b;
}
 
/*_____________________________________________________________________________
 |
 | Shared
 |_____________________________________________________________________________
*/
 
typedef struct SPool SPool;
typedef struct {
    FBucket B;
    SPool *P;
} SBucket;
struct SPool {
    Lock L;
    Dir D;
    TlsMemPool *Child;
    SBucket *Root;
};
 
static struct TlsMemPool SharedStatic;
 
/* Called with the lock held (or before the pool is published). */
static SBucket *SWrap(SPool *P,TlsMemPool *ch) {
    SBucket *b = (SBucket *)DirFind(&P->D,ch);
    if (b) return b;
    b = calloc(1,sizeof(*b));
    if (!b) Crit("Out of Memory");
    b->B.TlsMemPool.Static = &SharedStatic;
    b->B.Child = ch;
    b->B.Size = x_(ch,BucketSize);
    b->P = P;
    DirAdd(&P->D,&b->B);
    return b;
}
 
static void *SharedAllocate(TlsMemPool *this) {
    TlsGetThat(SBucket,B.TlsMemPool);
    void *r;
    LockTake(&that->P->L);
    r = x_(that->B.Child,Allocate);
    LockDrop(&that->P->L);
    return r;
}
/* The child sorts out a size that is not the one of its bucket. */
static void SharedFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(SBucket,B.TlsMemPool);
    LockTake(&that->P->L);
    x_(that->B.Child,Free,mem,sz);
    LockDrop(&that->P->L);
}
static TlsMemPool *SharedSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(SBucket,B.TlsMemPool);
    SPool *P = that->P;
    SBucket *w;
    LockTake(&P->L);
    w = SWrap(P,x_(that->B.Child,SizeSelect,sz));
    LockDrop(&P->L);
    return &w->B.TlsMemPool;
}
static unsigned int SharedBucketSize(TlsMemPool *this) {
    TlsGetThat(SBucket,B.TlsMemPool);
    return that->B.Size;
}
static void SharedClose(TlsMemPool *this) {
    TlsGetThat(SBucket,B.TlsMemPool);
    SPool *P = that->P;
    FBucket *b,*n;
    for (b=P->D.All;b;b=n) { n = b->All; free(b); }
    x_(P->Child,Close);
    free(P);
}
static TlsIntCounter *SharedCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(SBucket,B.TlsMemPool);
    TlsIntCounter *r;
    LockTake(&that->P->L);
    r = x_(that->B.Child,CountSwitch,Swp);
    LockDrop(&that->P->L);
    return r;
}
static struct TlsMemPool SharedStatic = {
    SharedAllocate,SharedFree,SharedSizeSelect,SharedBucketSize,SharedClose,SharedCountSwitch
};
 
TlsMemPool *TlsMemPoolShared(TlsMemPool *child) {
    SPool *P = calloc(1,sizeof(*P));
    if (!P) Crit("Out of Memory");
    P->Child = child;
    P->Root = SWrap(P,child);
    return &P->Root->B.TlsMemPool;
}
unsigned long TlsMemPoolSharedLocks(TlsMemPool *this) {
    TlsGetThat(SBucket,B.TlsMemPool);
    return that->P->L.n;
}
 
/* For the buffer: if h is a handle of a Shared, the lock of its pool is put in
 * *L and the handle it wraps is returned (to be used under that lock);
 * else 0. */
static TlsMemPool *SharedUnwrap(TlsMemPool *h,Lock **L) {
    SBucket *b;
    if (h->Static!=&SharedStatic) return 0;
    b = v_(SBucket,B.TlsMemPool,h);
    *L = &b->P->L;
    return b->B.Child;
}
 
/*_____________________________________________________________________________
 |
 | Buffered
 |_____________________________________________________________________________
*/
 
typedef struct BPool BPool;
typedef struct {
    FBucket B;
    BPool *P;
    TlsMemPool *Under;                      /* where blocks come from and go to, under the lock if any */
    void *Free;                             /* cached blocks, linked by their first word */
    uint32_t NFree;
    uint32_t Batch;                         /* blocks per refill; 0: not buffered */
    uint32_t SzOk;                          /* last size checked to belong to this bucket */
} BBucket;
struct BPool {
    Dir D;
    TlsMemPool *Child;
    Lock *L;                                /* the lock of the child when it is a Shared */
    uint64_t Budget,Held;                   /* bytes */
    TlsIntCounter *Count;
    BBucket *Root;
};
 
static struct TlsMemPool BufferedStatic;
 
static void LinkSet(void *blk,void *next) { memcpy(blk,&next,sizeof(next)); }
static void *LinkGet(void *blk) { void *n; memcpy(&n,blk,sizeof(n)); return n; }
 
static void Hold(BPool *P) { if (P->L) LockTake(P->L); }
static void Unhold(BPool *P) { if (P->L) LockDrop(P->L); }
 
static BBucket *BWrap(BPool *P,TlsMemPool *ch) {
    BBucket *b = (BBucket *)DirFind(&P->D,ch);
    TlsMemPool *raw;
    Lock *l;
    uint64_t n;
    if (b) return b;
    b = calloc(1,sizeof(*b));
    if (!b) Crit("Out of Memory");
    b->B.TlsMemPool.Static = &BufferedStatic;
    b->B.Child = ch;
    b->B.Size = x_(ch,BucketSize);
    b->P = P;
    raw = SharedUnwrap(ch,&l);
    b->Under = raw ? raw : ch;
    b->SzOk = b->B.Size;
    if (b->B.Size==0 || b->B.Size>P->Budget/4) {
        b->Batch = 0;
    } else {
        n = (P->Budget/4)/b->B.Size;
        if (n<2) n = 2;
        if (n>BatchMax) n = BatchMax;
        b->Batch = (uint32_t)n;
    }
    DirAdd(&P->D,&b->B);
    return b;
}
 
/* Gives back to the child the cached blocks of the buckets, those of `first`
 * (if not 0) before the others, until no more than `target` bytes are held. */
static void FlushOne(BPool *P,BBucket *b,uint64_t target) {
    while (b->Free && P->Held>target) {
        void *blk = b->Free;
        b->Free = LinkGet(blk);
        b->NFree--;
        P->Held -= b->B.Size;
        x_(b->Under,Free,blk,b->B.Size);
    }
}
static void Flush(BPool *P,BBucket *first,uint64_t target) {
    FBucket *f;
    if (P->Held<=target) return;
    Hold(P);                                /* one access for the whole flush */
    if (first) FlushOne(P,first,target);
    for (f=P->D.All;f && P->Held>target;f=f->All) FlushOne(P,(BBucket *)f,target);
    Unhold(P);
}
 
static void *Refill(BBucket *b) {
    BPool *P = b->P;
    uint32_t n = b->Batch,i;
    void *r;
    if (P->Held+(uint64_t)(n-1)*b->B.Size>P->Budget) Flush(P,0,P->Budget/2);
    Hold(P);
    r = x_(b->Under,Allocate);
    for (i=1;i<n;i++) {
        void *q = x_(b->Under,Allocate);
        LinkSet(q,b->Free);
        b->Free = q;
    }
    Unhold(P);
    b->NFree += n-1;
    P->Held += (uint64_t)(n-1)*b->B.Size;
    return r;
}
 
static void *BufferedAllocate(TlsMemPool *this) {
    TlsGetThat(BBucket,B.TlsMemPool);
    BPool *P = that->P;
    void *r;
    if (!that->Batch) {
        r = x_(that->B.Child,Allocate);
    } else if (that->Free) {
        r = that->Free;
        that->Free = LinkGet(r);
        that->NFree--;
        P->Held -= that->B.Size;
    } else {
        r = Refill(that);
    }
    x_(P->Count,Add,(int)that->B.Size);
    return r;
}
static void BufferedFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(BBucket,B.TlsMemPool);
    BPool *P = that->P;
    if (sz!=that->B.Size && sz!=that->SzOk) {   /* maybe not the bucket of sz: let it do */
        TlsMemPool *h = x_(this,SizeSelect,sz);
        if (h!=this) { x_(h,Free,mem,sz); return; }
        that->SzOk = sz;
    }
    x_(P->Count,Add,-(int)that->B.Size);
    if (!that->Batch) {
        x_(that->B.Child,Free,mem,that->B.Size);
        return;
    }
    LinkSet(mem,that->Free);
    that->Free = mem;
    that->NFree++;
    P->Held += that->B.Size;
    if (P->Held>P->Budget) Flush(P,that,P->Budget/2);
}
static TlsMemPool *BufferedSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(BBucket,B.TlsMemPool);
    if (sz && sz<MinBlock) sz = MinBlock;
    return &BWrap(that->P,x_(that->B.Child,SizeSelect,sz))->B.TlsMemPool;
}
static unsigned int BufferedBucketSize(TlsMemPool *this) {
    TlsGetThat(BBucket,B.TlsMemPool);
    return that->B.Size;
}
static void BufferedClose(TlsMemPool *this) {
    TlsGetThat(BBucket,B.TlsMemPool);
    BPool *P = that->P;
    FBucket *b,*n;
    Flush(P,0,0);
    for (b=P->D.All;b;b=n) { n = b->All; free(b); }
    free(P);
}
static TlsIntCounter *BufferedCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(BBucket,B.TlsMemPool);
    BPool *P = that->P;
    TlsIntCounter *r = P->Count;
    if (Swp) {
        int val = x_(r,Add,0);
        x_(r,Add,-val);
        x_(Swp,Add,val);
        P->Count = Swp;
    }
    return r;
}
static struct TlsMemPool BufferedStatic = {
    BufferedAllocate,BufferedFree,BufferedSizeSelect,BufferedBucketSize,BufferedClose,BufferedCountSwitch
};
 
TlsMemPool *TlsMemPoolBuffered(TlsMemPool *child,unsigned int buffersize) {
    BPool *P = calloc(1,sizeof(*P));
    Lock *l;
    if (!P) Crit("Out of Memory");
    P->Child = child;
    P->Budget = buffersize;
    P->Count = &TlsIntCounterNull;
    if (SharedUnwrap(child,&l)) P->L = l;
    P->Root = BWrap(P,child);
    return &P->Root->B.TlsMemPool;
}
 
/*_____________________________________________________________________________
 |
 | Introspection, for tests.
 |_____________________________________________________________________________
*/
 
unsigned long TlsMemPoolBufferedHeld(TlsMemPool *this) {
    TlsGetThat(BBucket,B.TlsMemPool);
    return (unsigned long)that->P->Held;
}
