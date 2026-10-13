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
#include <Tools_Core.h>

/*_____________________________________________________________________________
 |
 | MemPools
 |_____________________________________________________________________________
*/

struct TlsBNod {uint32_t d[4];};

typedef union {
   TlsDList Free;
   struct {
        TlsBNod Nod;
        void *Mem;
        unsigned int sz;
    } Data;
} XtrnMemChunk;
typedef struct {
    TlsIntCounter *MemWatch;
    struct {
        void *Mem;
        unsigned int sz;
        TlsDList Nods;
        int TopOffset;
    } Intrn;
    struct {
        int Lg2Algn;
        TlsBTreeRoot Nods;
        TlsDList Free;
    } Xtrn;
} MemFrame;

static void MemFrameIntrnGrow(MemFrame *fr) {
    void *b,*p;
    TlsDList *Nod;
    unsigned int sz = fr->Intrn.sz;
    x_(fr->MemWatch,Add,sz);
    p = b = malloc(sz);
    Nod = p; p = Nod+1;
    Nod->n = &fr->Intrn.Nods;
    Nod->p = Nod->n->p; Nod->n->p = Nod->p->n = Nod;
    fr->Intrn.TopOffset = p-b;
}
static XtrnMemChunk *MemFrameNewNod(MemFrame *fr) {
    XtrnMemChunk *r;
    void *b,*p,*e;
    b = fr->Intrn.Nods.p;
    e = b+fr->Intrn.sz;
    r = b+fr->Intrn.TopOffset;
    p = r+1;
    if (p-e>0) {
        MemFrameIntrnGrow(fr);
        r = MemFrameNewNod(fr);
    }
    return r;
}
static XtrnMemChunk *MemFrameGetNod(MemFrame *fr) {
    XtrnMemChunk *r;
    TlsDList *rt = &fr->Xtrn.Free;
    TlsDList *p = rt->n;
    if (p==rt) {
        r = MemFrameNewNod(fr);
    } else {
        rt->n = p->n; rt->n->p = rt;
        r = v_(XtrnMemChunk,Free,p);
    }
    return r;
}
static XtrnMemChunk *MemFrameAllocate(MemFrame *fr,unsigned int sz,unsigned int Algn) {
    XtrnMemChunk *r;
    {int Algn1 = 1<<fr->Xtrn.Lg2Algn; if (Algn<Algn1) {Algn=Algn1;}}
    size_t offs;
    TlsBNod *nod,*in;
    sz = (sz+Algn-1)&(-Algn);
    r = MemFrameGetNod(fr);
    r->Data.sz = sz;
    x_(fr->MemWatch,Add,sz);
    r->Data.Mem = aligned_alloc(Algn,sz); /* malloc(sz); */
    nod = &r->Data.Nod;
    offs = (r->Data.Mem-((void*)0))>>fr->Xtrn.Lg2Algn;
    in = x_(&fr->Xtrn.Nods.TlsBTree,Insert,nod,offs);
    if (in!=nod) { x_(&TlsCritErrorShutdown,Signal,"Memory allocated twice.");}
    return r;
}
static void MemFrameFree(MemFrame *fr,void *Data) {
    TlsBNod *F;
    size_t offs;
    unsigned int v;
    v = offs = (Data-((void *)0))>>fr->Xtrn.Lg2Algn;
    F = TlsBNodGetHighest(&fr->Xtrn.Nods.TlsBTree,&v);
    if (F!=TlsBNodEnd) {
        XtrnMemChunk *f=v_(XtrnMemChunk,Data.Nod,F);
        if (Data==f->Data.Mem) {
            x_(&fr->Xtrn.Nods.TlsBTree,Remove,v);
            x_(fr->MemWatch,Add,-f->Data.sz);
            free(f->Data.Mem);
            TlsDList *rt= &fr->Xtrn.Free;
            TlsDList *fp = &f->Free;
            fp->n=rt; fp->p=rt->p; fp->p->n=rt->n=fp;
        } else {
            x_(&TlsCritErrorShutdown,Signal,"Trying to free unused memory");
        }
    }
}
static XtrnMemChunk *MemFrameLocate(MemFrame *fr,void *Data) {
    XtrnMemChunk *r = 0;
    TlsBNod *F;
    size_t offs;
    unsigned int v;
    v = offs = (Data-((void *)0))>>fr->Xtrn.Lg2Algn;
    F = TlsBNodGetHighest(&fr->Xtrn.Nods.TlsBTree,&v);
    if (F!=TlsBNodEnd) {r=v_(XtrnMemChunk,Data.Nod,F);}
    return r;
}

