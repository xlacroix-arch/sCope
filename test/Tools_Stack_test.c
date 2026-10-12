#define _GNU_SOURCE
#include <Tools_Core.h>
#include <Tools_TestPool.h>
#include <Tools_PoolContract.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>

/* ---- helpers -------------------------------------------------------------- */

static TlsIntCounter *NewCounter(void *buf,size_t n) {      /* a counter living in buf */
    TlsHeap *h = TlsBufferHeap(buf,(char *)buf+n);
    TlsIntCounter *c;
    TlsHeapIn(h)
        c = TlsIntCounterDirect(0);
    TlsHeapOut
    return c;
}
static int Cnt(TlsIntCounter *c) { return x_(c,Add,0); }

static void Fill(unsigned char *p,size_t n,unsigned seed) { for (size_t i=0;i<n;i++) p[i]=(unsigned char)(seed+i); }
static int Check(unsigned char *p,size_t n,unsigned seed) {
    for (size_t i=0;i<n;i++) if (p[i]!=(unsigned char)(seed+i)) return 0;
    return 1;
}

/* ---- 1. null heap --------------------------------------------------------- */
static void test_null_heap(void) {
    assert(TlsHeap_.Tgt != NULL);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) { int *v; freopen("/dev/null","w",stderr); rPush(v); *v = 1; _exit(0); }
    int st; waitpid(pid,&st,0);
    assert(WIFEXITED(st) && WEXITSTATUS(st) != 0);          /* fatal error, not a crash */
    puts("null heap: allocation with no heap is a clean fatal error: ok");
}

/* ---- 2. pushes across chunks, root close ---------------------------------- */
static void test_push_chunks(void) {
    static char cbuf[256];
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(0,4096,w);
    x_(TlsStackHeap(s),Set);
    enum { N = 5000 };
    static int *a[N];
    for (int i=0;i<N;i++) { int *v; rPush(v); *v = i; a[i] = v; }
    for (int i=0;i<N;i++) assert(*a[i] == i);
    assert(Cnt(w) > 4096);                                   /* chunks were added */
    TlsHeapClose(TlsStackHeap(s));
    assert(Cnt(w) == 0);                                     /* all given back */
    assert(TlsHeap_.Tgt != NULL);
    puts("pushes across chunks, close gives everything back: ok");
}

/* ---- 3. blocks and layers ------------------------------------------------- */
static void test_layers_blocks(void) {
    static char cbuf[256];
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(0,4096,w);
    x_(TlsStackHeap(s),Set);

    int *x; rPush(x); *x = 42;
    void *mark = TlsHeap_.p;
    int w0 = Cnt(w);
    TlsStackNod *L = TlsHeapLayer();
    unsigned char *big; rnPush(big,20000);                   /* a block */
    Fill(big,20000,7);
    assert(Cnt(w)-w0 == (20000+63)/64);                      /* in 64 bytes atoms */
    int *y; rPush(y); *y = 7;                                /* heap goes on after the block */
    assert((void *)y > (void *)x && (char *)y < (char *)x+4096);
    for (int i=0;i<1500;i++) { int *v; rPush(v); *v = i; }   /* several chunks */
    assert(Check(big,20000,7) && *y == 7 && *x == 42);
    assert(TlsHeapLayerPop(L) == 1);
    assert(Cnt(w) == w0);                                    /* block and chunks released */
    assert(TlsHeap_.p >= mark && (char *)TlsHeap_.p-(char *)mark < 8);
    assert(*x == 42);

    /* nested: pop the outer layer straight through the inner one */
    TlsStackNod *L1 = TlsHeapLayer();
    for (int i=0;i<700;i++) { int *v; rPush(v); *v = i; }
    TlsStackNod *L2 = TlsHeapLayer();
    unsigned char *b2; rnPush(b2,300);                       /* just above the block limit */
    memset(b2,1,300);
    for (int i=0;i<700;i++) { int *v; rPush(v); *v = i; }
    (void)L2;
    assert(TlsHeapLayerPop(L1) == 1);
    assert(Cnt(w) == w0);

    /* a layer that is not in the stack is not found */
    TlsStackNod *L3 = TlsHeapLayer();
    assert(TlsHeapLayerPop(L3) == 1);
    TlsHeapClose(TlsStackHeap(s));
    assert(Cnt(w) == 0);
    puts("layers, blocks, nested pops, accounting: ok");
}

