/* A stand-in child pool for the tests: malloc, one bucket per size rounded to 16.
 * Not thread safe by itself: it is meant to sit under TlsMemPoolShared. */
#include <stdlib.h>
#include <Tools_Core.h>
typedef struct MPBucket {
    TlsMemPool TlsMemPool;
    struct MPPool *Pool;
    unsigned sz;
    struct MPBucket *next;
} MPBucket;
typedef struct MPPool {
    MPBucket Root;
    MPBucket *All;
    long Live;                               /* bytes handed out and not given back */
    TlsIntCounter *Count;
} MPPool;
static void *MPAllocate(TlsMemPool *this) {
    TlsGetThat(MPBucket,TlsMemPool);
    void *r = malloc(that->sz ? that->sz : 1);
    if (!r) abort();
    that->Pool->Live += that->sz;
    x_(that->Pool->Count,Add,(int)that->sz);
    return r;
}
static TlsMemPool *MPSizeSelect(TlsMemPool *this,unsigned int sz);
static void MPFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(MPBucket,TlsMemPool);
    if (sz!=that->sz) that = v_(MPBucket,TlsMemPool,MPSizeSelect(this,sz));
    that->Pool->Live -= that->sz;
    x_(that->Pool->Count,Add,-(int)that->sz);
    free(mem);
}
static TlsMemPool *MPSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(MPBucket,TlsMemPool);
    MPPool *p = that->Pool;
    MPBucket *b;
    sz = sz ? (sz+15)&~15u : 0;
    if (sz==0) return &p->Root.TlsMemPool;
    for (b=p->All;b;b=b->next) if (b->sz==sz) return &b->TlsMemPool;
    b = calloc(1,sizeof(*b));
    b->TlsMemPool.Static = p->Root.TlsMemPool.Static;
    b->Pool = p; b->sz = sz; b->next = p->All;
    p->All = b;
    return &b->TlsMemPool;
}
static unsigned MPBucketSize(TlsMemPool *this) { TlsGetThat(MPBucket,TlsMemPool); return that->sz; }
static void MPClose(TlsMemPool *this) {
    TlsGetThat(MPBucket,TlsMemPool);
    MPPool *p = that->Pool;
    MPBucket *b,*n;
    for (b=p->All;b;b=n) { n = b->next; free(b); }
    free(p);
}
static TlsIntCounter *MPCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(MPBucket,TlsMemPool);
    TlsIntCounter *r = that->Pool->Count;
    if (Swp) { that->Pool->Count = Swp; }
    return r;
}
static MPPool *MallocPoolOpen(void) {
    static struct TlsMemPool Static = {MPAllocate,MPFree,MPSizeSelect,MPBucketSize,MPClose,MPCountSwitch};
    MPPool *p = calloc(1,sizeof(*p));
    p->Root.TlsMemPool.Static = &Static;
    p->Root.Pool = p;
    p->Count = &TlsIntCounterNull;
    return p;
}
 