typedef struct {
    TlsBNodProcess TlsBNodProcess;
    MemFrame *fr;
} rXtrnCloseClos;
static int rXtrnCloseFound(TlsBNodProcess *this,TlsBNod *N,unsigned int off) {
    TlsGetThat(rXtrnCloseClos,TlsBNodProcess);
    XtrnMemChunk *n=v_(XtrnMemChunk,Data.Nod,N);
    x_(that->fr->MemWatch,Add,-n->Data.sz);
    free(n->Data.Mem);
    /* We could add this to the list of free nods, but it will be deleted anyway.*/
    return (0!=0);
}
static void MemFrameClose(MemFrame *that) {
    TlsDList *p,*e;
    int freed = 1;
    struct TlsBNodProcess Static = {rXtrnCloseFound};
    rXtrnCloseClos clos = {{&Static},that};
    x_(&that->Xtrn.Nods.TlsBTree,Clear,&clos.TlsBNodProcess);
    e = &that->Intrn.Nods; p = e->n;
    while (p!=e) {
        TlsDList *f;
        freed++;
        f = p; p = p->n; p->p = f->p; p->p->n = p;
        free(f);
    }
    x_(that->MemWatch,Add,-(freed*that->Intrn.sz));
    free(that->Intrn.Mem);
}

     /*------------------*/

typedef struct {
    TlsMemPool TlsMemPool;
    TlsBTreeRoot Select;
} PoolRoot;
typedef struct {
    TlsMemPool TlsMemPool;
    TlsBNod SzSelect;
} BucketPool;

typedef struct {
    BucketPool Bucket;
    TlsMemPool *Main;
    int Sz;
} SubPool;
static TlsMemPool *SubPoolSelect(TlsMemPool *this,unsigned int sz)  {
    TlsMemPool *r=this;
    TlsGetThat(SubPool,Bucket.TlsMemPool);
    if (sz!=that->Sz) { r=x_(that->Main,SizeSelect,sz);}
    return r;
}
static unsigned int SubChunkSize(TlsMemPool *this) {
    TlsGetThat(SubPool,Bucket.TlsMemPool);
    return that->Sz;
}
static void SubChunkClose(TlsMemPool *this) {
    TlsGetThat(SubPool,Bucket.TlsMemPool);
    x_(that->Main,Close);
}
static TlsIntCounter *SubCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(SubPool,Bucket.TlsMemPool);
    return x_(that->Main,CountSwitch,Swp);
}
     /*------------------*/

typedef struct {
    uint32_t u,d;
    struct {
        unsigned int u:8;
        unsigned int d:8;
        unsigned int Map:16; /* 1 is free  */
    }H;
} SmllHeader;
typedef struct {
    SmllHeader *Pckd;
    SmllHeader *u,*d;
} SmllUnpckedHdr;
#define SmllHeadrUnpckList(_dst,_p) {\
    size_t off=((_dst).Pckd->_p+((size_t)((_dst).Pckd->H._p))<<32)<<8;\
    (_dst)._p = ((void *)0)+off;\
}
#define SmllHeadrPckList(_dst,_p) {\
    size_t off=((void *)((_dst)._p)-((void *)0))>>8;\
    (_dst).Pckd->_p = off; (_dst).Pckd->H._p=off>>32;\
}
#define SmllHeadrPck(_dst) \
    SmllHeadrPckList(_dst,u)\
    SmllHeadrPckList(_dst,d)

#define SmllHeadrUnpck(_dst) \
    SmllHeadrUnpckList(_dst,u)\
    SmllHeadrUnpckList(_dst,d)

typedef struct SmllPool {
    SubPool Sub;
    struct {
        TlsMemPool *Pool;
        struct {
            unsigned int Align:8;
            unsigned int DataOff:8;
            unsigned int DataMax:8;
            unsigned int foo:8;
        } Capacity;
    } Buffer;
    struct {
        SmllHeader *Base;
        SmllHeader *Full;
    } Datas;
} SmllPool;

