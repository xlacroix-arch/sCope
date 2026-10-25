/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_LineMap.h>

#define PerChunk 63

typedef struct Chunk Chunk;
struct Chunk {
    Chunk *next;
    int n;
    struct { unsigned int out,orig; } m[PerChunk];      /* out lines go up */
};
typedef struct {
    TlsCpp0Hook TlsCpp0Hook;
    TlsHeap *H;
    Chunk *head,*last;
    int warns;
    unsigned int wline;
    char *wmsg;
} BMap;

static struct TlsCpp0Hook Static;

static void MapMark(TlsCpp0Hook *this,unsigned long OutPos,unsigned int OutLine,unsigned int OrigLine) {
    TlsGetThat(BMap,TlsCpp0Hook);
    Chunk *c = that->last;
    (void)OutPos;
    if (c->n==PerChunk) {
        if (!c->next) {
            Chunk *nc;
            TlsHeapIn(that->H)
                rPush(nc);
            TlsHeapOut
            nc->next = 0; nc->n = 0;
            c->next = nc;
        }
        c = that->last = c->next;
        c->n = 0;
    }
    c->m[c->n].out = OutLine;
    c->m[c->n].orig = OrigLine;
    c->n++;
}
static void MapWarn(TlsCpp0Hook *this,unsigned int OrigLine,char *Msg) {
    TlsGetThat(BMap,TlsCpp0Hook);
    if (!that->warns++) { that->wline = OrigLine; that->wmsg = Msg; }
}
static void MapReset(TlsCpp0Hook *this) {
    TlsGetThat(BMap,TlsCpp0Hook);
    that->last = that->head;
    that->head->n = 0;
    that->warns = 0;
}

TlsCpp0Hook *TlsLineMapNew(TlsHeap *Where) {
    BMap *r;
    Chunk *c;
    TlsHeap *H = Where ? Where : TlsHeapFork();
    Static.Mark = MapMark; Static.Warn = MapWarn; Static.Reset = MapReset;
    TlsHeapIn(H)
        rPush(r);
        rPush(c);
    TlsHeapOut
    c->next = 0; c->n = 0;
    r->TlsCpp0Hook.Static = &Static;
    r->H = H;
    r->head = r->last = c;
    r->warns = 0; r->wline = 0; r->wmsg = 0;
    return &r->TlsCpp0Hook;
}

unsigned int TlsLineMapOrig(TlsCpp0Hook *m,unsigned int OutLine) {
    BMap *that;
    Chunk *c;
    unsigned int out = 0,orig = 0;
    if (m->Static!=&Static) return OutLine;
    that = v_(BMap,TlsCpp0Hook,m);
    for (c=that->head;c;c=c->next) {
        int lo = 0,hi = c->n;                           /* the last mark with out<=OutLine */
        if (!c->n || c->m[0].out>OutLine) break;
        while (lo+1<hi) { int mid = (lo+hi)/2; if (c->m[mid].out<=OutLine) lo = mid; else hi = mid; }
        out = c->m[lo].out; orig = c->m[lo].orig;
        if (c==that->last) break;
    }
    return out ? OutLine+orig-out : OutLine;
}
int TlsLineMapWarns(TlsCpp0Hook *m,unsigned int *Line,char **Msg) {
    BMap *that;
    if (m->Static!=&Static) return 0;
    that = v_(BMap,TlsCpp0Hook,m);
    if (that->warns) { if (Line) *Line = that->wline; if (Msg) *Msg = that->wmsg; }
    return that->warns;
}
