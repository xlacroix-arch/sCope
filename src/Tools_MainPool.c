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
#include <stdatomic.h>
#include <Tools_Core.h>
#include <Tools_Chunk.h>
#include <Tools_Core_Debug.h>

/*_____________________________________________________________________________
 |
 | The main pool: a TlsMemPool that serves every size, through a table of
 | specialised buckets created on demand.
 |
 | Prototypes (to go in Tools_Core.h):
 |    TlsMemPool *TlsMainPoolOpen(TlsIntCounter *Count);
 |    TlsMemPool *TlsMainPoolOpenWith(TlsIntCounter *Count,TlsChunkSource *Src);
 |    unsigned int TlsMainPoolClasses(void);               number of classes
 |    unsigned int TlsMainPoolClassOf(unsigned int sz);    sz in 1..4 MiB
 |    unsigned int TlsMainPoolClassSize(unsigned int c);
 |
 | The handle of the root is the 0-size bucket.
 |
 | SIZE CLASSES. The table of classes is built once, from the chunk geometry
 | (see ClassTableBuild); SizeSelect is a binary search in it. Up to 4 MiB:
 |    - tiny: every size 1..64 is a class;
 |    - powers of 2: every one is a class, aligned on its own size;
 |    - harmonic: for each chunk size C=2^k (1 KiB to 4 MiB) and each count N
 |      of objects in [Nmin(k), 2*Nmin(k-1)), the class floor((C-head)/N)
 |      rounded down to a multiple of its alignment a (the largest power of 2
 |      not above size/64). Chunks are filled to the byte, the rounding costs
 |      under 1/64, and the class sizes follow the chunk sizes. Nmin goes down
 |      with the size (12 for chunks up to 256 KiB, 8 from 2 MiB): big objects
 |      cannot be 12 per chunk;
 |    - large: above the biggest harmonic class, 16 steps per octave.
 | Above 4 MiB the bucket size is the request rounded to 64 KiB.
 |
 | ALIGNMENT. In a slab chunk the objects start at the base of the chunk
 | (the header is at its tail), so object i is at i*size from an address
 | aligned on the chunk size: blocks are aligned on the lowest bit of their
 | size. So a power of 2 is aligned on itself, a harmonic class on its
 | alignment a (4 from 256, 8 from 512, 16 from 1024, ...). Large buckets give
 | the page, or the size for a power of 2. TlsMainPoolAlign tells it.
 |
 | BUCKETS. Two kinds, chosen from the class:
 |   - Slab buckets serve blocks from chunks aligned on their size (a power
 |     of 2). A chunk is [objects][padding][bitmap of free objects, 1=free]
 |     [Slab header, 32 bytes]. The header is found from a block by masking
 |     its address with the chunk size, which Free gets thru the class of sz.
 |     A power of 2 class has 31 or 15 objects: the header takes the last slot.
 |   - Large buckets: one aligned block per allocation (0, big classes, >4 MiB).
 |
 | OCCUPANCY. The non-full chunks of a bucket are binned by log2 of their
 | free count; Allocate takes from the lowest non empty bin, i.e. from the
 | fullest chunk, so that the others can drain and be released. Hysteresis:
 | in the small tiers (chunks up to SpareMaxChunk), the first chunk that
 | becomes empty is kept as a Spare (no release); only a second empty one is
 | given back. A small bucket that allocates and frees around a chunk boundary
 | does not thrash. Big chunks are given back at once: a spare there would pin
 | too much memory per class (see Tools_MainPool.Policies.md).
 | The bins and the Full list are TlsDList heads (circular sentinels); a Slab
 | is on one of them thru its List component, and on none when it is the Spare.
 |
 | Chunks, and the blocks of the large buckets, come from a TlsChunkSource
 | (Tools_Chunk.h): the arena of Tools_ChunkArena.c by default, which places
 | them low and gives free memory back to the system. TlsMainPoolOpenWith takes
 | another one (the libc source, for example); the pool closes its source when
 | it is closed.
 |_____________________________________________________________________________
*/

#define SmallMax (1u<<22)                   /* last size served thru the class table */
#define MaxClasses 400
#define BigStep 0x10000u                    /* granularity above SmallMax */
#define SzMax 0x7FFF0000u                   /* the counters are int */
#define SlabBins 20                         /* log2 of free counts, N < 2^20 */
#define SlabHdr 32
#define TinyMax 64                          /* sizes 1..TinyMax: a class each */
#define TinyChunk 4096
#define KMin 10                             /* smallest harmonic chunk: 1 KiB */
#define KMax 22                             /* biggest: 4 MiB */
#define PowSlabMax (1u<<16)                 /* powers of 2 up to this are slabs */
#define PowChunkMax (1u<<20)                /* and their chunks are at most this */
#define PowObjs 32                          /* objects per chunk of a power of 2 (16 if too big) */
#define SpareMaxChunk 16384                 /* chunks up to this size keep a spare */

