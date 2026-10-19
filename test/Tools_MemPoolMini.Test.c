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
TlsMemPool *TlsMemPoolMini(TlsMemPool *Parent,int MaxSize,int tight);
void TlsMemPoolMiniStats(TlsMemPool *Handle,unsigned long *got,unsigned long *put);
 
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
 
/* a plain counter */
typedef struct { TlsIntCounter TlsIntCounter; long Val; } Cnt;
static int CntAdd(TlsIntCounter *this,int v) { TlsGetThat(Cnt,TlsIntCounter); that->Val += v; return (int)that->Val; }
static int CntSet(TlsIntCounter *this,int v) { TlsGetThat(Cnt,TlsIntCounter); that->Val = v; return v; }
static struct TlsIntCounter CntStatic = {CntAdd,CntSet};
static void CntInit(Cnt *c) { c->TlsIntCounter.Static = &CntStatic; c->Val = 0; }
 
typedef struct { uint64_t s; } Rng;
static unsigned int rnd(Rng *r) { r->s ^= r->s<<13; r->s ^= r->s>>7; r->s ^= r->s<<17; return (unsigned int)(r->s>>11); }
 
/* The alignment rule of Tools_Core.h. */
static unsigned int RuleAlign(unsigned int s) {
    if (s && !(s&(s-1))) return s;
    if (s%8==0) return 8;
    if (s%4==0) return 4;
    if (s%2==0) return 2;
    return 1;
}
static void CheckAlign(void *p,unsigned int bucketsize) {
    if (bucketsize) CHECK(((uintptr_t)p&(RuleAlign(bucketsize)-1))==0);
}
 
/*---- the contract and the alignment, for several MaxSize ----*/
static void TestContract(int maxs) {
    static void *P[4400];
    static TlsMemPool *H[4400];
    Cnt pc,mc; TlsMemPool *par,*M,*h,*h2; unsigned int sz,n = 0,i,bs; long sum = 0;
    CntInit(&pc); CntInit(&mc);
    par = TlsMainPoolOpen(&pc.TlsIntCounter);
    M = TlsMemPoolMini(par,maxs,0);
    CHECK(pc.Val>0);                                        /* the structure is in the parent */
    x_(M,CountSwitch,&mc.TlsIntCounter);
    CHECK(x_(M,BucketSize)==0);
    CHECK(x_(M,SizeSelect,0)==M);
    h2 = M;
    for (sz=1;sz<=(unsigned int)maxs+70 && sz<4400;sz++) {
        h = x_(M,SizeSelect,sz);
        bs = x_(h,BucketSize);
        CHECK(bs>=sz);
        CHECK(x_(h,SizeSelect,bs)==h);                      /* fixed point */
        CHECK(x_(M,SizeSelect,sz)==h);                      /* same sz, same handle */
        CHECK(x_(h2,SizeSelect,sz)==h);                     /* from any handle */
        if (sz<=(unsigned int)maxs) {
            CHECK(bs<=(unsigned int)maxs);
            if (sz<=32) CHECK(bs==sz);                      /* exact for the tiny */
            else CHECK(bs*16<=sz*17);                       /* never more than 1/16 lost */
        } else {
            { TlsMemPool *ph = x_(par,SizeSelect,sz); CHECK(bs==x_(ph,BucketSize)); } /* the parent's */
        }
        h2 = h;
        H[n] = h; P[n] = x_(h,Allocate); sum += bs;
        CheckAlign(P[n],bs);
        memset(P[n],0xA5,bs);
        n++;
    }
    CHECK(x_(M,CountSwitch,0)==&mc.TlsIntCounter && mc.Val==sum);
    for (i=0;i<n;i++) {                                     /* free thru the bucket, the root, or another */
        unsigned int s = i+1,k = i%3;
        if (k==0) x_(H[i],Free,P[i],s);
        else if (k==1) x_(M,Free,P[i],s);
        else x_(H[(i*7+1)%n],Free,P[i],s);
    }
    CHECK(mc.Val==0);
    x_(M,Close);
    CHECK(pc.Val==0);                                       /* everything went back to the parent */
    x_(par,Close);
    printf("mini contract and alignment, MaxSize %d: ok\n",maxs);
}
 
