/* Test pool: a reference TlsMemPool with power of 2 buckets (16 and up) and a bucket of size 0.
 * It checks the contract as it goes: Free must come with the size of the
 * allocating bucket class, a double free or a foreign pointer is caught.
 * The root has no bucket of its own (BucketSize 0). Test only. */
#ifndef Tools_TestPool_h
#define Tools_TestPool_h
#include <Tools_Core.h>

typedef struct {
    long Live;          /* blocks allocated and not freed */
    long Bytes;         /* their bucket sizes, summed */
    long Selects;       /* calls to SizeSelect */
    long SlowSelects;   /* those that were not the identity (a real lookup) */
    long Allocs,Frees;
    long Misrouted;     /* Free with a size of another bucket class */
} TestPoolStats;

TlsMemPool *TestPoolNew(void);
/* Same, but every block starts `skew` bytes (1..15) after a 16 boundary: for
 * code that must not assume any alignment from a pool. */
TlsMemPool *TestPoolNewSkewed(int skew);
TestPoolStats TestPoolGetStats(TlsMemPool *any);
void TestPoolResetSelects(TlsMemPool *any);
#endif
