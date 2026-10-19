/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
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
 | The Mini pool: a helper pool for small data, on top of a parent pool.
 |
 | Prototype (to go in Tools_Core.h):
 |    TlsMemPool *TlsMemPoolMini(TlsMemPool *Parent,int MaxSize,int tight);
 | Introspection (to go in Tools_Core_Debug.h):
 |    void TlsMemPoolMiniStats(TlsMemPool *Handle,unsigned long *got,unsigned long *put);
 |
 | MaxSize is in 1..4095. A request of sz bytes, 0 < sz <= MaxSize, is served
 | from a page the Mini pool took from its Parent; a request of 0 bytes or of
 | more than MaxSize is the Parent's: the Mini pool only forwards it, and
 | accounts it in its own counter.
 |
 | THE STRUCTURE LIVES IN THE PARENT. The pool itself, its buckets and its
 | pages are all blocks of the Parent: it takes no memory from anywhere else.
 | `tight` tells what Close(root) does:
 |    - tight == 0: the Mini pool is a guest of the Parent. Close gives
 |      everything back to it, and does NOT close it; once Mini is closed the
 |      Parent's counter is back where it was.
 |    - tight != 0: the Mini pool and the Parent are one thing (a pool pushed on
 |      a heap, for one). Close only calls Close on the Parent, which releases
 |      all it handed out, the pages and the structure of the Mini pool among
 |      them: the pages are not given back one by one. Nothing of the Mini pool
 |      may be used afterwards, nor the Parent.
 | Blocks still allocated are the caller's affair, as always.
 | The Mini pool is not thread safe: one per thread, over a Parent that is
 | (a Shared pool, say).
 |
 | SIZE CLASSES. Exact for 1..32, then 16 steps per octave (34, 36 ... 64, 68
 | ... 128, 136 ... up to 4096): a waste of at most 1/16, and nothing for the
 | tiny sizes this pool is for. The last class is MaxSize itself: the sizes
 | between the class before it and MaxSize all get it. A class has no free
 | list in its blocks, so a block may be 1 byte. Buckets are made on the first
 | SizeSelect of their class.
 |
 | PAGES. A bucket has its own page size, a power of 2 (512 bytes at least,
 | enough for 8 blocks and a header), taken from the Parent as a block of that
 | size: the ALIGNMENT rule of Tools_Core.h makes it aligned on itself, so the
 | page of a block is found by masking its address (checked at each page: a
 | parent that does not follow the rule is a critical error). A page is
 |    [blocks, from the base][bitmap of free blocks, 1=free][header, 24 bytes]
 | with the header at the tail, so block i is at i*size from the base. Its
 | alignment is then the lowest bit of the size, at least what the rule asks,
 | and a power of 2 is aligned on itself.
 | The class of a size gives its bucket (an arithmetic function and a table):
 | Free needs no search. A page that is full is on the Full list of its bucket,
 | else on the Partial list; Allocate takes from the head of Partial (the page
 | touched last). Hysteresis: for pages up to 16 KiB the first page that
 | becomes empty is kept as the bucket's spare, only a second one is given back
 | to the Parent: a bucket that allocates and frees around a page boundary does
 | not bounce a page to the Parent each time.
 |
 | COUNTER. CountSwitch accounts the bytes got thru the Mini pool (the size of
 | the buckets, the forwarded ones included), not its pages: the Parent has its
 | own counter, and sees the pages.
 |
 | Misuse that is detected (critical error): a Free of a block that is not of
 | the bucket of its size, one that is already free, a size never selected.
 |_____________________________________________________________________________
 */
 
#define MiniMax 4095                        /* MaxSize at most */
#define NClass 144                          /* 16 exact, then 16 per octave to 4096 */
#define PageMin 512
#define SpareMaxPage 16384                  /* pages up to this keep a spare */
#define DirN 32                             /* forwarders, hashed by the parent handle */
 
static void Crit(char *Msg) {
    x_(&TlsCritErrorShutdown,Signal,Msg);
    abort();
}
 
typedef struct Mini Mini;
typedef struct { TlsMemPool TlsMemPool; Mini *M; } Hd;
 
typedef struct Page Page;
struct Page {                               /* header of a page, at its tail */
    Page *Next,*Prev;
    uint32_t NFree;
    uint32_t Hint;                          /* no free block in the bitmap words below */
};
 
typedef struct Bucket Bucket;
struct Bucket {                             /* a class of small sizes */
    Hd H;
    TlsMemPool *PageH;                      /* the bucket of the parent that gives the pages */
    uint32_t Size;                          /* BucketSize */
    uint32_t PageSz,N,Words;                /* page geometry: blocks per page, bitmap words */
    uint64_t Recip;                         /* ceil(2^32/Size): offset -> block index */
    Page *Partial,*Full,*Spare;
    Bucket *All;
};
 
