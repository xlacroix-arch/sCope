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
#include <Tools_Core.h>

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

struct TlsBNod {
    unsigned int iKey;
    TlsBNod *p[2]; 
};

typedef struct {
    unsigned int Key;
    int bit;
} BTUnpackedKey;
#define BT27TopBit ((s_(unsigned int)<<3)-4)
#define BT27XtrctKey(_s) ((_s)>>5)
#define BT27XtrctBit(_s) ((_s)&0x1f)
#define BT27Unpack(_d,_s) _d.Key=BT27XtrctKey(_s); _d.bit=BT27XtrctBit(_s)
#define BT27Unpacked(_s) {BT27XtrctKey(_s),BT27XtrctBit(_s)}
#define BT27Packed(_s)  (_s.Key<<5)+_s.bit
/* static TlsBNod BT27NodEnd = {BT27TopBit+1,{&BT27NodEnd,&BT27NodEnd}}; */
static int BT27NodNumbers(TlsBTree *this) {
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    return that->foo;
}
static unsigned int BT27GetKey(TlsBTree *this,TlsBNod *n) { 
    unsigned int iKey = n->iKey;
    return BT27XtrctKey(iKey); 
}
static TlsBNod *BT27Seek(TlsBTree *this,unsigned int Key) {
    TlsBNod *r;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    r = &TlsBNodEnd;
    if (that->Root!=&TlsBNodEnd) {
        BTUnpackedKey nod;
        int bit = BT27TopBit;
        TlsBNod *p = that->Root->p[0];
        BT27Unpack(nod,p->iKey);
        while ((nod.Key!=Key)&&(nod.bit<bit)) {
            bit = nod.bit;
            p = p->p[(Key>>nod.bit)&1];
            BT27Unpack(nod,p->iKey);
        }
        if (nod.Key==Key) r=p;
    }
    return r;
}
static TlsBNod *BT27Insert(TlsBTree *this,TlsBNod *n,unsigned int Key) {
    TlsBNod *r;
    BTUnpackedKey nod={Key,BT27TopBit};
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    r = n;
    if (that->Root==&TlsBNodEnd) {
        r->iKey=BT27Packed(nod);
        r->p[0]=r;
        r->p[1]=&TlsBNodEnd;
        that->foo = 1;
        that->Root = r;
    } else {
        TlsBNod *p;
        struct { TlsBNod *p; int i,bit;} pq;
        int found,bit;
        BTUnpackedKey pnod;
        pq.p=that->Root; pq.i=0;
        p = pq.p->p[0]; 
        pq.bit = BT27TopBit;
        BT27Unpack(pnod,p->iKey);
        bit = TlsLog2(Key^pnod.Key);
        while ((pnod.bit<pq.bit)&&(pnod.bit>=bit)&&(pnod.Key!=Key)) {
            pq.bit = pnod.bit;
            pq.p = p; pq.i = ((Key>>pq.bit)&1);
            p = p->p[pq.i];
            BT27Unpack(pnod,p->iKey);
            bit = TlsLog2(Key^pnod.Key);
        }
        if (Key!=pnod.Key) {
            BTUnpackedKey nnod={Key,bit};
            n->iKey = BT27Packed(nnod);
            int i = (Key>>nnod.bit)&1;
            n->p[i] = n; n->p[i^1] = p;
            pq.p->p[pq.i] = n;
            that->foo++;
        } else {
            r = p;
        }
    }
    return r;
}
static TlsBNod *BT27Remove(TlsBTree *this,unsigned int Key) {
    TlsBNod *p,*R;
    BTUnpackedKey pnod,qnod;
    struct { TlsBNod *p; int idx; } pq;
    TlsBNod **b,**e;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    /*  p is the nod to suppress (if found) b point to it from above 
        and q point to it from the leaf position. e point to q.
    */
    R = &TlsBNodEnd;
    if (that->Root!=&TlsBNodEnd) {
    e = b = &that->Root;
    pq.p = *b; pq.idx = 0; qnod.bit = BT27TopBit;
    p = pq.p->p[0];
    BT27Unpack(pnod,p->iKey);
    while (qnod.bit>pnod.bit) {
        e = pq.p->p+pq.idx;
        if (pnod.Key==Key) b=e;
        pq.p = p; pq.idx = (Key>>pnod.bit)&1;
        qnod.bit = pnod.bit;
        p = p->p[pq.idx];
        BT27Unpack(pnod,p->iKey);
    }
    /* here, we have b!=p and e!=q. Special case are 
       1)  b==e and q==p 
       2)  p==e
     */
    if (pnod.Key==Key) {
        R = p;
        that->foo--;
        *b = pq.p;
        *e = pq.p->p[pq.idx^1];
        /* */ {
            qnod.Key = BT27XtrctKey(pq.p->iKey);
            qnod.bit = pnod.bit;
            pq.p->iKey = BT27Packed(qnod);
            pq.p->p[0] = p->p[0]; pq.p->p[1] = p->p[1];
        }
    }}
    return R;
}
static TlsBNod *rBT27FwdForEach(TlsBNod *p,TlsBNodProcess *F) {
    int found,i;
    BTUnpackedKey nod;
    TlsBNod *r=&TlsBNodEnd;
    BT27Unpack(nod,p->iKey);
    i = 0;
    do {
        TlsBNod *pn = p->p[i];
        int niKey = pn->iKey;
        if (BT27XtrctBit(niKey)>=nod.bit) {
            found = x_(F,found,pn,BT27XtrctKey(niKey));
            if (found)  r = pn;
        } else {
            r = rBT27FwdForEach(pn,F);
            found = (r!=&TlsBNodEnd);
        }
        i++;
    } while((!found)&&(i<2));
    return r;
}
static TlsBNod *BT27FwdForEach(TlsBTree *this,TlsBNodProcess *Found) {
    TlsBNod *r=&TlsBNodEnd;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *frst = that->Root;
    if (frst!=&TlsBNodEnd) {
        TlsBNod *p = frst->p[0];
        if (p==frst) {
            if (x_(Found,found,p,BT27XtrctKey(p->iKey))) r=p;
        } else {
            r = rBT27FwdForEach(p,Found);
        }
    }
    return r;
}
typedef struct {
     unsigned int StartKey;
     unsigned int on;
     TlsBNodProcess *Found;
} rForEachFromClos;
static TlsBNod *rBT27FwdForEachFrom(TlsBNod *c,rForEachFromClos *clos) {
    TlsBNod *r;
    r = &TlsBNodEnd;
    if (clos->on) {
        r = rBT27FwdForEach(c,clos->Found);
    } else {
        BTUnpackedKey nod;
        int found,i;
        unsigned int start,max,inc;
        BT27Unpack(nod,c->iKey);
        start = clos->StartKey;
        max = nod.Key & (((unsigned int)-1)<<(nod.bit+1));
        inc = (1<<nod.bit);
        max = max+inc;
        i = 0; found = (start>=max+inc);
        while ((i<2)&&(!found)) {
            if (max>start) {
                TlsBNod *pn=c->p[i];
                BTUnpackedKey nod1=BT27Unpacked(pn->iKey);
                if (nod1.bit>=nod.bit) {
                     clos->on = (clos->on)||(nod1.Key>=start);
                     if (clos->on) {
                         found = x_(clos->Found,found,pn,nod1.Key);
                         if (found) r = pn;
                     }
                } else {
                     r = rBT27FwdForEachFrom(pn,clos);
                     found = (r!=&TlsBNodEnd);
                }
            }
            i++; max+=inc;
        }
    }
    return r;
}
static TlsBNod *BT27FwdForEachFrom(TlsBTree *this,unsigned int StartKey,TlsBNodProcess *Found) {
    TlsBNod *r,*frst;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    r = &TlsBNodEnd; frst=that->Root;
    if (frst!=&TlsBNodEnd) {
        TlsBNod *p= frst->p[0];
        if (p==frst) {
             int K=BT27XtrctKey(p->iKey);
             if (K>=StartKey) if(x_(Found,found,p,K)) r=p;
        } else {
            rForEachFromClos clos = {StartKey,(0!=0),Found};
            r = rBT27FwdForEachFrom(p,&clos);
        }
    }
    return r;
}
static TlsBNod *rBT27BckForEach(TlsBNod *p,TlsBNodProcess *Found) {
    int found,i;
    BTUnpackedKey nod;
    TlsBNod *r = &TlsBNodEnd;
    BT27Unpack(nod,p->iKey);
    i = 1;
    do {
        TlsBNod *pn = p->p[i];
        int niKey = pn->iKey;
        if (BT27XtrctBit(niKey)>=nod.bit) {
             found = x_(Found,found,pn,BT27XtrctKey(niKey));
             if (found) r = pn;
        } else {
             r = rBT27BckForEach(pn,Found);
             found = (r!=&TlsBNodEnd);
        }
        i--;
    } while ((i>=0)&&(!found));
    return r;
}
static TlsBNod *BT27BckForEach(TlsBTree *this,TlsBNodProcess *Found) {
    TlsBNod *r = &TlsBNodEnd;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *frst = that->Root;
    if (frst!=&TlsBNodEnd){
        TlsBNod *p=frst->p[0];
        if (p==frst) {
            if (x_(Found,found,p,BT27XtrctKey(p->iKey))) r=p;
        } else {
            r = rBT27BckForEach(p,Found);
        }
    }
    return r;
}
static TlsBNod *rBT27BckForEachFrom(TlsBNod *p,rForEachFromClos *clos) {
    TlsBNod *r=&TlsBNodEnd;
    if (clos->on) {
        r = rBT27BckForEach(p,clos->Found);
    } else {
        BTUnpackedKey pnod=BT27Unpacked(p->iKey);
        unsigned int start = clos->StartKey;
        unsigned int inc = 1<<(pnod.bit);
        unsigned int min = (pnod.Key & (((unsigned int)-1)<<(pnod.bit+1)))+inc;
        int i = 1;
        int found = start<(min-inc);
        while((i>=0)&&(!found)) {
            if (start>=min) {
                TlsBNod *pn = p->p[i];
                BTUnpackedKey nnod = BT27Unpacked(pn->iKey);
                if (nnod.bit>=pnod.bit) {
                    clos->on = (clos->on || (nnod.Key<=start));
                    if (clos->on) {
                        found = x_(clos->Found,found,pn,nnod.Key);
                        if (found) r=pn;
                    }
                } else {
                    r=rBT27BckForEachFrom(pn,clos);
                    found = (r!=&TlsBNodEnd);
                }
            }
            min-=inc; i--;
        }
    }
    return r;
}
static TlsBNod *BT27BckForEachFrom(TlsBTree *this,unsigned int StartKey,TlsBNodProcess *Found) {
    TlsBNod *r = &TlsBNodEnd;
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *frst = that->Root;
    if (frst!=&TlsBNodEnd) {
        TlsBNod *p = frst->p[0];
        if (p==frst) {
            unsigned int K=BT27XtrctKey(p->iKey);
            if (K<=StartKey) if (x_(Found,found,p,K)) r=p;
        } else {
            rForEachFromClos clos = {StartKey,(0!=0),Found};
            r = rBT27BckForEachFrom(p,&clos);
        }
    }
    return r;
}
static void rBT27Clear(TlsBNod *p,TlsBNodProcess *Remove) {
    int pbit = BT27XtrctBit(p->iKey);
    int i=0;
    while (i<2) {
        TlsBNod *np = p->p[i];
        int nbit = BT27XtrctBit(np->iKey);
        if (nbit<pbit) {
            rBT27Clear(np,Remove);
        }
        i++;
    }
    x_(Remove,found,p,BT27XtrctKey(p->iKey));
}
static void BT27Clear(TlsBTree *this,TlsBNodProcess *Remove) {
    TlsGetThat(TlsBTreeRoot,TlsBTree);
    TlsBNod *frst = that->Root;
    if (frst!=&TlsBNodEnd) {
        TlsBNod *p = frst->p[0];
        if (frst!=p) {
            rBT27Clear(p,Remove);
        }
        x_(Remove,found,frst,BT27XtrctKey(frst->iKey));
    }
    that->foo=0;
    that->Root=&TlsBNodEnd;
}
TlsBTree *Tls27RadixInit(TlsBTreeRoot *r,int nodSize) {
    static struct TlsBTree Static = {
        BT27NodNumbers,BT27GetKey,BT27Seek,BT27Insert,BT27Remove,
        {BT27FwdForEach,BT27FwdForEachFrom},{BT27BckForEach,BT27BckForEachFrom},
        BT27Clear
    };
    if (nodSize<s_(TlsBNod)) {
        x_(&TlsCritErrorShutdown,Signal,"Error: initializing 27 radix tree with nod size<24 bytes."); 
    }
    r->TlsBTree.Static = &Static;
    r->Root = &TlsBNodEnd;
    r->foo = 0;
    return &r->TlsBTree;
}
