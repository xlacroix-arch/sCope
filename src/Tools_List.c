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
#include <stdint.h>
#include <stddef.h>
#include <Tools_Core.h>

/*_____________________________________________________________________________
 |
 | Utils
 |_____________________________________________________________________________
*/

typedef struct { TlsBNodProcess TlsBNodProcess; TlsBNod *f; unsigned int *Val; }TlsBNodFoundVal;
static int FirstVal(TlsBNodProcess *this,TlsBNod *n,unsigned int Key) {
    TlsGetThat(TlsBNodFoundVal,TlsBNodProcess);
    that->f = n;
    *that->Val = Key;
    return (0==0);
}
TlsBNod *TlsBNodGetLowest(TlsBTree *rt,unsigned int *from) {
    static struct TlsBNodProcess Static = {&FirstVal};
    TlsBNodFoundVal firstVal = {{&Static},TlsBNodEnd,from};
    x_(rt,Fwd.ForEachFrom,*from,&firstVal.TlsBNodProcess);
    return firstVal.f;
}
TlsBNod *TlsBNodGetHighest(TlsBTree *rt,unsigned int *from) {
    static struct TlsBNodProcess Static = {&FirstVal};
    TlsBNodFoundVal firstVal = {{&Static},TlsBNodEnd,from};
    x_(rt,Bck.ForEachFrom,*from,&firstVal.TlsBNodProcess);
    return firstVal.f;
}

/*_____________________________________________________________________________
 |
 | Notes on binary radix tree: 
 | * Each node stores a key and a bit to test. In strictly decreasing (resp 
 | growing) order from the root. 
 | * At the leaf level, one link points back 
 | toward a nod upper in the branch: it can be tested by checking that the
 | next bit to test is greater (resp lesser) or equal to the current.
 | * Along the branch, all the bits of the key strictly greater (resp lower)
 | than the bit to test are shared with the previous nod in the branch.
 | * The top nodes is special: its bit to test is always set strictly above the 
 | max(resp min) possible; and we start exploring the tree with its 0th 
 | child, the 1st child being set to a special end nod.
 | * All others nodes have exactly two nodes pointing to them: one from above
 | and the other from below (or itself).
 |_____________________________________________________________________________
*/

typedef struct {
    TlsBNod *p[2];
    unsigned int Key;
    int bit;
} BNodUnpacked;

struct TlsBNod {
    union B31Packed {
        uint32_t pL;
        struct {
           unsigned int pH : 14;
           unsigned int KeyL : 18;
        } d1;
        struct {
           unsigned int pH : 14;
           unsigned int KeyH: 13;
           unsigned int bit : 5;
        } d3;
    } d[4];
};
#define B31TopBit 31 
#define B31XtrctKey(_s) (((_s).d[1].d1.KeyL)|(((_s).d[3].d3.KeyH)<<18))
#define B31XtrctBit(_s) (_s).d[3].d3.bit
#define B31XtrctNext(_s,_i) (TlsBNod *)(((uint32_t *)0)+((_s).d[_i<<1].pL|((uint64_t)((_s).d[(_i<<1)+1].d1.pH)<<32)))
#define o4_(_p) (((uint32_t *)(_p))-((uint32_t *)0))
#define B31KeySet(_r,_v) {\
    (_r).d[1].d1.KeyL=_v;\
    (_r).d[3].d3.KeyH=_v>>18;\
}
#define B31NextSet(_r,_i,_v) {\
    size_t off = o4_(_v);\
    union B31Packed *d = (_r).d;\
    d[_i<<1].pL=off;\
    d[(_i<<1)+1].d1.pH=off>>32;\
}
#define B31Unpacked(_s) {{B31XtrctNext(_s,0),B31XtrctNext(_s,1)},B31XtrctKey(_s),B31XtrctBit(_s)}
#define B31Pack(_r,_p) {\
    size_t off0=o4_((_p).p[0]), off1=o4_((_p).p[1]);\
    (_r) =(TlsBNod){{{.pL=off0},{.d1={off0>>32,(_p).Key}},{.pL=off1},{.d3={off1>>32,(_p).Key>>18,(_p).bit}}}};\
}

