/* Generic conformance test of a TlsMemPool against the contract of Tools_Core.h.
 * `exact`: the pool is expected to serve exactly the sizes asked (BucketSize ==
 * size given to SizeSelect), otherwise BucketSize >= size is enough.
 * Sizes down to 0 are exercised: a 0 size bucket still gives valid pointers. */
#ifndef Tools_PoolContract_h
#define Tools_PoolContract_h
#include <Tools_Core.h>
/* `align`: the alignment the pool promises (1 = none): checked on every block. */
void PoolContractRun(TlsMemPool *root,const char *name,int exact,unsigned align);
#endif