/* Nmin by chunk size 2^k, k=KMin..KMax: objects per chunk, at least. */
static const unsigned char NminOf[KMax-KMin+1] = {12,12,12,12,12,12,12,12,12,11,10,8,8};

static void Crit(char *Msg) {
    x_(&TlsCritErrorShutdown,Signal,Msg);
    abort();
}

/*_____________________________________________________________________________
 |
 | Size classes
 |_____________________________________________________________________________
*/

typedef struct {
    uint32_t Size;
    uint32_t Chunk;             /* 0: large bucket */
    uint32_t N;                 /* objects per chunk */
    uint32_t Align;
} Class;
static Class Tab[MaxClasses];
static unsigned int NTab;
static atomic_int TabState;     /* 0 not built, 1 building, 2 built */

static uint32_t LowBit(uint32_t v) { return v&(-v); }
static uint32_t MapWords(uint32_t N) { return (N+31)/32; }
static uint32_t SlabHead(uint32_t N) { return SlabHdr+4*MapWords(N); }
/* Objects that fit a chunk of C bytes: bitmap + header + N objects <= C. */
static uint32_t SlabFit(uint32_t C,uint32_t s) {
    uint32_t N = (C-SlabHdr)/s;
    while (N && SlabHead(N)+N*s>C) N--;
    return N;
}
static void ClassAdd(Class *t,unsigned int *n,uint32_t s,uint32_t C,uint32_t N,uint32_t A) {
    if (*n>=MaxClasses) Crit("Too many size classes.");
    if (C && (N==0 || SlabHead(N)+(uint64_t)N*s>C)) Crit("Size class does not fit its chunk.");
    t[*n].Size = s; t[*n].Chunk = C; t[*n].N = N; t[*n].Align = A;
    (*n)++;
}
static void ClassTableBuild(void) {
    Class *t = Tab;
    unsigned int n = 0,i,j,k,top = 0;
    uint32_t s;
    for (s=1;s<=TinyMax;s++) ClassAdd(t,&n,s,TinyChunk,SlabFit(TinyChunk,s),LowBit(s));
    for (s=TinyMax*2;s<=SmallMax;s<<=1) {
        if (s<=PowSlabMax) {
            uint32_t C = PowObjs*s;
            if (C>PowChunkMax) C = (PowObjs/2)*s;
            ClassAdd(t,&n,s,C,SlabFit(C,s),s);
        } else ClassAdd(t,&n,s,0,0,s);
    }
    for (k=KMin;k<=KMax;k++) {
        uint32_t C = 1u<<k,Nlo = NminOf[k-KMin];
        uint32_t Nhi = 2*(k==KMin?Nlo:NminOf[k-1-KMin])-1,N;
        for (N=Nhi;N>=Nlo;N--) {
            uint32_t x = (C-SlabHead(N))/N,a = 1;
            while (a*128<=x) a *= 2;
            s = x-(x%a);
            if (s<=TinyMax) continue;
            ClassAdd(t,&n,s,C,N,LowBit(s));
            if (s>top) top = s;
        }
    }
    for (k=16;k<22;k++) {                       /* large grid: 16 steps per octave */
        for (j=1;j<=16;j++) {
            s = (1u<<k)+j*(1u<<(k-4));
            if (s>top && s<=SmallMax) ClassAdd(t,&n,s,0,0,(s&(s-1))?4096:s);
        }
    }
    for (i=1;i<n;i++) {                         /* stable insertion sort by size */
        Class c = t[i];
        for (j=i;j>0 && t[j-1].Size>c.Size;j--) t[j] = t[j-1];
        t[j] = c;
    }
    for (i=j=1;i<n;i++) if (t[i].Size!=t[j-1].Size) t[j++] = t[i];    /* first of equal sizes stays */
    NTab = j;
}
static void ClassTableEnsure(void) {
    int st = atomic_load_explicit(&TabState,memory_order_acquire);
    if (st==2) return;
    st = 0;
    if (atomic_compare_exchange_strong(&TabState,&st,1)) {
        ClassTableBuild();
        atomic_store_explicit(&TabState,2,memory_order_release);
    } else {
        while (atomic_load_explicit(&TabState,memory_order_acquire)!=2) ;
    }
}
unsigned int TlsMainPoolClasses(void) { ClassTableEnsure(); return NTab; }
/* The first class that serves sz (1..SmallMax). */
unsigned int TlsMainPoolClassOf(unsigned int sz) {
    unsigned int lo = 0,hi;
    ClassTableEnsure();
    hi = NTab-1;
    while (lo<hi) {
        unsigned int mid = (lo+hi)>>1;
        if (Tab[mid].Size>=sz) hi = mid; else lo = mid+1;
    }
    return lo;
}
unsigned int TlsMainPoolClassSize(unsigned int c) {
    ClassTableEnsure();
    return Tab[c].Size;
}

