/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <string.h>
#include <Tools_OutBuf.h>

#define WordRound(n) ((((size_t)(n))+sizeof(void *)-1)&~(sizeof(void *)-1))
#define Cap (OutChunk-sizeof(void *))   /* data bytes in a chunk */

typedef struct Chunk Chunk;
struct Chunk {
    Chunk *next;
    unsigned char d[Cap];
};
typedef struct {
    TlsOutBuf TlsOutBuf;
    TlsHeap *H;                         /* the scratch heap, where the chunks are */
    Chunk *head,*cur;
} BOut;

static Chunk *NewChunk(TlsHeap *H) {
    Chunk *c;
    TlsHeapIn(H)
        rPush(c);
    TlsHeapOut
    c->next = 0;
    return c;
}
static void Enter(BOut *that,Chunk *c) {
    that->cur = c;
    that->TlsOutBuf.cb = that->TlsOutBuf.p = c->d;
    that->TlsOutBuf.e = c->d+Cap;
}
static void OutGrow(TlsOutBuf *this) {
    TlsGetThat(BOut,TlsOutBuf);
    Chunk *c = that->cur->next;
    if (!c) c = that->cur->next = NewChunk(that->H);
    this->full += (unsigned int)Cap;
    Enter(that,c);
}
static unsigned char *OutTake(TlsOutBuf *this,int *n) {
    TlsGetThat(BOut,TlsOutBuf);
    size_t len = (size_t)TlsOutLen(this),sz = WordRound(len+1),left = len;
    unsigned char *r,*w;
    Chunk *c;
    rnPush(r,sz);
    for (w=r,c=that->head;left;c=c->next) {
        size_t k = left<Cap ? left : Cap;
        memcpy(w,c->d,k);
        w += k;
        left -= k;
    }
    r[len] = 0;
    if (n) *n = (int)len;
    return r;
}
static void OutReset(TlsOutBuf *this) {
    TlsGetThat(BOut,TlsOutBuf);
    this->full = 0;
    Enter(that,that->head);
}

TlsOutBuf *TlsOutBufNew(TlsHeap *Scratch) {
    static struct TlsOutBuf Static = {OutGrow,OutTake,OutReset};
    BOut *r;
    TlsHeap *H = Scratch ? Scratch : TlsHeapFork();
    rPush(r);
    r->TlsOutBuf.Static = &Static;
    r->H = H;
    r->head = NewChunk(H);
    r->TlsOutBuf.full = 0;
    Enter(r,r->head);
    return &r->TlsOutBuf;
}

void TlsOutWrite(TlsOutBuf *o,void const *s,unsigned int n) {
    unsigned char const *q = s;
    while (n) {
        size_t room = (size_t)(o->e-o->p),k;
        if (!room) { x_(o,Grow); continue; }
        k = n<room ? n : room;
        memcpy(o->p,q,k);
        o->p += k;
        q += k;
        n -= (unsigned int)k;
    }
}
