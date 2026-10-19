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
 
#include <Tools_Core.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
 
TlsMemPool *TlsMainPoolOpen(TlsIntCounter *Count);
TlsMemPool *TlsMemPoolShared(TlsMemPool *child);
TlsMemPool *TlsMemPoolBuffered(TlsMemPool *child,unsigned int buffersize);
unsigned long TlsMemPoolSharedLocks(TlsMemPool *Handle);
unsigned long TlsMemPoolBufferedHeld(TlsMemPool *Handle);
 
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
 
/* a plain counter for the child to account in */
typedef struct { TlsIntCounter TlsIntCounter; long Val; } Cnt;
static int CntAdd(TlsIntCounter *this,int v) { TlsGetThat(Cnt,TlsIntCounter); that->Val += v; return (int)that->Val; }
static int CntSet(TlsIntCounter *this,int v) { TlsGetThat(Cnt,TlsIntCounter); that->Val = v; return v; }
static struct TlsIntCounter CntStatic = {CntAdd,CntSet};
static void CntInit(Cnt *c) { c->TlsIntCounter.Static = &CntStatic; c->Val = 0; }
 
typedef struct { uint64_t s; } Rng;
static unsigned int rnd(Rng *r) { r->s ^= r->s<<13; r->s ^= r->s>>7; r->s ^= r->s<<17; return (unsigned int)(r->s>>11); }
 
static unsigned int PickSize(Rng *r) {
    unsigned int v = rnd(r)%1000;
    if (v<600) return 1+rnd(r)%64;
    if (v<900) return 65+rnd(r)%1936;
    if (v<995) return 2001+rnd(r)%30000;
    return 32001+rnd(r)%200000;
}
 
/*---- the contract, single thread ----*/
static void TestContract(void) {
    Cnt cnt; TlsMemPool *R,*h,*g; int i; Rng r = {88172645463325252ull};
    CntInit(&cnt);
    R = TlsMemPoolShared(TlsMainPoolOpen(&cnt.TlsIntCounter));
    CHECK(x_(R,BucketSize)==0);
    { TlsMemPool *z = x_(R,SizeSelect,0); CHECK(z==R || x_(z,BucketSize)==0); }
    for (i=0;i<5000;i++) {
        unsigned int sz = PickSize(&r);
        h = x_(R,SizeSelect,sz);
        CHECK(x_(h,BucketSize)>=sz);
        CHECK(x_(h,SizeSelect,x_(h,BucketSize))==h);        /* fixed point */
        CHECK(x_(R,SizeSelect,sz)==h);                      /* same sz, same handle */
        CHECK(x_(h,SizeSelect,sz)==h);
    }
    h = x_(R,SizeSelect,100);
    {   void *p = x_(h,Allocate);
        memset(p,5,x_(h,BucketSize));
        CHECK(cnt.Val==(long)x_(h,BucketSize));             /* the counter of the child is the one in use */
        g = x_(R,SizeSelect,3000);
        x_(g,Free,p,100);                                   /* thru another handle */
        CHECK(cnt.Val==0);
    }
    {   Cnt c2; TlsIntCounter *old;
        void *p = x_(h,Allocate);
        CntInit(&c2);
        old = x_(R,CountSwitch,&c2.TlsIntCounter);
        CHECK(old==&cnt.TlsIntCounter && cnt.Val==0 && c2.Val==(long)x_(h,BucketSize));
        CHECK(x_(R,CountSwitch,0)==&c2.TlsIntCounter);
        x_(h,Free,p,100);
        CHECK(c2.Val==0);
    }
    x_(R,Close);                                            /* closes the child too */
    printf("shared contract: ok\n");
}
 