/*_____________________________________________________________________________
 |
 | Structures
 |_____________________________________________________________________________
*/

typedef struct MainPool MainPool;
typedef struct Slab Slab;
struct Slab {                   /* header of a chunk, at its tail */
    TlsDList of_(Slab.List) List;   /* in a bin or in Full; in none when it is the Spare */
    uint32_t nfree;
    uint32_t hint;              /* no free object in the bitmap words below */
    uint16_t bin;               /* bin it is listed in, when not full/spare */
};
_Static_assert(sizeof(Slab)<=SlabHdr,"Slab header size");

typedef struct Bucket {
    TlsMemPool TlsMemPool;
    MainPool *Pool;
    uint32_t Size;              /* the size served */
    int64_t Lo;                 /* serves Lo < sz <= Size */
    struct Bucket *Next,*BigNext;
    uint32_t Align;             /* alignment of the blocks */
    uint32_t Chunk,N,MapWords;  /* slab geometry, Chunk is 0 if Large */
    uint32_t binMask;
    TlsDList of_(Slab.List) bins[SlabBins];
    TlsDList of_(Slab.List) Full;
    Slab *Spare;
} Bucket;

struct MainPool {
    Bucket Root;                /* the 0-size bucket */
    Bucket *Cls[MaxClasses];
    Bucket *Big;                /* buckets above SmallMax */
    Bucket *All;                /* every bucket but the root, for Close */
    TlsIntCounter *Count;
    TlsChunkSource *Src;
    unsigned long ChunksGot,ChunksPut;
};

/*_____________________________________________________________________________
 |
 | Chunks
 |_____________________________________________________________________________
*/

static void *ChunkGet(MainPool *P,uint32_t C) {
    P->ChunksGot++;
    return x_(P->Src,Get,C,C);
}
static void ChunkPut(MainPool *P,void *c,uint32_t C) {
    P->ChunksPut++;
    x_(P->Src,Put,c,C);
}

/*_____________________________________________________________________________
 |
 | Slab chunk lists and bins
 |_____________________________________________________________________________
*/

static void ListInit(TlsDList *h) { h->n = h->p = h; }
static int ListEmpty(TlsDList *h) { return h->n==h; }
static void ListAdd(TlsDList *h,TlsDList *e) {      /* e goes first after the head h */
    e->p = h; e->n = h->n;
    h->n->p = e; h->n = e;
}
static void ListDel(TlsDList *e) {
    e->p->n = e->n; e->n->p = e->p;
}
static void BinAdd(Bucket *b,Slab *s) {
    unsigned int k = (unsigned int)TlsLog2(s->nfree);
    s->bin = (uint16_t)k;
    ListAdd(&b->bins[k],&s->List);
    b->binMask |= 1u<<k;
}
static void BinDel(Bucket *b,Slab *s) {
    TlsDList *h = &b->bins[s->bin];
    ListDel(&s->List);
    if (ListEmpty(h)) b->binMask &= ~(1u<<s->bin);
}

/* The header is at the tail of the chunk, the bitmap just below it, and the
 * objects start at the base. */
static char *SlabBase(Bucket *b,Slab *s) { return (char *)s-(b->Chunk-SlabHdr); }
static uint32_t *SlabMap(Bucket *b,Slab *s) { return (uint32_t *)((char *)s-4*b->MapWords); }

static Slab *SlabNew(Bucket *b) {
    char *base = ChunkGet(b->Pool,b->Chunk);
    Slab *s = (Slab *)(base+b->Chunk-SlabHdr);
    uint32_t *m = SlabMap(b,s),i,rem = b->N&31;
    s->nfree = b->N;
    s->hint = 0;
    s->bin = 0;
    for (i=0;i<b->MapWords;i++) m[i] = 0xFFFFFFFFu;
    if (rem) m[b->MapWords-1] = (1u<<rem)-1;
    return s;
}

/*_____________________________________________________________________________
 |
 | Buckets
 |_____________________________________________________________________________
*/