static void SmllBufferUnchain(SmllHeader **Rt,SmllHeader *Buff) {
    SmllUnpckedHdr u,d,buff;
    buff.Pckd = Buff;
    SmllHeadrUnpckList(buff,u)
    SmllHeadrUnpckList(buff,d)
    u.Pckd = buff.u; d.Pckd=buff.d;
    d.u = buff.u; u.d=buff.d;
    SmllHeadrPckList(u,d);
    SmllHeadrPckList(d,u);
    if (*Rt==buff.Pckd) {
        *Rt=buff.u;
        if (*Rt==buff.Pckd) { *Rt=0;}
    }
}
static void SmllBufferChainBottom(SmllHeader **Rt,SmllHeader *Buff) {
    SmllUnpckedHdr buff;
    buff.Pckd = Buff;
    if (*Rt==0) {
        buff.u = buff.d = buff.Pckd;
        SmllHeadrPckList(buff,u);
        SmllHeadrPckList(buff,d);
        *Rt = buff.Pckd;
    } else {
        SmllUnpckedHdr u,d;
        u.Pckd = *Rt;
        SmllHeadrUnpckList(u,d);
        d.Pckd = u.d;
        SmllHeadrUnpckList(d,u);
        buff.d = d.Pckd; buff.u = u.Pckd;
        u.d = d.u = buff.Pckd;
        SmllHeadrPckList(u,d);
        SmllHeadrPckList(d,u);
        *Rt = buff.Pckd;
    }
}

static SmllHeader *smllNewBuffer(SmllPool *that) {
    void *p,*buff;
    unsigned int dmax = that->Buffer.Capacity.DataMax;
    SmllHeader *H;
    SmllUnpckedHdr h;
    p = buff = x_(that->Buffer.Pool,Allocate);
    H = buff; p = H+1;
    h.Pckd=h.u=h.d=H;
    SmllHeadrPck(h);
    that->Datas.Base = H;
    if (dmax<=16) {
        H->H.Map = (1<<dmax)-1;
    } else {
        uint32_t *mp = p;
        unsigned int i = dmax;
        H->H.Map = (1<<(((dmax+31)>>5)<<1))-1;
        while (i>32) { *mp++ = (uint32_t)(-1); i-=32;}
        *mp = (1<<i)-1;
    }
    return H;
}

static void *SmllAllocate(TlsMemPool *this) {
    TlsGetThat(SmllPool,Sub.Bucket.TlsMemPool);
    void *r=0;
    SmllHeader *h;
    int full;
    unsigned int dmax = that->Buffer.Capacity.DataMax;
    if (that->Datas.Base==0) {
        that->Datas.Base=smllNewBuffer(that);
    }
    h = that->Datas.Base;
    if (dmax<=16) {
        void *p=h+1;
        int off = TlsLog2(h->H.Map);
        r = p+(off*that->Sub.Sz);
        h->H.Map ^= 1<<off;
        full = (h->H.Map==0);
    } else {
        void *p=h+1;
        uint32_t *mp=p; p = p+((dmax+31)>>5);
        int mpi = TlsLog2(h->H.Map&0xaaaa)>>1;
        unsigned int off = TlsLog2(mp[mpi]);
        r = p+(((mpi<<5)+off)*that->Sub.Sz);
        mp[mpi] ^= 1<<off;
        h->H.Map &= ~(1<<(mpi<<1));
        if (!mp[mpi]) { h->H.Map &= ~(2<<(mpi<<1));}
        full = (h->H.Map==0);
    }
    if (full) {
        SmllBufferUnchain(&that->Datas.Base,h);
        SmllBufferChainBottom(&that->Datas.Full,h);
    }
    return r;
}
static void SmllFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(SmllPool,Sub.Bucket.TlsMemPool);
    if (sz==that->Sub.Sz) {
        void *chnk = ((void *)0)+((mem-(void *)0)&(-((size_t)(that->Buffer.Capacity.Align))));
        SmllUnpckedHdr hd;
        int Empty;
        int dmax= that->Buffer.Capacity.DataMax;
        hd.Pckd = chnk;
        int Full = (hd.Pckd->H.Map==0);
        if (dmax<16) {
            void *data = hd.Pckd+1;
            unsigned int Off = (mem-data)/sz;
            hd.Pckd->H.Map |= (1<<Off);
            Empty = (hd.Pckd->H.Map==((1<<dmax)-1));
        } else {
            void *p=hd.Pckd+1;
            uint32_t *Map=p;
            void *data = p = Map+(((dmax+31)>>5)<<2);
            unsigned int Off = (mem-data)/sz;
            uint32_t *pMap = Map+(Off>>5);
            *pMap |= (1<<(Off&31));
            Empty = (*pMap==((uint32_t)-1))||((Off>=(dmax&0xe0))&&(*pMap==((1<<(Off&0x1f))-1)));
            if (Empty) {
                hd.Pckd->H.Map |= 3<<((Off>>5)<<1);
                Empty= (hd.Pckd->H.Map==((1<<((dmax+31)>>5)<<1)-1));
            }
        }
        if (Full) {
            SmllBufferUnchain(&that->Datas.Full,hd.Pckd);
            SmllBufferChainBottom(&that->Datas.Base,hd.Pckd);
        }
        if (Empty) {
            SmllBufferUnchain(&that->Datas.Base,hd.Pckd);
            x_(that->Buffer.Pool,Free,chnk,1<<that->Buffer.Capacity.Align);
        }
    } else {
        x_(that->Sub.Main,Free,mem,sz);
    }
}