/*---- threads ----*/
#define NT 4
#define NLIVE 600
typedef struct { void *p; unsigned int sz; TlsMemPool *h; unsigned char tag; } Live;
static void Fill(Live *l) {
    unsigned int B = x_(l->h,BucketSize),i;
    unsigned char *c = l->p;
    if (B<=1024) memset(c,l->tag,B);
    else { memset(c,l->tag,256); memset(c+B-256,l->tag,256); for (i=0;i<B;i+=4096) c[i] = l->tag; }
}
static void Verify(Live *l) {
    unsigned int B = x_(l->h,BucketSize),i;
    unsigned char *c = l->p;
    if (B<=1024) { for (i=0;i<B;i++) CHECK(c[i]==l->tag); }
    else {
        for (i=0;i<256;i++) CHECK(c[i]==l->tag && c[B-256+i]==l->tag);
        for (i=0;i<B;i+=4096) CHECK(c[i]==l->tag);
    }
}
typedef struct { TlsMemPool *Shared; unsigned int Buffer; int id; } Job;
#define NSZ 48                                              /* a program uses few sizes: handles are kept */
static void *Worker(void *arg) {
    Job *j = arg;
    Live *L = calloc(NLIVE,sizeof(Live));
    Rng r; int op,i;
    TlsMemPool *R = j->Buffer ? TlsMemPoolBuffered(j->Shared,j->Buffer) : j->Shared;
    unsigned int sizes[NSZ]; TlsMemPool *hs[NSZ];
    r.s = 0x9E3779B97F4A7C15ull*(1+j->id);
    for (i=0;i<NSZ;i++) { sizes[i] = PickSize(&r); hs[i] = x_(R,SizeSelect,sizes[i]); }
    for (op=0;op<60000;op++) {
        Live *l = &L[rnd(&r)%NLIVE];
        if (l->p) {
            Verify(l);
            if (rnd(&r)&1) x_(l->h,Free,l->p,l->sz); else x_(R,Free,l->p,l->sz);
            l->p = 0;
        } else {
            i = rnd(&r)%NSZ;
            l->sz = sizes[i];
            l->h = hs[i];
            l->p = x_(l->h,Allocate);
            l->tag = (unsigned char)(1+rnd(&r)%250);
            Fill(l);
        }
    }
    for (i=0;i<NLIVE;i++) if (L[i].p) { Verify(&L[i]); x_(L[i].h,Free,L[i].p,L[i].sz); }
    free(L);
    if (j->Buffer) x_(R,Close);
    return 0;
}
static void RunThreads(unsigned int buffer) {
    Cnt cnt; TlsMemPool *S; pthread_t t[NT]; Job j[NT]; int i;
    CntInit(&cnt);
    S = TlsMemPoolShared(TlsMainPoolOpen(&cnt.TlsIntCounter));
    for (i=0;i<NT;i++) { j[i].Shared = S; j[i].Buffer = buffer; j[i].id = i; pthread_create(&t[i],0,Worker,&j[i]); }
    for (i=0;i<NT;i++) pthread_join(t[i],0);
    CHECK(cnt.Val==0);                                      /* all given back, buffers included */
    printf("%d threads over a shared pool%s: ok (%lu lock round trips)\n",NT,
        buffer ? ", each with its own buffer" : "",TlsMemPoolSharedLocks(S));
    x_(S,Close);
}
 