typedef struct Fwd Fwd;
struct Fwd {                                /* a bucket of the parent, forwarded; also the root */
    Hd H;
    TlsMemPool *Child;
    uint32_t Size;
    uint32_t SzOk;                          /* last size checked to belong to this bucket */
    Fwd *Chain,*All;
};
 
struct Mini {
    Fwd Root;                               /* the 0-size bucket: first, so that Mini is its handle */
    TlsMemPool *Parent;
    unsigned int MaxSize;
    int Tight;                              /* Close closes the parent */
    TlsIntCounter *Count;
    unsigned long PagesGot,PagesPut;
    Bucket *All;
    Fwd *AllF;
    Fwd *Dir[DirN];
    Bucket *Cls[NClass];
};
 
_Static_assert(sizeof(Page)%8==0,"Page header size");
_Static_assert(sizeof(Bucket)%8==0 && sizeof(Fwd)%8==0 && sizeof(Mini)%8==0,"sizes of the structures");
 
static struct TlsMemPool SmallStatic,FwdStatic;
 
static Mini *MiniOf(TlsMemPool *h) { return v_(Hd,TlsMemPool,h)->M; }
 
/* A block of the parent, for the structure of the pool (given back by Close). */
static void *PAlloc(Mini *M,unsigned int sz) {
    TlsMemPool *h = x_(M->Parent,SizeSelect,sz);    /* not nested: x_ evaluates its handle twice */
    return x_(h,Allocate);
}
 
/*_____________________________________________________________________________
 |
 | Classes
 |_____________________________________________________________________________
*/
 
/* The class of sz (1..4095): its index, and the size it serves. */
static unsigned int ClassOf(unsigned int sz,unsigned int *csz) {
    unsigned int k,step,c;
    if (sz<=16) { *csz = sz; return sz-1; }
    k = 31-__builtin_clz(sz-1);                 /* sz in (2^k, 2^(k+1)] */
    step = 1u<<(k-4);
    c = (sz+step-1)&~(step-1);
    *csz = c;
    return 16+(k-4)*16+(c-(1u<<k))/step-1;
}
 
/*_____________________________________________________________________________
 |
 | Pages
 |_____________________________________________________________________________
*/
 
static char *PageBase(Bucket *b,Page *p) { return (char *)p-(b->PageSz-sizeof(Page)); }
static uint64_t *PageMap(Bucket *b,Page *p) { return (uint64_t *)((char *)p-8*(size_t)b->Words); }
 
static void LAdd(Page **h,Page *p) {
    p->Prev = 0; p->Next = *h;
    if (*h) (*h)->Prev = p;
    *h = p;
}
static void LDel(Page **h,Page *p) {
    if (p->Prev) p->Prev->Next = p->Next; else *h = p->Next;
    if (p->Next) p->Next->Prev = p->Prev;
}
 
static Page *PageGet(Bucket *b) {
    Mini *M = b->H.M;
    char *base;
    Page *p;
    uint64_t *m;
    uint32_t i,rem;
    if (b->Spare) { p = b->Spare; b->Spare = 0; return p; }     /* all free, hint 0 */
    base = x_(b->PageH,Allocate);
    if ((uintptr_t)base&(b->PageSz-1)) Crit("MiniMemPool: the parent does not align its blocks as its power of 2 sizes.");
    M->PagesGot++;
    p = (Page *)(base+b->PageSz-sizeof(Page));
    p->Next = p->Prev = 0;
    p->NFree = b->N;
    p->Hint = 0;
    m = PageMap(b,p);
    for (i=0;i<b->Words;i++) m[i] = ~(uint64_t)0;
    rem = b->N&63;
    if (rem) m[b->Words-1] = ((uint64_t)1<<rem)-1;
    return p;
}
static void PagePut(Bucket *b,char *base) {
    b->H.M->PagesPut++;
    x_(b->PageH,Free,base,b->PageSz);
}
 
/*_____________________________________________________________________________
 |
 | Small buckets
 |_____________________________________________________________________________
*/
 