static TlsMemPool *SmllPoolInit(SmllPool *r,TlsMemPool *main,int sz,TlsMemPool *BuckPool,int BuckAlign) {
    static struct TlsMemPool Static = {
        SmllAllocate,SmllFree,SubPoolSelect,SubChunkSize,SubChunkClose,SubCountSwitch
    };
    r->Sub.Bucket.TlsMemPool.Static = &Static;
    r->Sub.Sz = sz;
    r->Sub.Main = main;
    r->Buffer.Pool = BuckPool;
    int BckSz = x_(BuckPool,BucketSize);
    r->Buffer.Capacity.Align = BuckAlign;
    { void *b,*p,*e;
      int dispSz,dmax;
      b = p = 0; e = p+BckSz;
      SmllHeader *h = p; p = h+1; dispSz = e-p;
      r->Buffer.Capacity.DataOff = p-b;
      r->Buffer.Capacity.DataMax = dmax = dispSz/sz;
      if (dmax>16) {
          int n = (dmax+31)>>5;
          uint32_t *np = p; p = np+n;
          r->Buffer.Capacity.DataOff = (p-b);
          r->Buffer.Capacity.DataMax = (e-p)/sz;
      }
    }
    r->Datas.Base = 0;
    r->Datas.Full = 0;
    return &r->Sub.Bucket.TlsMemPool;
}

/*_____________________________________________________________________________
 |
 | Align pool: We're dealing with power of 2 sized chunked here.
 | memchunk of size above 1Mo are directly allocated using 
 | MemFrames. Bellow that value, memchunks are subchunks of bigger chunks.
 | For each size of chunk, there is a fixed size directory chunk that will record
 | the occupation state of its nods.
 | That directory is kept at the bottom of the parent bChunks (parent chunk of size
 | 256^n*4ko) and at the top for tChunks (chunks of size 256^n*256).
 | As long as the directory nod is present, it is possible to have incomplete
 | chunk; so if a chunk is not perfectly align, it is possible to subdivide
 | it and put tChunks at its bottom and bChunks at its top.
 | Directory nodes can be Primary or Sub directories. Sub directories are data
 | nodes that have a directory structure; when empty they will be freed in
 | their parent nodes. Primary directories stay in place, when empty, unless
 | the root directory is empty. The root directory nod is somewhere in the
 | chunk and each 1Mo directory nod has a pointer to it. It contains the start
 | point and size of the chunk and the occupation state of the primary 
 | directories (empty or partially filled).
 | 
 | Directory chunk size are of size:
 |     4Mo : 256ko, 512ko; Header Top 4o+8o+4o
 |     1Mo : 16ko, 32ko, 64ko, 128ko; Header Bottom 8o+8o, Root Nod Ptr
 |    64ko : 1ko, 2ko, 4ko, 8ko, Header Top 4+8o
 |     4ko : 64o, 128o, 256o, 512o, Header Bottom 8o
 |    256o : 16o, 32o, Header Top 4o 
 | Regardless their header must have an offset to top chunks header.
 | The top chunk header will keep track of the primary chunks state.
 |_____________________________________________________________________________
*/

