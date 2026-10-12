/*  Fondement Michtam
 *  Copyright (C) 2026 <AI> Claude
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
#include <Tools_Chunk.h>
#include <Tools_Core_Debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
 
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
#define KiB 1024ul
#define MiB (1024ul*1024ul)
 
static uint64_t rs = 88172645463325252ull;
static unsigned int rnd(void) { rs ^= rs<<13; rs ^= rs>>7; rs ^= rs<<17; return (unsigned int)(rs>>11); }
 
static long Status(const char *key) {
    FILE *f = fopen("/proc/self/status","r");
    char l[256];
    long v = 0;
    while (fgets(l,sizeof(l),f)) if (!strncmp(l,key,strlen(key))) { sscanf(l+strlen(key)," %ld",&v); break; }
    fclose(f);
    return v;
}
static long RssKB(void) { return Status("VmRSS:"); }
static long VmKB(void) { return Status("VmSize:"); }
 
static void TouchPages(void *p,unsigned int sz) {
    unsigned int i;
    for (i=0;i<sz;i+=4096) ((char *)p)[i] = 1;
    ((char *)p)[sz-1] = 1;
}
 
/*---- random get/put: alignment, integrity (no overlap), exactness ----*/
typedef struct { unsigned char *p; unsigned int sz,align; unsigned char tag; } Blk;
#define NB 3000
static unsigned int PickSize(unsigned int *align) {
    unsigned int r = rnd()%1000,sz;
    if (r<600) { sz = 1024u<<(rnd()%8); *align = sz; }                          /* 1 KiB .. 128 KiB */
    else if (r<800) { sz = 256*1024u<<(rnd()%5); *align = sz; }                 /* 256 KiB .. 4 MiB */
    else if (r<900) { sz = 1024*(3+rnd()%60); if (!(sz&(sz-1))) sz += 1024; *align = 1024; }     /* not a power of 2 */
    else if (r<990) { sz = 4096*(3+rnd()%1000); if (!(sz&(sz-1))) sz += 4096; *align = 4096; }
    else { sz = 65536*(65+rnd()%80); *align = 4096; }                           /* above 4 MiB */
    return sz;
}
static void FillB(Blk *b) {
    unsigned int i;
    memset(b->p,b->tag,b->sz<4096?b->sz:4096);
    memset(b->p+b->sz-(b->sz<4096?b->sz:4096),b->tag,b->sz<4096?b->sz:4096);
    for (i=4096;i<b->sz;i+=65536) b->p[i] = b->tag;
}
static void VerifyB(Blk *b) {
    unsigned int i,n = b->sz<4096?b->sz:4096;
    for (i=0;i<n;i++) CHECK(b->p[i]==b->tag && b->p[b->sz-n+i]==b->tag);
    for (i=4096;i<b->sz;i+=65536) CHECK(b->p[i]==b->tag);
}
static int cmpb(const void *a,const void *b) {
    uintptr_t x = (uintptr_t)(*(Blk *const *)a)->p,y = (uintptr_t)(*(Blk *const *)b)->p;
    return x<y ? -1 : x>y;
}
static void Disjoint(Blk *L) {
    static Blk *V[NB];
    unsigned int n = 0,i;
    for (i=0;i<NB;i++) if (L[i].p) V[n++] = &L[i];
    qsort(V,n,sizeof(V[0]),cmpb);
    for (i=1;i<n;i++) CHECK(V[i-1]->p+V[i-1]->sz<=V[i]->p);
}
static void TestRandom(void) {
    static Blk L[NB];
    TlsChunkSource *A = TlsChunkSourceArena();
    TlsChunkArenaInfo in;
    uint64_t live = 0;
    int op;
    memset(L,0,sizeof(L));
    for (op=0;op<150000;op++) {
        Blk *b = &L[rnd()%NB];
        if (b->p) {
            VerifyB(b);
            x_(A,Put,b->p,b->sz);
            live -= b->sz; b->p = 0;
        } else {
            unsigned int al;
            unsigned int sz = PickSize(&al);
            if (live+sz>192*MiB) continue;
            b->sz = sz; b->align = al;
            b->p = x_(A,Get,sz,al);
            CHECK(((uintptr_t)b->p&(al-1))==0);
            b->tag = (unsigned char)(1+rnd()%250);
            FillB(b);
            live += sz;
        }
        if (op%15000==0) Disjoint(L);
    }
    TlsChunkArenaStats(A,&in);
    CHECK(in.InUse==live);
    for (op=0;op<NB;op++) if (L[op].p) { VerifyB(&L[op]); x_(A,Put,L[op].p,L[op].sz); }
    TlsChunkArenaStats(A,&in);
    CHECK(in.InUse==0);
    CHECK(in.Supers<=2 && in.FreeSupers==in.Supers && in.FreeSupers<=1+0u+(in.Supers>1));
    printf("random: ok (%lu mmap, %lu munmap, %lu madvise; %u supers left)\n",in.Mmap,in.Munmap,in.Madvise,in.Supers);
    x_(A,Close);
}
 