/*---- a stress with integrity checks ----*/
#define NLIVE 2500
typedef struct { void *p; unsigned int sz; TlsMemPool *h; unsigned char tag; } Live;
static void Fill(Live *l) {
    unsigned int B = x_(l->h,BucketSize),i,e = B<256 ? B : 256;
    unsigned char *c = l->p;
    for (i=0;i<e;i++) c[i] = c[B-1-i] = l->tag;
    for (i=0;i<B;i+=4096) c[i] = l->tag;
}
static void Verify(Live *l) {
    unsigned int B = x_(l->h,BucketSize),i,e = B<256 ? B : 256;
    unsigned char *c = l->p;
    for (i=0;i<e;i++) CHECK(c[i]==l->tag && c[B-1-i]==l->tag);
    for (i=0;i<B;i+=4096) CHECK(c[i]==l->tag);
}
static unsigned int PickSize(Rng *r,unsigned int maxs) {
    unsigned int v = rnd(r)%100;
    if (v<70) return 1+rnd(r)%(maxs<64 ? maxs : 64);
    if (v<90) return 1+rnd(r)%maxs;
    if (v<97) return maxs+1+rnd(r)%2000;
    return maxs+2001+rnd(r)%100000;
}
/* Runs on the root M of a pool of MaxSize maxs; leaves nothing allocated. */
static void Stress(TlsMemPool *M,unsigned int maxs,int ops,uint64_t seed) {
    Live *L = calloc(NLIVE,sizeof(Live));
    Cnt mc; Rng r; long sum = 0; int op,i;
    CntInit(&mc);
    x_(M,CountSwitch,&mc.TlsIntCounter);
    r.s = seed;
    for (op=0;op<ops;op++) {
        Live *l = &L[rnd(&r)%NLIVE];
        if (l->p) {
            unsigned int k = rnd(&r)%3;
            Verify(l);
            sum -= x_(l->h,BucketSize);
            if (k==0) x_(l->h,Free,l->p,l->sz);
            else if (k==1) x_(M,Free,l->p,l->sz);
            else { Live *o = &L[rnd(&r)%NLIVE]; x_(o->h ? o->h : M,Free,l->p,l->sz); }
            l->p = 0;
        } else {
            l->sz = PickSize(&r,maxs);
            l->h = x_(M,SizeSelect,l->sz);
            l->p = x_(l->h,Allocate);
            sum += x_(l->h,BucketSize);
            CheckAlign(l->p,x_(l->h,BucketSize));
            l->tag = (unsigned char)(1+rnd(&r)%250);
            Fill(l);
        }
        if ((op&1023)==0) CHECK(mc.Val==sum);
    }
    for (i=0;i<NLIVE;i++) if (L[i].p) { Verify(&L[i]); x_(L[i].h,Free,L[i].p,L[i].sz); sum -= x_(L[i].h,BucketSize); }
    CHECK(mc.Val==0 && sum==0);
    free(L);
}
static void TestStress(int maxs) {
    Cnt pc; TlsMemPool *par,*M;
    CntInit(&pc);
    par = TlsMainPoolOpen(&pc.TlsIntCounter);
    M = TlsMemPoolMini(par,maxs,0);
    Stress(M,maxs,200000,0x2545F4914F6CDD1Dull);
    x_(M,Close);
    CHECK(pc.Val==0);
    x_(par,Close);
    printf("mini stress, MaxSize %d: ok\n",maxs);
}
 