static void *SmallAllocate(TlsMemPool *this) {
    TlsGetThat(Bucket,H.TlsMemPool);
    Page *p = that->Partial;
    uint64_t *m;
    uint32_t w,idx;
    if (!p) { p = PageGet(that); LAdd(&that->Partial,p); }
    m = PageMap(that,p);
    w = p->Hint;
    while (!m[w]) w++;
    p->Hint = w;
    idx = (w<<6)+(uint32_t)__builtin_ctzll(m[w]);
    m[w] &= m[w]-1;
    if (--p->NFree==0) { LDel(&that->Partial,p); LAdd(&that->Full,p); }
    x_(that->H.M->Count,Add,(int)that->Size);
    return PageBase(that,p)+(size_t)idx*that->Size;
}
 
static void SmallFree(Bucket *b,void *mem) {
    char *base = (char *)((uintptr_t)mem&~(uintptr_t)(b->PageSz-1));
    Page *p = (Page *)(base+b->PageSz-sizeof(Page));
    uint64_t *m = PageMap(b,p),bit;
    uint32_t off = (uint32_t)((char *)mem-base),idx,w;
    idx = (uint32_t)(((uint64_t)off*b->Recip)>>32);     /* exact for a block, see Recip */
    if (idx>=b->N || idx*b->Size!=off) Crit("MiniMemPool: Free of a block that is not of the bucket of its size.");
    w = idx>>6; bit = (uint64_t)1<<(idx&63);
    if (m[w]&bit) Crit("MiniMemPool: block already free.");
    m[w] |= bit;
    if (w<p->Hint) p->Hint = w;
    x_(b->H.M->Count,Add,-(int)b->Size);
    if (p->NFree==0) {                                  /* was full */
        LDel(&b->Full,p);
        p->NFree = 1;
        LAdd(&b->Partial,p);
    } else if (p->NFree+1==b->N) {                      /* is empty */
        LDel(&b->Partial,p);
        p->NFree = b->N;
        p->Hint = 0;
        if (!b->Spare && b->PageSz<=SpareMaxPage) b->Spare = p; else PagePut(b,base);
    } else {
        p->NFree++;
    }
}
 
/*_____________________________________________________________________________
 |
 | Forwarded buckets
 |_____________________________________________________________________________
*/
 
static unsigned int DirHash(TlsMemPool *h) {
    uintptr_t v = (uintptr_t)h>>4;
    return (unsigned int)((v^(v>>7)^(v>>14))%DirN);
}
/* The forwarder of a bucket of the parent, made when it is first met. */
static Fwd *FwdOf(Mini *M,TlsMemPool *ph) {
    unsigned int h = DirHash(ph);
    Fwd *f;
    for (f=M->Dir[h];f && f->Child!=ph;f=f->Chain) ;
    if (f) return f;
    f = PAlloc(M,sizeof(*f));
    memset(f,0,sizeof(*f));
    f->H.TlsMemPool.Static = &FwdStatic;
    f->H.M = M;
    f->Child = ph;
    f->Size = x_(ph,BucketSize);
    f->SzOk = f->Size;
    f->Chain = M->Dir[h]; M->Dir[h] = f;
    f->All = M->AllF; M->AllF = f;
    return f;
}
 
static void *FwdAllocate(TlsMemPool *this) {
    TlsGetThat(Fwd,H.TlsMemPool);
    void *r = x_(that->Child,Allocate);
    x_(that->H.M->Count,Add,(int)that->Size);
    return r;
}
 
static void MiniFree(Mini *M,void *mem,unsigned int sz);
static void FwdFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(Fwd,H.TlsMemPool);
    if (sz!=that->Size && sz!=that->SzOk) { MiniFree(that->H.M,mem,sz); return; }
    x_(that->H.M->Count,Add,-(int)that->Size);
    x_(that->Child,Free,mem,sz);
}
 
/* Free from any handle: the class of sz tells the bucket, a forwarded size is
 * looked up in the parent, and the forwarder remembers it. */
static void MiniFree(Mini *M,void *mem,unsigned int sz) {
    if (sz && sz<=M->MaxSize) {
        unsigned int c;
        Bucket *b = M->Cls[ClassOf(sz,&c)];
        if (!b) Crit("MiniMemPool: Free of a size that was never selected.");
        SmallFree(b,mem);
    } else {
        Fwd *f = sz ? FwdOf(M,x_(M->Parent,SizeSelect,sz)) : &M->Root;
        f->SzOk = sz;
        FwdFree(&f->H.TlsMemPool,mem,sz);
    }
}
static void SmallFreeH(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(Bucket,H.TlsMemPool);
    if (sz==that->Size) SmallFree(that,mem); else MiniFree(that->H.M,mem,sz);
}
 
/*_____________________________________________________________________________
 |
 | The pool
 |_____________________________________________________________________________
*/
 