typedef struct {
    unsigned int PureDir:1;
    unsigned int Free:31;
} AlignHeader;
typedef struct {
    TlsDList p;
} AlignFreeNod;
struct SubPoolPck {
    unsigned int Sz:5;
    unsigned int CntnSz:5;
    unsigned int AtomSz:5;
    unsigned int foo:17;
};
typedef struct {
    SubPool Sub;
    MemFrame *MemFrame;
    struct AlignBucket {
        BucketPool Pool;
        struct SubPoolPck Sz;
        TlsDList/*AlignFreeNod*/ *Free,*PartFree;
    } MainBuck[21]; /*  32o -> 4Mo */
} AlignPool;


static void *AlignItemLock(AlignPool *that,int pSz,int cSz,TlsDList *P) {
    AlignHeader *hd;
    void *Item = P;
    unsigned int pMsk;
    pMsk = 1<<pSz;
    hd = (AlignHeader *)((((void *)Item)-((void *)0))&(-pMsk));
    if (cSz&1) { hd = ((void *)hd)+(pMsk-0x100);}
    return P;
}
static void *AlignItemDivide(AlignPool *that,struct SubPoolPck dst,int srcSz,void *chnk) {
}
static void *AlignPartition(AlignPool *that,struct SubPoolPck fmt,void *chnk) {
}
static void AlignGrow(AlignPool *that) {
}
static void *AlignItemAllocate(AlignPool *that,unsigned int cSz) {
    void *r;
    struct AlignBucket *b,*e,*p;
    struct SubPoolPck pckd;
    p = b = that->MainBuck+(cSz-8);
    pckd = b->Sz;
    e = that->MainBuck+(pckd.AtomSz+pckd.Sz)-8;
    while ((p->Free==0)&&(p<e)) { p++; }
    if (p->Free==0) {
        void *ck;
        TlsMemPool *MemPool= &that->MainBuck[pckd.CntnSz-8].Pool.TlsMemPool;
        ck = x_(MemPool,Allocate);
        r = AlignPartition(that,pckd,ck);
    } else {
        TlsDList *P;
        P = p->Free;
        P->p->n = P->n; P->n->p=P->p;
        p->Free = P->n;
        if (p->Free==P) { p->Free = 0; }
        if (p==b) {
            r = AlignItemLock(that,pckd.CntnSz,pckd.Sz,P);
        } else {
            r = AlignItemDivide(that,pckd,p->Sz.Sz,P);
        }
    }
    return r;
}
static void *AlignBucketAllocate(TlsMemPool *this) {
    void *r;
    TlsGetThat(struct AlignBucket,Pool.TlsMemPool);
    struct SubPoolPck Sz = that->Sz;
    int cSz = Sz.Sz;
    AlignPool *That=v_(AlignPool,MainBuck[cSz-8],that);
        TlsDList *P;
        int pSz = Sz.CntnSz;
        int parentPool = pSz-8;
        P = that->Free;
        if (!P) {
            r = AlignItemAllocate(That,cSz);
        } else {
            if (P->n==P) { 
                that->Free = 0; 
            } else {
                that->Free = P->n;
                P->n->p = P->p; P->p->n = P->n;
            }
            AlignItemLock(That,pSz,cSz,P);
            r = P;
        }
    return r;
}
static void AlignBucketFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(struct AlignBucket,Pool.TlsMemPool);
    
}
static TlsMemPool *AlignBucketSizeSelect(TlsMemPool *this,unsigned sz) {
    TlsGetThat(struct AlignBucket,Pool.TlsMemPool);
    struct SubPoolPck Pckd = that->Sz;
    if (sz!=(1<<Pckd.Sz)) {
        AlignPool *That=v_(AlignPool,MainBuck[Pckd.Sz-8],that);
        this = x_(That->Sub.Main,SizeSelect,sz);
    }
    return this;
}
static int AlignBucketSize(TlsMemPool *this) {
    TlsGetThat(struct AlignBucket,Pool.TlsMemPool);
    return 1<<that->Sz.Sz;
}

     /*------------------*/

typedef struct {
    TlsMemPool TlsMemPool;
    MemFrame MemFrame;
    TlsIntCounter *Watch;
} MainMem;

static void *MainMemAllocate(TlsMemPool *this) {}
static void MainMemFree(TlsMemPool *this,void *mem,unsigned int sz) {}
static TlsMemPool *MainMemSizeSelect(TlsMemPool *this,unsigned int sz) {}
static unsigned int MainMemChunkSize(TlsMemPool *this) {}
static void MainMemClose(TlsMemPool *this) {}
static TlsIntCounter *MainMemCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {}