/*---- pages: economy and hysteresis ----*/
static void TestPages(void) {
    static void *P[10000];
    Cnt pc; TlsMemPool *par,*M,*h; unsigned long g0,p0,g1,p1; int i;
    CntInit(&pc);
    par = TlsMainPoolOpen(&pc.TlsIntCounter);
    M = TlsMemPoolMini(par,64,0);
    h = x_(M,SizeSelect,16);                                /* 30 blocks per 512 bytes page */
    for (i=0;i<10000;i++) P[i] = x_(h,Allocate);
    TlsMemPoolMiniStats(M,&g1,&p1);
    CHECK(g1>=10000/30 && g1<=10000/30+2 && p1==0);
    CHECK(pc.Val<10000*16*1.12+4096);                       /* the parent sees about the data, not more */
    for (i=0;i<10000;i++) x_(h,Free,P[i],16);
    TlsMemPoolMiniStats(M,&g1,&p1);
    CHECK(p1==g1-1);                                        /* one spare is kept */
    for (i=0;i<30;i++) P[i] = x_(h,Allocate);               /* the spare is reused: nothing from the parent */
    TlsMemPoolMiniStats(M,&g0,&p0);
    CHECK(g0==g1);
    for (i=0;i<10000;i++) { void *p = x_(h,Allocate); x_(h,Free,p,16); }   /* around a page boundary: no bounce */
    TlsMemPoolMiniStats(M,&g1,&p1);
    CHECK(g1-g0<=1 && p1==p0);
    for (i=0;i<30;i++) x_(h,Free,P[i],16);
    x_(M,Close);
    CHECK(pc.Val==0);
    x_(par,Close);
    printf("mini pages: ok\n");
}
 
/*---- threads: a Mini per thread, over a Shared pool (and over a buffer of it) ----*/
#define NT 4
typedef struct { TlsMemPool *Shared; unsigned int Buffer; int id; } Job;
static void *Worker(void *arg) {
    Job *j = arg;
    TlsMemPool *par = j->Buffer ? TlsMemPoolBuffered(j->Shared,j->Buffer) : j->Shared;
    TlsMemPool *M = TlsMemPoolMini(par,300+j->id*500,0);
    Stress(M,300+j->id*500,60000,0x9E3779B97F4A7C15ull*(1+j->id));
    x_(M,Close);
    if (j->Buffer) x_(par,Close);
    return 0;
}
static void RunThreads(unsigned int buffer) {
    Cnt pc; TlsMemPool *S; pthread_t t[NT]; Job j[NT]; int i;
    CntInit(&pc);
    S = TlsMemPoolShared(TlsMainPoolOpen(&pc.TlsIntCounter));
    for (i=0;i<NT;i++) { j[i].Shared = S; j[i].Buffer = buffer; j[i].id = i; pthread_create(&t[i],0,Worker,&j[i]); }
    for (i=0;i<NT;i++) pthread_join(t[i],0);
    CHECK(pc.Val==0);
    x_(S,Close);
    printf("%d threads, a mini pool each over a shared pool%s: ok\n",NT,buffer ? " thru a buffer" : "");
}
 
/*---- a buffer over a Mini pool ----*/
static void TestBufferedOver(void) {
    Cnt pc; TlsMemPool *par,*M,*B;
    CntInit(&pc);
    par = TlsMainPoolOpen(&pc.TlsIntCounter);
    M = TlsMemPoolMini(par,1000,0);
    B = TlsMemPoolBuffered(M,8192);
    Stress(B,1000,100000,0xDEADBEEFCAFEull);
    x_(B,Close);
    x_(M,Close);
    CHECK(pc.Val==0);
    x_(par,Close);
    printf("a buffer over a mini pool: ok\n");
}
 
/*---- a parent like a pool pushed on a heap, and the two ways to Close ----*/
/* Its blocks are bumped from big chunks, a Free feeds a list per bucket, and
 * nothing goes back to the system before Close, which releases it all. */
