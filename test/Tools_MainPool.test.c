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
#include <Tools_Core_Debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
 
TlsMemPool *TlsMainPoolOpen(TlsIntCounter *Count);
unsigned int TlsMainPoolClasses(void);
unsigned int TlsMainPoolClassOf(unsigned int sz);
unsigned int TlsMainPoolClassSize(unsigned int c);
 
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
 
/* a plain counter for the pool to account in */
typedef struct { TlsIntCounter TlsIntCounter; long Val; } Cnt;
static int CntAdd(TlsIntCounter *this,int v) { TlsGetThat(Cnt,TlsIntCounter); that->Val += v; return (int)that->Val; }
static int CntSet(TlsIntCounter *this,int v) { TlsGetThat(Cnt,TlsIntCounter); that->Val = v; return v; }
static struct TlsIntCounter CntStatic = {CntAdd,CntSet};
static void CntInit(Cnt *c) { c->TlsIntCounter.Static = &CntStatic; c->Val = 0; }
 
static uint64_t rs = 88172645463325252ull;
static unsigned int rnd(void) { rs ^= rs<<13; rs ^= rs>>7; rs ^= rs<<17; return (unsigned int)(rs>>11); }
 
/*---- the class table ----*/
static unsigned int Pow2Floor(unsigned int v) { unsigned int p = 1; while (p*2<=v) p *= 2; return p; }
static void TestClasses(void) {
    unsigned int n = TlsMainPoolClasses(),c,sz,p;
    unsigned int mxs = 0,mxat = 0;
    CHECK(n>=200 && n<400);
    CHECK(TlsMainPoolClassSize(0)==1 && TlsMainPoolClassSize(n-1)==(1u<<22));
    for (sz=1;sz<=64;sz++) CHECK(TlsMainPoolClassSize(sz-1)==sz);       /* tiny sizes: one class each */
    for (c=1;c<n;c++) {
        unsigned int s = TlsMainPoolClassSize(c),q = TlsMainPoolClassSize(c-1);
        CHECK(s>q);                                         /* strictly increasing */
        CHECK(TlsMainPoolClassOf(s)==c);                    /* idempotent: SizeSelect(h,BucketSize(h))==h */
        if (s>64 && (s-q)*1000/s>mxs) { mxs = (s-q)*1000/s; mxat = s; }
    }
    for (p=1;p<=(1u<<22);p<<=1) CHECK(TlsMainPoolClassSize(TlsMainPoolClassOf(p))==p);    /* powers of 2 stay */
    for (sz=1;sz<=(1u<<22);sz++) {
        unsigned int c = TlsMainPoolClassOf(sz),s = TlsMainPoolClassSize(c);
        CHECK(s>=sz);                                       /* serves the request */
        if (c) CHECK(TlsMainPoolClassSize(c-1)<sz);         /* and is the first class that does */
        if (sz>64) CHECK((uint64_t)(s-sz)*8<=s);           /* waste at most 1/8 */
    }
    printf("classes: ok (%u classes up to 4 MiB; widest step above 64: %u.%u%% at %u)\n",n,mxs/10,mxs%10,mxat);
}
 