/* ---- 4. fork -------------------------------------------------------------- */
static void test_fork(void) {
    static char cbuf[256];
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(0,4096,w);
    TlsHeap *parent = TlsStackHeap(s);
    x_(parent,Set);
    int w0 = Cnt(w);

    TlsStackNod *L = TlsHeapLayer();
    TlsHeap *child = TlsHeapFork();                          /* not installed yet */
    assert(TlsHeap_.Tgt == parent);
    void *pp = TlsHeap_.p;

    unsigned char *stk, *cdata;
    TlsHeapIn(child)
        assert(TlsHeap_.Tgt == child);
        TlsHeapAlign(16);
        rnPush(stk,16384);                                   /* the "fiber stack" */
        assert(((uintptr_t)stk & 15) == 0);
        Fill(stk,16384,3);
        rnPush(cdata,9000);
        Fill(cdata,9000,5);
        for (int i=0;i<2000;i++) { int *v; rPush(v); *v = i; }
    TlsHeapOut
    assert(TlsHeap_.Tgt == parent && TlsHeap_.p == pp);      /* the parent is untouched */
    assert(Check(stk,16384,3) && Check(cdata,9000,5));

    int *z; rPush(z); *z = 99;                               /* the parent goes on */
    assert(Cnt(w) > w0);
    assert(TlsHeapLayerPop(L) == 1);                         /* closes the child */
    assert(Cnt(w) == w0);

    /* forks of forks, popped through the root */
    TlsStackNod *L0 = TlsHeapLayer();
    TlsHeap *c1 = TlsHeapFork();
    TlsHeapIn(c1)
        TlsHeap *c2 = TlsHeapFork();
        TlsHeapIn(c2)
            unsigned char *q; rnPush(q,5000); Fill(q,5000,1);
        TlsHeapOut
    TlsHeapOut
    assert(TlsHeapLayerPop(L0) == 1);
    assert(Cnt(w) == w0);
    TlsHeapClose(parent);
    assert(Cnt(w) == 0);
    puts("fork: child heap, stack on child, forks of forks, closed by the parent pop: ok");
}

/* ---- 5. alignment --------------------------------------------------------- */
static void test_align(void) {
    TlsMainHeapOpen(4096);
    for (int k=0;k<600;k++) {
        char *c; rnPush(c,(k*7)%61+1);
        TlsHeapAlign(64);
        char *d; rnPush(d,1);
        assert(((uintptr_t)d & 63) == 0);
        *d = 1; *c = 1;
    }
    TlsMainHeapClose();
    puts("TlsHeapAlign (also over chunk ends): ok");
}

/* ---- 6. external pool ----------------------------------------------------- */
static void test_extern_pool(void) {
    static char cbuf[256];
    TlsMemPool *tp = TestPoolNew();
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(tp,5000,w);                   /* effective growth: 8192 */
    assert(TlsStackPool(s) == tp);
    x_(TlsStackHeap(s),Set);
    assert(TestPoolGetStats(tp).Live == 1);
    assert(Cnt(w) == 8192);                                  /* the bucket size, not 5000 */
    TlsStackNod *L = TlsHeapLayer();
    TlsHeap *c = TlsHeapFork();
    TlsHeapIn(c)
        unsigned char *b; rnPush(b,10000); Fill(b,10000,9);
        for (int i=0;i<1000;i++) { int *v; rPush(v); *v = i; }
    TlsHeapOut
    unsigned char *b2; rnPush(b2,5000); Fill(b2,5000,2);
    for (int i=0;i<1000;i++) { int *v; rPush(v); *v = i; }
    assert(TestPoolGetStats(tp).Live > 3);
    assert(TlsHeapLayerPop(L) == 1);
    assert(TestPoolGetStats(tp).Live == 1);
    TlsHeapClose(TlsStackHeap(s));
    TestPoolStats st = TestPoolGetStats(tp);
    assert(st.Live == 0 && st.Bytes == 0 && st.Misrouted == 0);
    assert(Cnt(w) == 0);
    x_(tp,Close);
    puts("external pool: effective growth, everything allocated is freed to its bucket: ok");
}

/* ---- 6b. blocks go to the bucket class of their size, SizeSelect is rare ---- */
static void test_block_buckets(void) {
    static char cbuf[256];
    TlsMemPool *tp = TestPoolNew();
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(tp,4096,w);
    x_(TlsStackHeap(s),Set);
    TlsStackNod *L = TlsHeapLayer();
    TestPoolResetSelects(tp);
    for (int i=0;i<10;i++) { unsigned char *b; rnPush(b,20000); Fill(b,20000,i); }
    TestPoolStats st = TestPoolGetStats(tp);
    assert(st.Bytes == 4096 + 10*32768L);                    /* 20032 -> class 32768 */
    assert(st.SlowSelects <= 2);                             /* handle cached across blocks */
    assert(TlsHeapLayerPop(L) == 1);
    st = TestPoolGetStats(tp);
    assert(st.Bytes == 4096 && st.SlowSelects <= 4);
    TlsHeapClose(TlsStackHeap(s));
    assert(TestPoolGetStats(tp).Live == 0);
    x_(tp,Close);
    puts("blocks: freed to their class, SizeSelect seldom: ok");
}

