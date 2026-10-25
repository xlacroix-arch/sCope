/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_OutBuf_h_
#define _Tools_OutBuf_h_

#include <Tools_Core.h>

/*_____________________________________________________________________________
 |
 | OutBuf: a byte buffer that grows, the writing counterpart of a Buffer0. Parsers
 | build a token in it, whatever its size, and once its size is known copy it at
 | that size on the heap (Take).
 |
 | It is a list of chunks, all of the same size (OutChunk bytes, overridable at
 | compile time, a multiple of the word), the way the heap likes them: no block
 | is ever replaced by a bigger one, nothing is copied when it grows. The bytes
 | are written thru p, in the current chunk; e is the end of that chunk:
 |
 |     TlsOutPut(o,c)         one byte (Grow moves to the next chunk when full)
 |     TlsOutWrite(o,s,n)     n bytes
 |     TlsOutLen(o)           how many bytes there are
 |
 | The chunks live on a SCRATCH heap, the object itself on the current one. A
 | heap does not give blocks back before its layer is popped, so Reset keeps the
 | chain and the next token fills the same chunks again: the room taken is that
 | of the biggest token ever built, in chunks, and no more. Reuse the OutBuf.
 | The bytes are not contiguous: read them thru Take (or Walk).
 |
 |   TlsOutBufNew(Scratch)  Scratch is the heap that holds the chunks; NULL forks
 |       the current heap (it lives as long as the layer that holds the OutBuf).
 |   Take(o,&n)  copies the bytes at their exact size on the CURRENT heap, 0
 |       terminated (so that it can be read as a Buffer0 or a C string), and gives
 |       the address; n gets the length (it may be NULL). The OutBuf is left as it
 |       is: call Reset to write the next one.
 |   Reset(o)    forgets the bytes, keeps the chunks.
 |   Grow(o)     moves to the next chunk (made if there is none). Put calls it.
 |_____________________________________________________________________________
*/

#ifndef OutChunk
#define OutChunk 256
#endif

typedef struct {
    struct TlsOutBuf *Static;
    unsigned char *p,*e;                /* write position, end of the current chunk */
    unsigned char *cb;                  /* start of the data of the current chunk */
    unsigned int full;                  /* bytes in the chunks before the current one */
} TlsOutBuf;
struct TlsOutBuf {
    void (*Grow)(TlsOutBuf *this);
    unsigned char *(*Take)(TlsOutBuf *this,int *n);
    void (*Reset)(TlsOutBuf *this);
};

TlsOutBuf *TlsOutBufNew(TlsHeap *Scratch);
void TlsOutWrite(TlsOutBuf *o,void const *s,unsigned int n);

#define TlsOutLen(o) ((int)((o)->full+(unsigned int)((o)->p-(o)->cb)))
#define TlsOutPut(o,c) do {\
    TlsOutBuf *o__ = (o);\
    if (o__->p>=o__->e) x_(o__,Grow);\
    *o__->p++ = (unsigned char)(c);\
} while (0)

#endif