static int InRange(Bucket *b,unsigned int sz) { return ((int64_t)sz>b->Lo) && (sz<=b->Size); }

static void *SlabAllocate(TlsMemPool *this) {
    TlsGetThat(Bucket,TlsMemPool);
    Slab *s;
    uint32_t *m,w,idx;
    if (that->binMask) {
        TlsDList *h = &that->bins[__builtin_ctz(that->binMask)];
        s = v_(Slab,List,h->n);                             /* fullest non-full chunk */
    } else {
        s = that->Spare;
        if (s) that->Spare = 0; else s = SlabNew(that);
        BinAdd(that,s);
    }
    m = SlabMap(that,s);
    w = s->hint;
    while (!m[w]) w++;
    s->hint = w;
    idx = (w<<5)+__builtin_ctz(m[w]);
    m[w] &= m[w]-1;
    s->nfree--;
    if (s->nfree==0) {
        BinDel(that,s);
        ListAdd(&that->Full,&s->List);
    } else if ((unsigned int)TlsLog2(s->nfree)!=s->bin) {
        BinDel(that,s);
        BinAdd(that,s);
    }
    x_(that->Pool->Count,Add,(int)that->Size);
    return SlabBase(that,s)+(size_t)idx*that->Size;
}
static void SlabFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(Bucket,TlsMemPool);
    char *base;
    Slab *s;
    uint32_t *m,off,idx,w,bit;
    if (!InRange(that,sz)) {                /* not the bucket of sz: let it do */
        TlsMemPool *h = x_(this,SizeSelect,sz);
        x_(h,Free,mem,sz);
        return;
    }
    base = (char *)((uintptr_t)mem & ~(uintptr_t)(that->Chunk-1));
    s = (Slab *)(base+that->Chunk-SlabHdr);
    off = (uint32_t)((char *)mem-base);
    if (off%that->Size) Crit("Free: not a block of this bucket.");
    idx = off/that->Size;
    if (idx>=that->N) Crit("Free: not a block of this bucket.");
    m = SlabMap(that,s);
    w = idx>>5; bit = 1u<<(idx&31);
    if (m[w]&bit) Crit("Free: block already free.");
    m[w] |= bit;
    if (w<s->hint) s->hint = w;
    x_(that->Pool->Count,Add,-(int)that->Size);
    if (s->nfree==0) {                      /* was full: back in the bins */
        ListDel(&s->List);
        s->nfree = 1;
        BinAdd(that,s);
    } else if (s->nfree+1==that->N) {       /* is empty: keep one spare (small tiers), release the others */
        BinDel(that,s);
        s->nfree = that->N;
        s->hint = 0;
        if (!that->Spare && that->Chunk<=SpareMaxChunk) that->Spare = s; else ChunkPut(that->Pool,base,that->Chunk);
    } else {
        s->nfree++;
        if ((unsigned int)TlsLog2(s->nfree)!=s->bin) {
            BinDel(that,s);
            BinAdd(that,s);
        }
    }
}

static void *LargeAllocate(TlsMemPool *this) {
    TlsGetThat(Bucket,TlsMemPool);
    void *r;
    if (that->Size) r = x_(that->Pool->Src,Get,that->Size,that->Align);   /* a multiple of Align */
    else r = malloc(1);                                                   /* a 0 size still gets a valid pointer */
    if (!r) Crit("Out of Memory");
    x_(that->Pool->Count,Add,(int)that->Size);
    return r;
}
static void LargeFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(Bucket,TlsMemPool);
    if (!InRange(that,sz)) {
        TlsMemPool *h = x_(this,SizeSelect,sz);
        x_(h,Free,mem,sz);
        return;
    }
    x_(that->Pool->Count,Add,-(int)that->Size);
    if (that->Size) x_(that->Pool->Src,Put,mem,that->Size);
    else free(mem);
}

static struct TlsMemPool SlabStatic,LargeStatic;

static Bucket *BucketNew(MainPool *P,uint32_t Size,int64_t Lo,uint32_t Chunk,uint32_t N,uint32_t Align) {
    Bucket *b = calloc(1,sizeof(*b));
    int i;
    if (!b) Crit("Out of Memory");
    b->Pool = P;
    b->Size = Size;
    b->Lo = Lo;
    b->Align = Align;
    b->Chunk = Chunk;
    b->N = N;
    b->MapWords = MapWords(N);
    for (i=0;i<SlabBins;i++) ListInit(&b->bins[i]);
    ListInit(&b->Full);
    b->TlsMemPool.Static = Chunk ? &SlabStatic : &LargeStatic;
    b->Next = P->All;
    P->All = b;
    return b;
}