typedef struct HP HP;
typedef struct HB { TlsMemPool TlsMemPool; HP *P; unsigned int sz; void *Free; struct HB *Next; } HB;
struct HP { HB Root; HB *All; char *cur,*end; void *Chunks; TlsIntCounter *Count; };
static int HeapCloses;
static struct TlsMemPool HStatic;
static unsigned int HCls(unsigned int s) { return s==0 ? 0 : s<=8 ? 8 : !(s&(s-1)) ? s : (s+15)&~15u; }
static void *HAlloc(TlsMemPool *t) {
    HB *b = (HB *)t; HP *P = b->P; void *r;
    if (b->Free) {
        r = b->Free;
        memcpy(&b->Free,r,sizeof(void *));
    } else {
        size_t s = b->sz ? b->sz : 1,al = RuleAlign(b->sz ? b->sz : 1),a = ((uintptr_t)P->cur+al-1)&~(al-1);
        if (!P->cur || a+s>(uintptr_t)P->end) {
            size_t n = s+al>((size_t)1<<20) ? s+al : ((size_t)1<<20);
            char *c = malloc(n+16);
            memcpy(c,&P->Chunks,sizeof(void *));
            P->Chunks = c;
            P->cur = c+16; P->end = c+16+n;
            a = ((uintptr_t)P->cur+al-1)&~(al-1);
        }
        r = (void *)a;
        P->cur = (char *)a+s;
    }
    x_(P->Count,Add,(int)b->sz);
    return r;
}
static TlsMemPool *HSel(TlsMemPool *t,unsigned int sz) {
    HP *P = ((HB *)t)->P; HB *b; unsigned int c = HCls(sz);
    if (!c) return &P->Root.TlsMemPool;
    for (b=P->All;b;b=b->Next) if (b->sz==c) return &b->TlsMemPool;
    b = calloc(1,sizeof(*b));
    b->TlsMemPool.Static = &HStatic; b->P = P; b->sz = c; b->Next = P->All; P->All = b;
    return &b->TlsMemPool;
}
static void HFree(TlsMemPool *t,void *mem,unsigned int sz) {
    HB *b = (HB *)HSel(t,sz);
    if (!b->sz) return;
    memcpy(mem,&b->Free,sizeof(void *));
    b->Free = mem;
    x_(b->P->Count,Add,-(int)b->sz);
}
static unsigned int HBucketSize(TlsMemPool *t) { return ((HB *)t)->sz; }
static void HClose(TlsMemPool *t) {
    HP *P = ((HB *)t)->P; HB *b,*n; void *c,*cn;
    for (c=P->Chunks;c;c=cn) { memcpy(&cn,c,sizeof(void *)); free(c); }
    for (b=P->All;b;b=n) { n = b->Next; free(b); }
    free(P);
    HeapCloses++;
}
static TlsIntCounter *HCountSwitch(TlsMemPool *t,TlsIntCounter *Swp) {
    HP *P = ((HB *)t)->P; TlsIntCounter *r = P->Count;
    if (Swp) { int v = x_(r,Add,0); x_(r,Add,-v); x_(Swp,Add,v); P->Count = Swp; }
    return r;
}
static struct TlsMemPool HStatic = {HAlloc,HFree,HSel,HBucketSize,HClose,HCountSwitch};
static TlsMemPool *HeapOpen(TlsIntCounter *c) {
    HP *P = calloc(1,sizeof(*P));
    P->Root.TlsMemPool.Static = &HStatic; P->Root.P = P; P->Count = c;
    return &P->Root.TlsMemPool;
}
static void TestPushed(void) {
    Cnt pc; TlsMemPool *par,*M;
    CntInit(&pc);                                           /* a guest of the parent: it is not closed */
    par = HeapOpen(&pc.TlsIntCounter);
    M = TlsMemPoolMini(par,700,0);
    Stress(M,700,100000,0x1234567ull);
    x_(M,Close);
    CHECK(pc.Val==0 && HeapCloses==0);
    x_(par,Close);
    CHECK(HeapCloses==1);
    CntInit(&pc);                                           /* tight: Close of the Mini pool closes the parent */
    par = HeapOpen(&pc.TlsIntCounter);
    M = TlsMemPoolMini(par,700,1);
    Stress(M,700,100000,0x7654321ull);
    CHECK(pc.Val>0);                                        /* the structure and the pages are in the parent */
    x_(M,Close);
    CHECK(HeapCloses==2);
    printf("mini over a parent that gives nothing back, loose and tight: ok\n");
}
 
int main(void) {
    TestContract(1);
    TestContract(7);
    TestContract(100);
    TestContract(101);
    TestContract(1000);
    TestContract(4095);
    TestStress(64);
    TestStress(101);
    TestStress(4095);
    TestPages();
    RunThreads(0);
    RunThreads(32768);
    TestBufferedOver();
    TestPushed();
    printf("all passed\n");
    return 0;
}
 