/*---- chunk geometry and alignment ----*/
static int cmpp(const void *a,const void *b) {
    uintptr_t x = (uintptr_t)*(void *const *)a,y = (uintptr_t)*(void *const *)b;
    return x<y ? -1 : x>y;
}
static void TestGeometry(void) {
    static void *P[4200];
    Cnt cnt; TlsMemPool *R,*h; unsigned int n,c,nslab = 0,nlarge = 0;
    double wsum = 0,wmax = 0;
    CntInit(&cnt);
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    n = TlsMainPoolClasses();
    for (c=0;c<n;c++) {
        unsigned int s = TlsMainPoolClassSize(c),C,N,a,i,k;
        h = x_(R,SizeSelect,s);
        CHECK(x_(h,BucketSize)==s);
        a = TlsMainPoolAlign(h);
        CHECK(a && !(a&(a-1)));
        if (!(s&(s-1))) CHECK(a==s);                        /* a power of 2 is aligned on itself */
        if (s>=256) {                                       /* 4 from 256, 8 from 512, 16 from 1024 ... (page at most) */
            unsigned int want = Pow2Floor(s/64);
            CHECK(a>=(want<4096?want:4096));
        }
        if (TlsMainPoolSlabInfo(h,&C,&N)) {
            unsigned int head = 32+4*((N+31)/32);
            double w = 100.0*(C-(double)N*s)/C;
            CHECK(C>=256 && C<=(1u<<22) && !(C&(C-1)));
            CHECK((uint64_t)N*s+head<=C);                   /* the objects, the bitmap and the header fit */
            CHECK(N>=6 && N<=4000);
            if (s>64) { wsum += w; nslab++; if (w>wmax) wmax = w; }
            if (s>64 && (s&(s-1))) CHECK(w<8.0);            /* header, bitmap and rounding only */
            k = N+2; if (k>4200) k = 4200;
        } else { nlarge++; k = 3; CHECK(s>=(1u<<17)); }
        for (i=0;i<k;i++) { P[i] = x_(h,Allocate); *(char *)P[i] = 1; }
        for (i=0;i<k;i++) {
            CHECK(((uintptr_t)P[i]&(a-1))==0);
            if (!(s&(s-1))) CHECK(((uintptr_t)P[i]%s)==0);
        }
        qsort(P,k,sizeof(P[0]),cmpp);
        for (i=1;i<k;i++) CHECK((uintptr_t)P[i]-(uintptr_t)P[i-1]>=s);     /* no overlap */
        for (i=0;i<k;i++) x_(h,Free,P[i],s);
    }
    CHECK(cnt.Val==0);
    x_(R,Close);
    printf("geometry+alignment: ok (%u slab classes above 64: waste avg %.2f%% max %.2f%%; %u large)\n",nslab,wsum/nslab,wmax,nlarge);
}
 
/*---- handles ----*/
static void TestHandles(void) {
    Cnt cnt; TlsMemPool *R,*h; int i;
    CntInit(&cnt);
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    CHECK(x_(R,BucketSize)==0);
    CHECK(x_(R,SizeSelect,0)==R);
    for (i=0;i<20000;i++) {
        unsigned int sz = 1+(rnd()%(i%7==0?6000000:70000));
        h = x_(R,SizeSelect,sz);
        CHECK(x_(h,BucketSize)>=sz);
        CHECK(x_(h,SizeSelect,x_(h,BucketSize))==h);        /* fixed point */
        CHECK(x_(R,SizeSelect,sz)==h);                      /* same sz, same handle */
    }
    x_(R,Close);
    printf("handles: ok\n");
}
 