static TlsMemPool *MainSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(Bucket,TlsMemPool);
    MainPool *P = that->Pool;
    Bucket *b;
    if (sz==0) return &P->Root.TlsMemPool;
    if (sz<=SmallMax) {
        unsigned int c = TlsMainPoolClassOf(sz);
        b = P->Cls[c];
        if (!b) {
            b = BucketNew(P,Tab[c].Size,c?Tab[c-1].Size:0,Tab[c].Chunk,Tab[c].N,Tab[c].Align);
            P->Cls[c] = b;
        }
    } else {
        uint32_t Size;
        if (sz>SzMax) Crit("Allocation too large.");
        Size = (sz+(BigStep-1))&~(BigStep-1);
        for (b=P->Big;b && b->Size!=Size;b=b->BigNext) ;
        if (!b) {
            b = BucketNew(P,Size,(int64_t)Size-BigStep,0,0,4096);
            b->BigNext = P->Big;
            P->Big = b;
        }
    }
    return &b->TlsMemPool;
}
static unsigned int MainBucketSize(TlsMemPool *this) {
    TlsGetThat(Bucket,TlsMemPool);
    return that->Size;
}
/* Gives back the chunks of the slabs on the list h. */
static void ReleaseList(Bucket *b,TlsDList *h) {
    TlsDList *e = h->n;
    while (e!=h) {
        TlsDList *n = e->n;
        ChunkPut(b->Pool,SlabBase(b,v_(Slab,List,e)),b->Chunk);
        e = n;
    }
}
static void MainClose(TlsMemPool *this) {
    TlsGetThat(Bucket,TlsMemPool);
    MainPool *P = that->Pool;
    Bucket *b,*n;
    for (b=P->All;b;b=n) {
        int i;
        n = b->Next;
        for (i=0;i<SlabBins;i++) ReleaseList(b,&b->bins[i]);
        ReleaseList(b,&b->Full);
        if (b->Spare) ChunkPut(P,SlabBase(b,b->Spare),b->Chunk);
        free(b);
    }
    x_(P->Src,Close);
    free(P);
}
static TlsIntCounter *MainCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(Bucket,TlsMemPool);
    MainPool *P = that->Pool;
    TlsIntCounter *r = P->Count;
    if (Swp) {
        int val = x_(r,Add,0);
        x_(r,Add,-val);
        x_(Swp,Add,val);
        P->Count = Swp;
    }
    return r;
}

static struct TlsMemPool SlabStatic = {
    SlabAllocate,SlabFree,MainSizeSelect,MainBucketSize,MainClose,MainCountSwitch
};
static struct TlsMemPool LargeStatic = {
    LargeAllocate,LargeFree,MainSizeSelect,MainBucketSize,MainClose,MainCountSwitch
};

/* Count is where the allocated bytes are accounted (NULL: nowhere). */
TlsMemPool *TlsMainPoolOpenWith(TlsIntCounter *Count,TlsChunkSource *Src) {
    MainPool *P = calloc(1,sizeof(*P));
    if (!P) Crit("Out of Memory");
    ClassTableEnsure();
    P->Src = Src;
    P->Count = Count ? Count : &TlsIntCounterNull;
    P->Root.TlsMemPool.Static = &LargeStatic;
    P->Root.Pool = P;
    P->Root.Size = 0;
    P->Root.Lo = -1;
    P->Root.Align = 1;
    return &P->Root.TlsMemPool;
}
TlsMemPool *TlsMainPoolOpen(TlsIntCounter *Count) {
    return TlsMainPoolOpenWith(Count,TlsChunkSourceArena());
}

/*_____________________________________________________________________________
 |
 | Introspection, for tests.
 |_____________________________________________________________________________
*/

void TlsMainPoolChunkStats(TlsMemPool *this,unsigned long *got,unsigned long *put) {
    TlsGetThat(Bucket,TlsMemPool);
    *got = that->Pool->ChunksGot;
    *put = that->Pool->ChunksPut;
}
/* 1 and the chunk size and object count if the bucket is a slab, else 0. */
int TlsMainPoolSlabInfo(TlsMemPool *this,unsigned int *Chunk,unsigned int *N) {
    TlsGetThat(Bucket,TlsMemPool);
    if (!that->Chunk) return 0;
    *Chunk = that->Chunk;
    *N = that->N;
    return 1;
}
unsigned int TlsMainPoolAlign(TlsMemPool *this) {
    TlsGetThat(Bucket,TlsMemPool);
    return that->Align;
}
