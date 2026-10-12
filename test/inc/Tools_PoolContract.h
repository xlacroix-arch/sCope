/* Generic conformance test of a TlsMemPool against the contract of Tools_Core.h.
 * `exact`: the pool is expected to serve exactly the sizes asked (BucketSize ==
 * size given to SizeSelect), otherwise BucketSize >= size is enough. */
#ifndef Tools_PoolContract_h
#define Tools_PoolContract_h
#include <Tools_Core.h>
void PoolContractRun(TlsMemPool *root,const char *name,int exact);
#endif