/*---- random alloc/free ----*/
typedef struct { void *p; unsigned int sz; TlsMemPool *h; unsigned char tag; } Live;
static void Fill(Live *l) {
    unsigned int B = x_(l->h,BucketSize),i;
    unsigned char *c = l->p;
    if (B<=4096) memset(c,l->tag,B);
    else { memset(c,l->tag,256); memset(c+B-256,l->tag,256); for (i=0;i<B;i+=4096) c[i] = l->tag; }
}
static void Verify(Live *l) {
    unsigned int B = x_(l->h,BucketSize),i;
    unsigned char *c = l->p;
    if (B<=4096) { for (i=0;i<B;i++) CHECK(c[i]==l->tag); }
    else {
        for (i=0;i<256;i++) CHECK(c[i]==l->tag && c[B-256+i]==l->tag);
        for (i=0;i<B;i+=4096) CHECK(c[i]==l->tag);
    }
}
static unsigned int PickSize(void) {
    unsigned int r = rnd()%10000;
    if (r<6000) return 1+rnd()%64;
    if (r<8500) return 65+rnd()%1936;
    if (r<9500) return 2001+rnd()%68000;
    if (r<9990) return 70001+rnd()%400000;
    if (r<9997) return (1u<<22)-rnd()%4;
    return (1u<<22)+1+rnd()%3000000;
}
#define NLIVE 4000
static void TestStress(void) {
    static Live L[NLIVE];
    Cnt cnt,cnt2,*cur = &cnt; TlsMemPool *R,*h; long sum = 0; int op;
    unsigned long got,put;
    CntInit(&cnt); CntInit(&cnt2);
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    memset(L,0,sizeof(L));
    for (op=0;op<400000;op++) {
        Live *l = &L[rnd()%NLIVE];
        if (l->p) {
            unsigned int mode = rnd()%3;
            Verify(l);
            if (mode==0) x_(l->h,Free,l->p,l->sz);
            else if (mode==1) x_(R,Free,l->p,l->sz);
            else {                                          /* through the handle of some other size */
                TlsMemPool *o = x_(R,SizeSelect,PickSize());
                x_(o,Free,l->p,l->sz);
            }
            sum -= x_(l->h,BucketSize);
            l->p = 0;
        } else {
            l->sz = PickSize();
            l->h = x_(R,SizeSelect,l->sz);
            l->p = x_(l->h,Allocate);
            l->tag = (unsigned char)(1+rnd()%250);
            Fill(l);
            sum += x_(l->h,BucketSize);
        }
        if (op%2500==0) CHECK(cur->Val==sum);
        if (op==200000) {                                   /* switch the counter in the middle */
            TlsIntCounter *old = x_(R,CountSwitch,&cnt2.TlsIntCounter);
            CHECK(old==&cnt.TlsIntCounter && cnt.Val==0 && cnt2.Val==sum);
            CHECK(x_(R,CountSwitch,0)==&cnt2.TlsIntCounter);
            cur = &cnt2;
        }
    }
    for (op=0;op<NLIVE;op++) {
        Live *l = &L[op];
        if (l->p) { Verify(l); x_(l->h,Free,l->p,l->sz); sum -= x_(l->h,BucketSize); l->p = 0; }
    }
    CHECK(sum==0);
    CHECK(cur->Val==0);
    TlsMainPoolChunkStats(R,&got,&put);
    printf("stress: ok (chunks got %lu, put %lu, %lu kept as spares)\n",got,put,got-put);
    h = x_(R,SizeSelect,1);                                 /* the pool still works */
    x_(h,Free,x_(h,Allocate),1);
    x_(R,Close);
}
 
/*---- hysteresis ----*/
static void TestHysteresis(void) {
    static void *P[4000];
    Cnt cnt; TlsMemPool *R,*h; unsigned int C,N,i;
    unsigned long got,put;
    CntInit(&cnt);
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    h = x_(R,SizeSelect,64);
    CHECK(TlsMainPoolSlabInfo(h,&C,&N));
    CHECK(3*N<=4000);
    for (i=0;i<3*N;i++) P[i] = x_(h,Allocate);
    TlsMainPoolChunkStats(R,&got,&put);
    CHECK(got==3 && put==0);
    for (i=0;i<3*N;i++) x_(h,Free,P[i],64);
    TlsMainPoolChunkStats(R,&got,&put);
    CHECK(got==3 && put==2);                                /* one spare kept, two released */
    for (i=0;i<5000;i++) { void *p = x_(h,Allocate); x_(h,Free,p,64); }
    TlsMainPoolChunkStats(R,&got,&put);
    CHECK(got==3 && put==2);                                /* no thrash around the boundary */
    for (i=0;i<N+1;i++) P[i] = x_(h,Allocate);              /* spare, then a new chunk */
    TlsMainPoolChunkStats(R,&got,&put);
    CHECK(got==4 && put==2);
    for (i=0;i<N+1;i++) x_(h,Free,P[i],64);
    x_(R,Close);
    printf("hysteresis: ok (chunk %u bytes, %u objects of 64)\n",C,N);
    /* a big tier gives its empty chunk back at once: no spare */
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    h = x_(R,SizeSelect,20000);
    CHECK(TlsMainPoolSlabInfo(h,&C,&N) && C>16384 && N<=4000);
    for (i=0;i<N;i++) P[i] = x_(h,Allocate);
    for (i=0;i<N;i++) x_(h,Free,P[i],20000);
    TlsMainPoolChunkStats(R,&got,&put);
    CHECK(got==1 && put==1);
    for (i=0;i<10;i++) { void *p = x_(h,Allocate); x_(h,Free,p,20000); }
    TlsMainPoolChunkStats(R,&got,&put);
    CHECK(got==11 && put==11);                              /* the price of no spare: a chunk per round trip */
    x_(R,Close);
    printf("no spare for big tiers: ok (chunk %u bytes, %u objects of 20000)\n",C,N);
}
 