/*---- the buffer ----*/
static void TestBuffered(void) {
    static void *P[4000];
    Cnt cnt,own; TlsMemPool *S,*B,*h,*big,*tiny; unsigned long l0,l1; int round; unsigned int i;
    const unsigned int budget = 65536;
    CntInit(&cnt); CntInit(&own);
    S = TlsMemPoolShared(TlsMainPoolOpen(&cnt.TlsIntCounter));
    B = TlsMemPoolBuffered(S,budget);
    CHECK(x_(B,CountSwitch,&own.TlsIntCounter)!=0);
    h = x_(B,SizeSelect,64);
    CHECK(x_(h,BucketSize)==64 && x_(h,SizeSelect,64)==h && x_(B,SizeSelect,64)==h);
    tiny = x_(B,SizeSelect,3);                              /* below a link: served by the 8 bytes bucket */
    CHECK(x_(tiny,BucketSize)==8 && x_(B,SizeSelect,8)==tiny && x_(tiny,SizeSelect,1)==tiny);
 
    l0 = TlsMemPoolSharedLocks(S);
    for (round=0;round<5;round++) {
        for (i=0;i<4000;i++) { P[i] = x_(h,Allocate); memset(P[i],round,64); CHECK(TlsMemPoolBufferedHeld(B)<=budget); }
        CHECK(own.Val==4000*64);
        for (i=0;i<4000;i++) { x_(h,Free,P[i],64); CHECK(TlsMemPoolBufferedHeld(B)<=budget); }
        CHECK(own.Val==0);
    }
    l1 = TlsMemPoolSharedLocks(S);
    CHECK(l1-l0<40000/20);                                  /* 40000 operations, a few dozen accesses */
    printf("buffered: ok (40000 operations of 64 bytes: %lu accesses to the shared pool)\n",l1-l0);
 
    for (i=0;i<1000;i++) P[i] = x_(h,Allocate);             /* a steady state around a boundary: no storm */
    { void *p = x_(h,Allocate); x_(h,Free,p,64); }          /* the buffer holds something now */
    l0 = TlsMemPoolSharedLocks(S);
    for (i=0;i<20000;i++) { void *p = x_(h,Allocate); x_(h,Free,p,64); }
    CHECK(TlsMemPoolSharedLocks(S)==l0);
    for (i=0;i<1000;i++) x_(h,Free,P[i],64);
 
    {   void *p = x_(h,Allocate);                           /* Free thru the root: the sizes are cached, no lock */
        x_(B,Free,p,64);                                    /* (the first one may learn the size) */
        l0 = TlsMemPoolSharedLocks(S);
        for (i=0;i<20000;i++) { p = x_(h,Allocate); x_(B,Free,p,64); }
        CHECK(TlsMemPoolSharedLocks(S)==l0);
    }
    {   TlsMemPool *g = x_(B,SizeSelect,1000),*s48 = x_(B,SizeSelect,48);   /* a refill that must flush first: one access */
        unsigned int bs = x_(s48,BucketSize),n = 0;
        void *p;
        for (i=0;i<64;i++) P[i] = x_(g,Allocate);
        while (n<64 && TlsMemPoolBufferedHeld(B)+127*bs<=budget) x_(g,Free,P[n++],1000);
        CHECK(n<64 && TlsMemPoolBufferedHeld(B)+127*bs>budget && TlsMemPoolBufferedHeld(B)<=budget);
        l0 = TlsMemPoolSharedLocks(S);
        p = x_(s48,Allocate);
        CHECK(TlsMemPoolSharedLocks(S)-l0==1);
        CHECK(TlsMemPoolBufferedHeld(B)<=budget/2+127*bs);  /* it did flush */
        x_(s48,Free,p,48);
        while (n<64) x_(g,Free,P[n++],1000);
    }
 
    big = x_(B,SizeSelect,20000);                           /* above a quarter of the buffer: not buffered */
    l0 = TlsMemPoolSharedLocks(S);
    for (i=0;i<100;i++) { void *p = x_(big,Allocate); x_(big,Free,p,20000); }
    CHECK(TlsMemPoolSharedLocks(S)-l0>=200);
    {   void *p = x_(tiny,Allocate);                        /* freed thru another handle and another size */
        memset(p,1,8);
        x_(B,Free,p,3);
        p = x_(h,Allocate);
        { TlsMemPool *o = x_(B,SizeSelect,5000); x_(o,Free,p,64); }
    }
    CHECK(own.Val==0);
    x_(B,Close);                                            /* gives the held blocks back */
    CHECK(cnt.Val==0);
    x_(S,Close);
    printf("buffered bypass, other handles, close: ok\n");
}
 
/*---- a buffer over a plain pool ----*/
static void TestPlain(void) {
    static void *P[3000];
    Cnt cnt; TlsMemPool *M,*B,*h; unsigned int i;
    CntInit(&cnt);
    M = TlsMainPoolOpen(&cnt.TlsIntCounter);
    B = TlsMemPoolBuffered(M,4096);
    h = x_(B,SizeSelect,100);
    for (i=0;i<3000;i++) { P[i] = x_(h,Allocate); memset(P[i],7,100); CHECK(TlsMemPoolBufferedHeld(B)<=4096); }
    for (i=0;i<3000;i++) { x_(h,Free,P[i],100); CHECK(TlsMemPoolBufferedHeld(B)<=4096); }
    x_(B,Close);
    CHECK(cnt.Val==0);
    x_(M,Close);
    printf("buffered over a plain pool: ok\n");
}
 
int main(void) {
    TestContract();
    RunThreads(0);
    RunThreads(65536);
    TestBuffered();
    TestPlain();
    printf("all passed\n");
    return 0;
}
 