static Bucket *BucketNew(Mini *M,unsigned int size) {
    Bucket *b = PAlloc(M,sizeof(*b));
    uint32_t P = PageMin,N;
    memset(b,0,sizeof(*b));
    b->H.TlsMemPool.Static = &SmallStatic;
    b->H.M = M;
    b->Size = size;
    while (P<8*size+256) P <<= 1;
    N = (P-sizeof(Page))/size;
    while (N*size+8*((N+63)/64)>P-sizeof(Page)) N--;
    b->PageSz = P;
    b->N = N;
    b->Words = (N+63)/64;
    b->Recip = (((uint64_t)1<<32)+size-1)/size;
    b->PageH = x_(M->Parent,SizeSelect,P);
    if (x_(b->PageH,BucketSize)!=P) Crit("MiniMemPool: the parent has no bucket of the size of a page.");
    b->All = M->All; M->All = b;
    return b;
}
 
static TlsMemPool *MiniSizeSelect(TlsMemPool *this,unsigned int sz) {
    Mini *M = MiniOf(this);
    unsigned int c,i;
    Bucket *b;
    if (sz==0) return &M->Root.H.TlsMemPool;
    if (sz>M->MaxSize) return &FwdOf(M,x_(M->Parent,SizeSelect,sz))->H.TlsMemPool;
    i = ClassOf(sz,&c);
    b = M->Cls[i];
    if (!b) b = M->Cls[i] = BucketNew(M,c<M->MaxSize ? c : M->MaxSize);
    return &b->H.TlsMemPool;
}
static unsigned int SmallBucketSize(TlsMemPool *this) {
    TlsGetThat(Bucket,H.TlsMemPool);
    return that->Size;
}
static unsigned int FwdBucketSize(TlsMemPool *this) {
    TlsGetThat(Fwd,H.TlsMemPool);
    return that->Size;
}
 
static void ReleaseList(Bucket *b,Page *p) {
    while (p) {
        Page *n = p->Next;
        PagePut(b,PageBase(b,p));
        p = n;
    }
}
static void MiniClose(TlsMemPool *this) {
    Mini *M = MiniOf(this);
    TlsMemPool *par = M->Parent;
    Bucket *b,*bn;
    Fwd *f,*fn;
    if (M->Tight) { x_(par,Close); return; }    /* all of it is in the parent: it goes with it */
    for (b=M->All;b;b=bn) {
        bn = b->All;
        ReleaseList(b,b->Partial);
        ReleaseList(b,b->Full);
        if (b->Spare) PagePut(b,PageBase(b,b->Spare));
        x_(par,Free,b,sizeof(*b));
    }
    for (f=M->AllF;f;f=fn) {
        fn = f->All;
        x_(par,Free,f,sizeof(*f));
    }
    x_(par,Free,M,sizeof(*M));
}
static TlsIntCounter *MiniCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    Mini *M = MiniOf(this);
    TlsIntCounter *r = M->Count;
    if (Swp) {
        int val = x_(r,Add,0);
        x_(r,Add,-val);
        x_(Swp,Add,val);
        M->Count = Swp;
    }
    return r;
}
 
static struct TlsMemPool SmallStatic = {
    SmallAllocate,SmallFreeH,MiniSizeSelect,SmallBucketSize,MiniClose,MiniCountSwitch
};
static struct TlsMemPool FwdStatic = {
    FwdAllocate,FwdFree,MiniSizeSelect,FwdBucketSize,MiniClose,MiniCountSwitch
};
 
TlsMemPool *TlsMemPoolMini(TlsMemPool *Parent,int MaxSize,int tight) {
    Mini *M;
    TlsMemPool *h;
    if (MaxSize<1 || MaxSize>MiniMax) Crit("MiniMemPool: MaxSize must be in 1..4095.");
    h = x_(Parent,SizeSelect,sizeof(*M));
    M = x_(h,Allocate);
    memset(M,0,sizeof(*M));
    M->Parent = Parent;
    M->Tight = tight;
    M->MaxSize = (unsigned int)MaxSize;
    M->Count = &TlsIntCounterNull;
    M->Root.H.TlsMemPool.Static = &FwdStatic;
    M->Root.H.M = M;
    M->Root.Child = x_(Parent,SizeSelect,0);
    return &M->Root.H.TlsMemPool;
}
 
/*_____________________________________________________________________________
 |
 | Introspection, for tests: pages taken from the parent, and given back.
 |_____________________________________________________________________________
*/
 
void TlsMemPoolMiniStats(TlsMemPool *this,unsigned long *got,unsigned long *put) {
    Mini *M = MiniOf(this);
    *got = M->PagesGot;
    *put = M->PagesPut;
}
 