/* ---- 6c. the pool contract, on noPool and on the test pool ------------------ */
static void test_contract(void) {
    static char cbuf[256];
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(0,4096,w);
    PoolContractRun(TlsStackPool(s),"noPool",1,16);
    TlsStackClose(s);
    assert(Cnt(w) == 0);
    TlsMemPool *tp = TestPoolNew();
    PoolContractRun(tp,"TestPool",0,16);
    assert(TestPoolGetStats(tp).Live == 0);
    x_(tp,Close);
    tp = TestPoolNewSkewed(5);
    PoolContractRun(tp,"TestPool skewed",0,1);
    x_(tp,Close);
}

/* ---- 6d. a pool that promises no alignment: the stack aligns by itself ------ */
static void test_skewed_pool(void) {
    static char cbuf[256];
    for (int skew=8; skew<=8; skew+=8) {   /* 8 aligned chunks, blocks not 16 aligned */
        TlsMemPool *tp = TestPoolNewSkewed(skew);
        TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
        TlsStack *s = TlsStackOpen(tp,4096,w);
        x_(TlsStackHeap(s),Set);
        for (int round=0; round<20; round++) {
            TlsStackNod *L = TlsHeapLayer();
            for (int i=0;i<400;i++) {
                TlsHeapAlign(16);
                struct { void *a; long b; } *v; rPush(v);
                v->a = v; v->b = i;
                if (i%97==0) { unsigned char *b; rnPush(b,300+i); Fill(b,300+i,i); }
                if (i%131==0) {
                    TlsHeap *c = TlsHeapFork();
                    TlsHeapIn(c) TlsHeapAlign(64); unsigned char *q; rnPush(q,2000); Fill(q,2000,3); TlsHeapOut
                    TlsStackNod *L2 = TlsHeapLayer(); (void)L2;
                }
            }
            assert(TlsHeapLayerPop(L) == 1);
        }
        TlsHeapClose(TlsStackHeap(s));
        assert(Cnt(w) == 0);
        assert(TestPoolGetStats(tp).Live == 0);
        x_(tp,Close);
    }
    /* chunks the stack cannot use are a clean fatal error */
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) { freopen("/dev/null","w",stderr); TlsStackOpen(TestPoolNewSkewed(3),4096,0); _exit(0); }
    int st; waitpid(pid,&st,0);
    assert(WIFEXITED(st) && WEXITSTATUS(st) != 0);
    puts("pool with weaker alignment: layers, blocks, forks, aligned pushes; unusable chunks refused: ok");
}

/* ---- 7. randomized layers / pushes / pops against a model ------------------ */
#define MAXA 4000
static struct { unsigned char *p; size_t n; unsigned seed; } al[MAXA];
static struct { TlsStackNod *nod; int na; } ly[200];

static void test_random(void) {
    static char cbuf[256];
    TlsIntCounter *w = NewCounter(cbuf,sizeof cbuf);
    TlsStack *s = TlsStackOpen(0,4096,w);
    x_(TlsStackHeap(s),Set);
    int w0 = Cnt(w);
    int na = 0, nl = 0, pops = 0, forks = 0;
    srand(12345);
    for (int op=0; op<30000; op++) {
        int r = rand() % 100;
        if (r < 70 && na < MAXA) {
            size_t n = (rand()%10==0) ? (size_t)(rand()%9000+1) : (size_t)(rand()%200+1);
            unsigned char *p; rnPush(p,n);
            unsigned seed = (unsigned)rand();
            Fill(p,n,seed);
            al[na].p = p; al[na].n = n; al[na].seed = seed; na++;
        } else if (r < 85 && nl < 200) {
            ly[nl].nod = TlsHeapLayer(); ly[nl].na = na; nl++;
        } else if (r < 90 && nl < 200) {                      /* a fork that lives and dies in the layer */
            TlsHeap *c = TlsHeapFork();
            TlsHeapIn(c)
                unsigned char *p; size_t n = (size_t)(rand()%12000+1); rnPush(p,n); Fill(p,n,1);
            TlsHeapOut
            forks++;
        } else if (nl > 0) {
            int j = rand() % nl;
            assert(TlsHeapLayerPop(ly[j].nod) == 1);
            na = ly[j].na; nl = j; pops++;
        }
        if (op % 150 == 0) for (int i=0;i<na;i++) assert(Check(al[i].p,al[i].n,al[i].seed));
    }
    for (int i=0;i<na;i++) assert(Check(al[i].p,al[i].n,al[i].seed));
    TlsHeapClose(TlsStackHeap(s));
    assert(Cnt(w) == 0 && w0 > 0);
    printf("randomized (%d pops, %d forks) against a model: ok\n",pops,forks);
}

int main(void) {
    test_null_heap();
    test_push_chunks();
    test_layers_blocks();
    test_fork();
    test_align();
    test_extern_pool();
    test_block_buckets();
    test_contract();
    test_skewed_pool();
    test_random();
    puts("all ok");
    return 0;
}