/*---- merging: everything freed gives back one whole super ----*/
static void TestMerge(void) {
    static void *P[4096];
    TlsChunkSource *A = TlsChunkSourceArena();
    TlsChunkArenaInfo in;
    unsigned int i,j;
    for (i=0;i<4096;i++) P[i] = x_(A,Get,1024,1024);
    TlsChunkArenaStats(A,&in);
    CHECK(in.Supers==1 && in.FreeCached==0);                /* 4096 chunks of 1 KiB are exactly a super */
    for (i=4095;i>0;i--) {                                  /* free in random order */
        j = rnd()%(i+1);
        void *t = P[i]; P[i] = P[j]; P[j] = t;
    }
    for (i=0;i<4096;i++) x_(A,Put,P[i],1024);
    TlsChunkArenaStats(A,&in);
    CHECK(in.Supers==1 && in.FreeSupers==1 && in.FreeCached==4*MiB);
    x_(A,Close);
    printf("merge: ok (4096 chunks of 1 KiB freed in random order: one whole free super)\n");
}
 
/*---- placement: the lowest address first ----*/
static void TestLowest(void) {
    static unsigned char *P[64];
    TlsChunkSource *A = TlsChunkSourceArena();
    unsigned int i;
    for (i=0;i<64;i++) P[i] = x_(A,Get,65536,65536);
    for (i=1;i<64;i++) CHECK(P[i]==P[i-1]+65536);           /* contiguous, upward */
    x_(A,Put,P[40],65536); x_(A,Put,P[10],65536); x_(A,Put,P[25],65536);
    CHECK(x_(A,Get,65536,65536)==P[10]);                    /* lowest first, not last freed */
    CHECK(x_(A,Get,65536,65536)==P[25]);
    CHECK(x_(A,Get,65536,65536)==P[40]);
    x_(A,Close);
    printf("placement: ok (lowest address first)\n");
}
 
/*---- exact sizes: no rounding to a power of 2 ----*/
static void TestExact(void) {
    void *P[5],*Q[1000];
    TlsChunkSource *A = TlsChunkSourceArena();
    TlsChunkArenaInfo in;
    unsigned int i;
    for (i=0;i<5;i++) { P[i] = x_(A,Get,704*1024,4096); CHECK(((uintptr_t)P[i]&4095)==0); TouchPages(P[i],704*1024); }
    TlsChunkArenaStats(A,&in);
    CHECK(in.Supers==2);                                    /* a 704 KiB extent takes a 1 MiB block: 4 in a super, the 5th in another */
    CHECK(in.FreeCached==8*MiB-5*704*KiB);                  /* but only the 704 KiB are used: the tails are free blocks */
    for (i=0;i<5;i++) x_(A,Put,P[i],704*1024);
    TlsChunkArenaStats(A,&in);
    CHECK(in.FreeSupers==1 && in.InUse==0);
    for (i=0;i<1000;i++) Q[i] = x_(A,Get,3072,1024);        /* 3 KiB extents, aligned on 1 KiB */
    for (i=0;i<1000;i++) CHECK(((uintptr_t)Q[i]&1023)==0);
    for (i=0;i<1000;i++) x_(A,Put,Q[i],3072);
    TlsChunkArenaStats(A,&in);
    CHECK(in.FreeSupers==1 && in.FreeCached==4*MiB);
    x_(A,Close);
    printf("exact: ok (704 KiB and 3 KiB extents, tails merged back)\n");
}
 