/*---- fullest chunk first ----*/
static void TestFullest(void) {
    static void *P[1000];
    Cnt cnt; TlsMemPool *R,*h; unsigned int C,N,i,nx = 0,ny = 0;
    uintptr_t X,Y;
    void *keepX[10],*keepY[100];
    CntInit(&cnt);
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    h = x_(R,SizeSelect,16);
    CHECK(TlsMainPoolSlabInfo(h,&C,&N) && N>=200 && 2*N<=1000);
    for (i=0;i<2*N;i++) P[i] = x_(h,Allocate);
    X = (uintptr_t)P[0] & ~(uintptr_t)(C-1);
    Y = 0;
    for (i=0;i<2*N;i++) {
        uintptr_t c = (uintptr_t)P[i] & ~(uintptr_t)(C-1);
        if (c!=X) { CHECK(!Y || Y==c); Y = c; }
    }
    CHECK(Y);
    for (i=0;i<2*N;i++) {                                   /* free 10 of X, 100 of Y */
        uintptr_t c = (uintptr_t)P[i] & ~(uintptr_t)(C-1);
        if (c==X && nx<10) { x_(h,Free,P[i],16); keepX[nx++] = P[i]; P[i] = 0; }
        else if (c==Y && ny<100) { x_(h,Free,P[i],16); keepY[ny++] = P[i]; P[i] = 0; }
    }
    for (i=0;i<10;i++) {
        void *p = x_(h,Allocate);
        CHECK(((uintptr_t)p & ~(uintptr_t)(C-1))==X);       /* the fuller chunk is refilled first */
        P[i] = p;
    }
    {   void *p = x_(h,Allocate);
        CHECK(((uintptr_t)p & ~(uintptr_t)(C-1))==Y);       /* then the other */
        x_(h,Free,p,16);
    }
    (void)keepX; (void)keepY;
    x_(R,Close);
    printf("occupancy: ok (fullest chunk refilled first)\n");
}
 
/*---- misc ----*/
static void TestMisc(void) {
    Cnt cnt; TlsMemPool *R,*h,*r; void *p,*q; unsigned int C,N;
    CntInit(&cnt);
    R = TlsMainPoolOpen(&cnt.TlsIntCounter);
    p = x_(R,Allocate); q = x_(R,Allocate);                 /* the root is the 0-size bucket */
    CHECK(p && q && p!=q);
    x_(R,Free,p,0); x_(R,Free,q,0);
    h = x_(R,SizeSelect,(1u<<22)+1);                        /* above 4 MiB: 64 KiB steps */
    CHECK(x_(h,BucketSize)==(1u<<22)+65536);
    CHECK(!TlsMainPoolSlabInfo(h,&C,&N));
    p = x_(h,Allocate); ((char *)p)[x_(h,BucketSize)-1] = 1;
    x_(R,Free,p,(1u<<22)+1);
    h = x_(R,SizeSelect,1u<<21);                            /* a big power of 2: one block, aligned on its size */
    CHECK(!TlsMainPoolSlabInfo(h,&C,&N) && x_(h,BucketSize)==(1u<<21));
    p = x_(h,Allocate);
    CHECK(((uintptr_t)p&((1u<<21)-1))==0);
    r = x_(R,SizeSelect,1);
    x_(r,Free,p,1u<<21);                                    /* freed thru the handle of a slab of another size */
    h = x_(R,SizeSelect,300000);                            /* a slab in a 4 MiB chunk */
    CHECK(TlsMainPoolSlabInfo(h,&C,&N) && C==(1u<<22) && N>=8);
    p = x_(h,Allocate);
    x_(r,Free,p,300000);                                    /* freed thru the handle of another bucket */
    CHECK(cnt.Val==0);
    x_(R,Close);
    printf("misc: ok\n");
}
 
int main(void) {
    TestClasses();
    TestGeometry();
    TestHandles();
    TestStress();
    TestHysteresis();
    TestFullest();
    TestMisc();
    printf("all passed\n");
    return 0;
}
 
