#include <Tools_TestPool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define NCLS 40
#define MINSHIFT 4
#define HASH 4096

typedef struct tpEntry { struct tpEntry *next; void *p; int cls; } tpEntry;
struct tpPool;
typedef struct { TlsMemPool TlsMemPool; struct tpPool *P; int cls; } tpBucket;
typedef struct tpPool {
    tpBucket Root;                  /* cls -1: no bucket */
    tpBucket B[NCLS];
    tpEntry *Tab[HASH];
    TlsIntCounter *Count;
    int Local;                      /* accounting while there is no counter */
    TestPoolStats St;
} tpPool;

static int ClsOf(unsigned int sz) {
    int c = MINSHIFT;
    if (sz==0) { fprintf(stderr,"TestPool: SizeSelect(0)\n"); abort(); }
    while ((1u<<c)<sz) c++;
    return c;
}
static void Account(tpPool *P,int d) {
    P->St.Bytes += d;
    if (P->Count) x_(P->Count,Add,d); else P->Local += d;
}
static void *tpAllocate(TlsMemPool *this) {
    TlsGetThat(tpBucket,TlsMemPool);
    tpPool *P = that->P;
    if (that->cls<0) { fprintf(stderr,"TestPool: Allocate on the root\n"); abort(); }
    size_t sz = (size_t)1<<that->cls;
    void *m = aligned_alloc(16,sz);
    if (!m) { fprintf(stderr,"TestPool: out of memory\n"); abort(); }
    tpEntry *e = malloc(sizeof *e);
    size_t h = ((size_t)m>>4)%HASH;
    e->p = m; e->cls = that->cls; e->next = P->Tab[h]; P->Tab[h] = e;
    P->St.Live++; P->St.Allocs++;
    Account(P,(int)sz);
    return m;
}
static void tpFree(TlsMemPool *this,void *mem,unsigned int sz) {
    TlsGetThat(tpBucket,TlsMemPool);
    tpPool *P = that->P;
    size_t h = ((size_t)mem>>4)%HASH;
    tpEntry **pe = &P->Tab[h];
    while (*pe && (*pe)->p!=mem) pe = &(*pe)->next;
    if (!*pe) { fprintf(stderr,"TestPool: Free of a block not allocated (or freed twice)\n"); abort(); }
    tpEntry *e = *pe;
    if (e->cls != ClsOf(sz)) {
        P->St.Misrouted++;
        fprintf(stderr,"TestPool: Free with size %u (class %d) of a block of class %d\n",sz,ClsOf(sz),e->cls);
        abort();
    }
    *pe = e->next;
    Account(P,-(int)((size_t)1<<e->cls));
    P->St.Live--; P->St.Frees++;
    free(e); free(mem);
}
static TlsMemPool *tpSizeSelect(TlsMemPool *this,unsigned int sz) {
    TlsGetThat(tpBucket,TlsMemPool);
    tpPool *P = that->P;
    int c = ClsOf(sz);
    P->St.Selects++;
    if (that->cls!=c) P->St.SlowSelects++;
    return &P->B[c].TlsMemPool;
}
static unsigned int tpBucketSize(TlsMemPool *this) {
    TlsGetThat(tpBucket,TlsMemPool);
    return that->cls<0 ? 0 : 1u<<that->cls;
}
static void tpClose(TlsMemPool *this) {
    TlsGetThat(tpBucket,TlsMemPool);
    tpPool *P = that->P;
    assert(that==&P->Root);
    for (int i=0;i<HASH;i++) for (tpEntry *e=P->Tab[i],*n;e;e=n) { n=e->next; free(e->p); free(e); }
    free(P);
}
static TlsIntCounter *tpCountSwitch(TlsMemPool *this,TlsIntCounter *Swp) {
    TlsGetThat(tpBucket,TlsMemPool);
    tpPool *P = that->P;
    int val;
    TlsIntCounter *r = P->Count;
    if (r) { val = x_(r,Add,0); x_(r,Add,-val); } else { val = P->Local; P->Local = 0; }
    if (!Swp) Swp = r;
    if (Swp) x_(Swp,Add,val); else P->Local = val;
    P->Count = Swp;
    return r;
}
static struct TlsMemPool tpStatic = {tpAllocate,tpFree,tpSizeSelect,tpBucketSize,tpClose,tpCountSwitch};

TlsMemPool *TestPoolNew(void) {
    tpPool *P = calloc(1,sizeof *P);
    P->Root.TlsMemPool.Static = &tpStatic; P->Root.P = P; P->Root.cls = -1;
    for (int i=0;i<NCLS;i++) { P->B[i].TlsMemPool.Static = &tpStatic; P->B[i].P = P; P->B[i].cls = i; }
    return &P->Root.TlsMemPool;
}
TestPoolStats TestPoolGetStats(TlsMemPool *any) {
    return v_(tpBucket,TlsMemPool,any)->P->St;
}
void TestPoolResetSelects(TlsMemPool *any) {
    tpPool *P = v_(tpBucket,TlsMemPool,any)->P;
    P->St.Selects = P->St.SlowSelects = 0;
}