/*---- above 4 MiB ----*/
static void TestBig(void) {
    TlsChunkSource *A = TlsChunkSourceArena();
    unsigned int sz;
    long v0 = VmKB();
    for (sz=5*1024*1024;sz<=9*1024*1024;sz+=1024*1024+65536) {
        unsigned char *p = x_(A,Get,sz,4096);
        CHECK(((uintptr_t)p&4095)==0);
        p[0] = 1; p[sz-1] = 2;
        x_(A,Put,p,sz);
    }
    CHECK(VmKB()-v0<2048);                                  /* given back at once */
    x_(A,Close);
    printf("big: ok\n");
}
 
/*---- memory really given back ----*/
static void TestRelease(void) {
    static void *P[2048];
    TlsChunkSource *A = TlsChunkSourceArena();
    TlsChunkArenaInfo in;
    long r0 = RssKB(),r1,r2;
    unsigned int i;
    for (i=0;i<2048;i++) { P[i] = x_(A,Get,65536,65536); memset(P[i],7,65536); }     /* 128 MiB resident */
    r1 = RssKB();
    CHECK(r1-r0>120*1024);
    for (i=0;i<2048;i++) x_(A,Put,P[i],65536);
    r2 = RssKB();
    TlsChunkArenaStats(A,&in);
    CHECK(r2-r0<16*1024);                                   /* at most the cache allowance stays */
    CHECK(in.Madvise+in.Munmap>0 && in.Supers<=2);   /* whole free supers are unmapped, the rest madvised */
    x_(A,Close);
    printf("release: ok (RSS +%ld MiB with 128 MiB in use, +%ld MiB once freed; %lu madvise, %lu munmap)\n",
        (r1-r0)/1024,(r2-r0)/1024,in.Madvise,in.Munmap);
}
 
/* The same pattern on both sources: 4 KiB chunks, 63 of 64 given back. */
static long Retained(TlsChunkSource *S,int *pct) {
    enum { N = 16384 };
    void **P = malloc(N*sizeof(void *));
    long r0 = RssKB(),r1,r2;
    unsigned int i;
    for (i=0;i<N;i++) { P[i] = x_(S,Get,4096,4096); memset(P[i],3,4096); }
    r1 = RssKB();
    for (i=0;i<N;i++) if (i%64) x_(S,Put,P[i],4096);
    r2 = RssKB();
    *pct = (int)(100*(r2-r0)/(r1-r0));
    for (i=0;i<N;i+=64) x_(S,Put,P[i],4096);
    free(P);
    return r1-r0;
}
static void TestRetained(void) {
    int pa,pl;
    TlsChunkSource *A = TlsChunkSourceArena();
    Retained(TlsChunkSourceLibc(),&pl);
    Retained(A,&pa);
    CHECK(pa<45);
    x_(A,Close);
    printf("retained: ok (4 KiB chunks, 63/64 freed: RSS still %d%% with the arena, %d%% with libc)\n",pa,pl);
}
 
static void TestClose(void) {
    TlsChunkSource *A = TlsChunkSourceArena();
    void *p = x_(A,Get,4096,4096);
    void *q = x_(A,Get,6*1024*1024,4096);
    (void)p; (void)q;
    x_(A,Close);                                            /* live blocks are the caller's affair */
    printf("close: ok\n");
}
 
int main(void) {
    TestRandom();
    TestMerge();
    TestLowest();
    TestExact();
    TestBig();
    TestRelease();
    TestRetained();
    TestClose();
    printf("all passed\n");
    return 0;
}