static int B31NodNumbers(TlsBTree *this) {
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    return that->foo;
}
static unsigned int B31GetKey(TlsBTree *this,TlsBNod *nod) { return B31XtrctKey(*nod); }
static TlsBNod *B31Seek(TlsBTree *this,unsigned int Key) {
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *r,*p=that->Root;
    r = TlsBNodEnd;
    if (p!=TlsBNodEnd) {
        BNodUnpacked nod;
        int bit = B31TopBit;
        TlsBNod *pn = B31XtrctNext(*p,0);
        nod.bit = B31XtrctBit(*pn);
        nod.Key = B31XtrctKey(*pn);
        while ((nod.Key!=Key)&&(nod.bit<bit)) {
            bit = nod.bit;
            int br = (Key>>nod.bit)&1;
            pn = B31XtrctNext(*pn,br);
            nod.bit = B31XtrctBit(*pn);
            nod.Key = B31XtrctKey(*pn);
        }
        if (nod.Key==Key) r=pn;
    }
    return r;
}
static TlsBNod *B31Insert(TlsBTree *this,TlsBNod *n,unsigned int Key) {
    TlsBNod *r;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    r = n;
    if (that->Root==TlsBNodEnd) {
        BNodUnpacked nod={{r,TlsBNodEnd},Key,B31TopBit};
        B31Pack(*r,nod);
        that->foo = 1;
        that->Root = r;
    } else {
        TlsBNod *p;
        struct { TlsBNod *p; int i,bit;} pq;
        int found,bit;
        BNodUnpacked pnod;
        pq.p=that->Root; pq.i=0;
        p = B31XtrctNext(*pq.p,0);
        pq.bit = B31TopBit;
        pnod.Key = B31XtrctKey(*p);
        pnod.bit = B31XtrctBit(*p);
        bit = TlsLog2(Key^pnod.Key);
        while ((pnod.bit<pq.bit)&&(pnod.bit>=bit)&&(pnod.Key!=Key)) {
            pq.bit = pnod.bit;
            pq.p = p; pq.i = ((Key>>pq.bit)&1);
            p = B31XtrctNext(*p,pq.i);
            pnod.Key = B31XtrctKey(*p);
            pnod.bit = B31XtrctBit(*p);
            bit = TlsLog2(Key^pnod.Key);
        }
        if (Key!=pnod.Key) {
            int br = (Key>>bit)&1;
            BNodUnpacked nnod;
            nnod.Key=Key;
            nnod.bit=bit;
            nnod.p[br] = n; nnod.p[br^1] = p;
            B31Pack(*n,nnod);
            B31NextSet(*pq.p,pq.i,n);
            that->foo++;
        } else {
            r = p;
        }
    }
    return r;
}
static TlsBNod *B31Remove(TlsBTree *this,unsigned int Key) {
    TlsBNod *R = TlsBNodEnd;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    /*  p is the nod to suppress (if found) b point to it from above 
        and q point to it from the leaf position. e point to q.
    */
    if (that->Root!=TlsBNodEnd) {
    BNodUnpacked pnod,qnod;
    TlsBNod *p,rt;
    B31NextSet(rt,0,that->Root);
    struct { TlsBNod *p; int idx; } pq,b,e={&rt,0};
    b = e;
    pq.p = that->Root; pq.idx=0;
    qnod.bit = B31TopBit;
    p = B31XtrctNext(*pq.p,0);
    pnod.Key = B31XtrctKey(*p);
    pnod.bit = B31XtrctBit(*p);
    while (qnod.bit>pnod.bit) {
        e = pq;
        if (pnod.Key==Key) b=e;
        pq.p = p; pq.idx = (Key>>pnod.bit)&1;
        qnod.bit = pnod.bit;
        p = B31XtrctNext(*p,pq.idx);
        pnod.Key = B31XtrctKey(*p);
        pnod.bit = B31XtrctBit(*p);
    }
    /* here, we have b!=p and e!=q. Special case are 
       1)  b==e and q==p 
       2)  p==e
     */
    if (pnod.Key==Key) {
        TlsBNod *alt;
        R = p;
        that->foo--;
        B31NextSet(*b.p,b.idx,pq.p);
        alt=B31XtrctNext(*pq.p,(pq.idx^1)); 
        B31NextSet(*e.p,e.idx,alt);
        /* */ {
            unsigned int qKey = B31XtrctKey(*pq.p);
            *pq.p = *p;
            B31KeySet(*pq.p,qKey);
        }
        that->Root = B31XtrctNext(rt,0);
    }}
    return R;
}
static TlsBNod *rB31FwdForEach(TlsBNod *p,TlsBNodProcess *F) {
    int found,i,cbit;
    TlsBNod *r=TlsBNodEnd;
    cbit = B31XtrctBit(*p);
    i = 0;
    do {
        TlsBNod *pn = B31XtrctNext(*p,i);
        int nbit = B31XtrctBit(*pn);
        if (nbit>=cbit) {
            unsigned int nKey = B31XtrctKey(*pn);
            found = x_(F,found,pn,nKey);
            if (found)  r = pn;
        } else {
            r = rB31FwdForEach(pn,F);
            found = (r!=TlsBNodEnd);
        }
        i++;
    } while((!found)&&(i<2));
    return r;
}
static TlsBNod *B31FwdForEach(TlsBTree *this,TlsBNodProcess *Found) {
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *r=TlsBNodEnd,*p=that->Root;
    if (p!=TlsBNodEnd) {
        TlsBNod *pn = B31XtrctNext(*p,0);
        if (p==pn) {
            if (x_(Found,found,p,B31XtrctKey(*p))) r=p;
        } else {
            r = rB31FwdForEach(pn,Found);
        }
    }
    return r;
}
typedef struct {
     unsigned int StartKey;
     unsigned int on;
     TlsBNodProcess *Found;
} rForEachFromClos;
static TlsBNod *rB31FwdForEachFrom(TlsBNod *c,rForEachFromClos *clos) {
    TlsBNod *r;
    r = TlsBNodEnd;
    if (clos->on) {
        r = rB31FwdForEach(c,clos->Found);
    } else {
        BNodUnpacked nod = B31Unpacked(*c);
        int found,i;
        unsigned int start,max,inc;
        start = clos->StartKey;
        max = nod.Key & (((unsigned int)-1)<<(nod.bit+1));
        inc = (1<<nod.bit);
        max = max+inc;
        i = 0; found = (start>=max+inc);
        while ((i<2)&&(!found)) {
            if (max>start) {
                TlsBNod *pn=nod.p[i];
                int nbit = B31XtrctBit(*pn); 
                unsigned int nKey = B31XtrctKey(*pn);
                if (nbit>=nod.bit) {
                     clos->on = (clos->on)||(nKey>=start);
                     if (clos->on) {
                         found = x_(clos->Found,found,pn,nKey);
                         if (found) r = pn;
                     }
                } else {
                     r = rB31FwdForEachFrom(pn,clos);
                     found = (r!=TlsBNodEnd);
                }
            }
            i++; max+=inc;
        }
    }
    return r;
}
static TlsBNod *B31FwdForEachFrom(TlsBTree *this,unsigned int StartKey,TlsBNodProcess *Found) {
    TlsBNod *r,*p,*pn;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    r = TlsBNodEnd; p=that->Root;
    if (p!=TlsBNodEnd) {
        pn = B31XtrctNext(*p,0);
        if (p==pn) {
             unsigned int K=B31XtrctKey(*p);
             if(K>=StartKey) if (x_(Found,found,p,K)) r=p;
        } else {
            rForEachFromClos clos = {StartKey,(0!=0),Found};
            r = rB31FwdForEachFrom(pn,&clos);
        }
    }
    return r;
}
static TlsBNod *rB31BckForEach(TlsBNod *p,TlsBNodProcess *Found) {
    int found,i;
    BNodUnpacked nod;
    TlsBNod *r = TlsBNodEnd;
    nod.Key = B31XtrctKey(*p);
    nod.bit = B31XtrctBit(*p);
    i = 1;
    do {
        TlsBNod *pn = B31XtrctNext(*p,i);
        int nbit = B31XtrctBit(*pn);
        if (nbit>=nod.bit) {
             found = x_(Found,found,pn,B31XtrctKey(*pn));
             if (found) r = pn;
        } else {
             r = rB31BckForEach(pn,Found);
             found = (r!=TlsBNodEnd);
        }
        i--;
    } while ((i>=0)&&(!found));
    return r;
}
static TlsBNod *B31BckForEach(TlsBTree *this,TlsBNodProcess *Found) {
    TlsBNod *r = TlsBNodEnd;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *p = that->Root;
    if (p != TlsBNodEnd) {
        TlsBNod *pn = B31XtrctNext(*p,0);
        if (pn==p){
            if (x_(Found,found,pn,B31XtrctKey(*pn))) r=pn;
        } else {
            r = rB31BckForEach(pn,Found);
        }
    }
    return r;
}
static TlsBNod *rB31BckForEachFrom(TlsBNod *p,rForEachFromClos *clos) {
    TlsBNod *r=TlsBNodEnd;
    if (clos->on) {
        r = rB31BckForEach(p,clos->Found);
    } else {
        BNodUnpacked pnod;
        pnod.bit=B31XtrctBit(*p);
        pnod.Key=B31XtrctKey(*p);
        unsigned int start = clos->StartKey;
        unsigned int inc = 1<<(pnod.bit);
        unsigned int min = (pnod.Key & (((unsigned int)-1)<<(pnod.bit+1)))+inc;
        int i = 1;
        int found = start<(min-inc);
        while((i>=0)&&(!found)) {
            if (start>=min) {
                TlsBNod *pn = B31XtrctNext(*p,i);
                BNodUnpacked nnod;
                nnod.bit = B31XtrctBit(*pn);
                if (nnod.bit>=pnod.bit) {
                    nnod.Key = B31XtrctKey(*pn);
                    clos->on = (clos->on || (nnod.Key<=start));
                    if (clos->on) {
                        found = x_(clos->Found,found,pn,nnod.Key);
                        if (found) r=pn;
                    }
                } else {
                    r=rB31BckForEachFrom(pn,clos);
                    found = (r!=TlsBNodEnd);
                }
            }
            min-=inc; i--;
        }
    }
    return r;
}
static TlsBNod *B31BckForEachFrom(TlsBTree *this,unsigned int StartKey,TlsBNodProcess *Found) {
    TlsBNod *r = TlsBNodEnd;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *p = that->Root;
    if (p!=TlsBNodEnd) {
        TlsBNod *pn = B31XtrctNext(*p,0);
        if (p!=pn) {
            rForEachFromClos clos = {StartKey,(0!=0),Found};
            r = rB31BckForEachFrom(pn,&clos);
        } else {
            unsigned int K=B31XtrctKey(*pn);
            if (K<=StartKey) if (x_(Found,found,p,K)) r=p;
        }
    }
    return r;
}
static void rB31Clear(TlsBNod *p,TlsBNodProcess *Remove) {
    int pbit = B31XtrctBit(*p);
    int i=0;
    while (i<2) {
        TlsBNod *np = B31XtrctNext(*p,i);
        int nbit = B31XtrctBit(*np);
        if (nbit<pbit) {
            rB31Clear(np,Remove);
        }
        i++;
    }
    x_(Remove,found,p,B31XtrctKey(*p));
}
static void B31Clear(TlsBTree *this,TlsBNodProcess *Remove) {
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *p = that->Root;
    if (p!=TlsBNodEnd) {
        TlsBNod *pn = B31XtrctNext(*p,0);
        if (pn==p) {
            x_(Remove,found,pn,B31XtrctKey(*pn));
        } else {
            rB31Clear(pn,Remove);
            x_(Remove,found,p,B31XtrctKey(*p));
        }
    }
    that->foo=0;
    that->Root=TlsBNodEnd;
}
TlsBTree *Tls31RadixInit(TlsBTreeRoot *r,int NodSize) {
    static struct TlsBTree Static = {
        B31NodNumbers,B31GetKey,B31Seek,B31Insert,B31Remove,
        {B31FwdForEach,B31FwdForEachFrom},{B31BckForEach,B31BckForEachFrom},
        B31Clear
    };
    if (NodSize<s_(TlsBNod)) { 
        x_(&TlsCritErrorShutdown,Signal,"Error: initializing 31 radix tree with nod size<16 bytes."); 
    }
    r->TlsBTree.Static = &Static;
    r->Root = TlsBNodEnd;
    r->foo = 0;
    return &r->TlsBTree;
}
