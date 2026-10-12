#include <Tools_PoolContract.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static char cbuf[256];
static TlsIntCounter *NewCounter(void) {
    TlsHeap *h = TlsBufferHeap(cbuf,cbuf+sizeof cbuf);
    TlsIntCounter *c;
    TlsHeapIn(h)
        c = TlsIntCounterDirect(0);
    TlsHeapOut
    return c;
}
static int Cnt(TlsIntCounter *c) { return x_(c,Add,0); }

static const unsigned Sizes[] = {1,16,17,100,255,256,1000,4096,4097,5000,20032,65536,70000};
#define NS (sizeof Sizes/sizeof *Sizes)

static void Fill(unsigned char *p,size_t n,unsigned seed) { for (size_t i=0;i<n;i++) p[i]=(unsigned char)(seed+i*7); }
static int Check(unsigned char *p,size_t n,unsigned seed) {
    for (size_t i=0;i<n;i++) if (p[i]!=(unsigned char)(seed+i*7)) return 0;
    return 1;
}

typedef struct { unsigned char *p; size_t bs; unsigned sz; unsigned seed; } Blk;

void PoolContractRun(TlsMemPool *root,const char *name,int exact) {
    TlsMemPool *H[NS];
    TlsIntCounter *w = NewCounter();
    TlsIntCounter *prev = x_(root,CountSwitch,w);
    int base = Cnt(w);

    /* 1. handles: stable, idempotent, selectable from any handle */
    for (size_t i=0;i<NS;i++) {
        H[i] = x_(root,SizeSelect,Sizes[i]);
        unsigned bs = x_(H[i],BucketSize);
        assert(bs>=Sizes[i] && bs>0);
        if (exact) assert(bs==Sizes[i]);
        assert(x_(root,SizeSelect,Sizes[i])==H[i]);            /* same size, same handle */
        assert(x_(H[i],SizeSelect,Sizes[i])==H[i]);
        assert(x_(H[i],SizeSelect,bs)==H[i]);                  /* the bucket is its own class */
    }
    for (size_t i=0;i<NS;i++) for (size_t j=0;j<NS;j++) {
        assert(x_(H[j],SizeSelect,Sizes[i])==H[i]);            /* from another bucket */
        if (H[i]==H[j]) assert(x_(H[i],BucketSize)==x_(H[j],BucketSize));
    }
    assert(Cnt(w)==base);                                      /* selecting does not allocate */

    /* 2. allocate: not NULL, aligned, whole bucket usable, no overlap */
    Blk b[NS]; long sum = 0;
    for (size_t i=0;i<NS;i++) {
        b[i].bs = x_(H[i],BucketSize); b[i].sz = Sizes[i]; b[i].seed = (unsigned)(i*31+1);
        b[i].p = x_(H[i],Allocate);
        assert(b[i].p && ((uintptr_t)b[i].p&15)==0);
        Fill(b[i].p,b[i].bs,b[i].seed);
        sum += b[i].bs;
    }
    for (size_t i=0;i<NS;i++) assert(Check(b[i].p,b[i].bs,b[i].seed));
    assert(Cnt(w)-base==sum);                                  /* accounting = bucket sizes */

    /* 3. free thru the bucket, the root, or another bucket */
    for (size_t i=0;i<NS;i++) {
        TlsMemPool *via = (i%3==0)?H[i]:(i%3==1)?root:H[(i+1)%NS];
        x_(via,Free,b[i].p,b[i].sz);
    }
    assert(Cnt(w)==base);

    /* 4. stress: random sizes, handles looked up each time, data checked */
    enum { N = 600 };
    static Blk live[N];
    memset(live,0,sizeof live);
    srand(777);
    for (int op=0;op<20000;op++) {
        int k = rand()%N;
        if (live[k].p) {
            assert(Check(live[k].p,live[k].bs,live[k].seed));
            TlsMemPool *via = (op%4==0)?root:x_(root,SizeSelect,live[k].sz);
            x_(via,Free,live[k].p,live[k].sz);
            live[k].p = 0;
        } else {
            unsigned sz = (rand()%8==0)?(unsigned)(rand()%70000+1):(unsigned)(rand()%300+1);
            TlsMemPool *h = x_(root,SizeSelect,sz);
            live[k].bs = x_(h,BucketSize); live[k].sz = sz; live[k].seed = (unsigned)rand();
            live[k].p = x_(h,Allocate);
            assert(live[k].p && ((uintptr_t)live[k].p&15)==0);
            Fill(live[k].p,live[k].bs,live[k].seed);
        }
    }
    long left = 0;
    for (int k=0;k<N;k++) if (live[k].p) {
        assert(Check(live[k].p,live[k].bs,live[k].seed));
        x_(root,Free,live[k].p,live[k].sz);
        left++;
    }
    assert(Cnt(w)==base);

    /* 5. counter: switching moves what is accumulated, NULL only reports */
    {
        TlsMemPool *h = x_(root,SizeSelect,1000);
        void *m = x_(h,Allocate);
        int v = Cnt(w);
        assert(x_(root,CountSwitch,0)==w && Cnt(w)==v);
        x_(h,Free,m,1000);
        assert(Cnt(w)==base);
    }
    if (prev) x_(root,CountSwitch,prev);
    printf("pool contract [%s]: ok\n",name);
    (void)left;
}
