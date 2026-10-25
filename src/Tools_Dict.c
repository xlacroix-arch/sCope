/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <stddef.h>
#include <string.h>
#include <Tools_Dict.h>

#define WordRound(n) ((((size_t)(n))+sizeof(void *)-1)&~(sizeof(void *)-1))

/*_____________________________________________________________________________
 |
 | A hash table of chained entries (it doubles, the entries do not move: only the
 | table is rebuilt), and the numbers are found back thru a directory of pages of
 | 256 pointers (it doubles too, the pages stay).
 |_____________________________________________________________________________
*/

#define PageBits 8
#define PageSize (1<<PageBits)
#define StartBuckets 64
#define StartDir 8

typedef struct Entry Entry;
struct Entry {
    Entry *next;
    unsigned int hash;
    int id,len;
    unsigned char s[];                  /* len bytes, then a 0 */
};
typedef struct {
    TlsDict TlsDict;
    TlsHeap *H;
    Entry **tab;
    unsigned int nb;                    /* buckets, a power of 2 */
    Entry ***dir;
    unsigned int ndir;                  /* pages the directory can point to */
    int count;
} BDict;

/* sz bytes (a multiple of the word) on the dictionary's heap */
static void *DPush(BDict *d,size_t sz) {
    unsigned char *r;
    TlsHeapIn(d->H)
        rnPush(r,sz);
    TlsHeapOut
    return r;
}

static unsigned int Hash(unsigned char *s,int n) {                      /* FNV-1a */
    unsigned int h = 2166136261u;
    while (n-->0) { h ^= *s++; h *= 16777619u; }
    return h;
}

static void GrowTab(BDict *d) {
    unsigned int nn = d->nb*2,i;
    Entry **nt = DPush(d,(size_t)nn*sizeof(Entry *));
    memset(nt,0,(size_t)nn*sizeof(Entry *));
    for (i=0;i<d->nb;i++) {
        Entry *e,*nx;
        for (e=d->tab[i];e;e=nx) {
            unsigned int k = e->hash&(nn-1);
            nx = e->next;
            e->next = nt[k];
            nt[k] = e;
        }
    }
    d->tab = nt;
    d->nb = nn;
}
static void SetId(BDict *d,int id,Entry *e) {
    unsigned int page = (unsigned int)id>>PageBits;
    if (page>=d->ndir) {
        unsigned int nn = d->ndir*2;
        Entry ***nd;
        while (nn<=page) nn *= 2;
        nd = DPush(d,(size_t)nn*sizeof(Entry **));
        memcpy(nd,d->dir,(size_t)d->ndir*sizeof(Entry **));
        memset(nd+d->ndir,0,(size_t)(nn-d->ndir)*sizeof(Entry **));
        d->dir = nd;
        d->ndir = nn;
    }
    if (!d->dir[page]) {
        Entry **pg = DPush(d,(size_t)PageSize*sizeof(Entry *));
        memset(pg,0,(size_t)PageSize*sizeof(Entry *));
        d->dir[page] = pg;
    }
    d->dir[page][id&(PageSize-1)] = e;
}

static Entry *Lookup(BDict *d,unsigned int h,unsigned char *s,int n) {
    Entry *e;
    for (e=d->tab[h&(d->nb-1)];e;e=e->next)
        if (e->hash==h && e->len==n && !memcmp(e->s,s,(size_t)n)) return e;
    return 0;
}
static int DictFind(TlsDict *this,unsigned char *s,int n) {
    TlsGetThat(BDict,TlsDict);
    Entry *e = Lookup(that,Hash(s,n),s,n);
    return e ? e->id : 0;
}
static int DictId(TlsDict *this,unsigned char *s,int n) {
    TlsGetThat(BDict,TlsDict);
    unsigned int h = Hash(s,n);
    Entry *e = Lookup(that,h,s,n);
    if (!e) {
        size_t sz = WordRound(offsetof(Entry,s)+(size_t)n+1);
        unsigned int k;
        e = DPush(that,sz);
        e->hash = h;
        e->len = n;
        e->id = ++that->count;
        if (n) memcpy(e->s,s,(size_t)n);
        e->s[n] = 0;
        k = h&(that->nb-1);
        e->next = that->tab[k];
        that->tab[k] = e;
        SetId(that,e->id,e);
        if ((unsigned int)that->count>that->nb) GrowTab(that);
    }
    return e->id;
}
static unsigned char *DictName(TlsDict *this,int id,int *n) {
    TlsGetThat(BDict,TlsDict);
    Entry *e;
    if (id<1 || id>that->count) return 0;
    e = that->dir[(unsigned int)id>>PageBits][id&(PageSize-1)];
    if (n) *n = e->len;
    return e->s;
}
static int DictCount(TlsDict *this) {
    TlsGetThat(BDict,TlsDict);
    return that->count;
}

TlsDict *TlsDictNew(TlsHeap *Where) {
    static struct TlsDict Static = {DictId,DictFind,DictName,DictCount};
    TlsHeap *H = Where ? Where : TlsHeapFork();
    BDict *d;
    TlsHeapIn(H)
        rPush(d);
    TlsHeapOut
    d->TlsDict.Static = &Static;
    d->H = H;
    d->count = 0;
    d->nb = StartBuckets;
    d->tab = DPush(d,(size_t)StartBuckets*sizeof(Entry *));
    memset(d->tab,0,(size_t)StartBuckets*sizeof(Entry *));
    d->ndir = StartDir;
    d->dir = DPush(d,(size_t)StartDir*sizeof(Entry **));
    memset(d->dir,0,(size_t)StartDir*sizeof(Entry **));
    return &d->TlsDict;
}

/*_____________________________________________________________________________
 |  IdMap
 |_____________________________________________________________________________
*/
typedef struct {
    TlsIdMap TlsIdMap;
    TlsHeap *H;
    void ***dir;
    unsigned int ndir;
} BIdMap;

static void *IdMapGet(TlsIdMap *this,int id) {
    TlsGetThat(BIdMap,TlsIdMap);
    unsigned int page = (unsigned int)id>>PageBits;
    if (id<0 || page>=that->ndir || !that->dir[page]) return 0;
    return that->dir[page][id&(PageSize-1)];
}
static void *IPush(BIdMap *m,size_t sz) {
    void *r;
    TlsHeapIn(m->H)
        rnPush(r,sz);
    TlsHeapOut
    memset(r,0,sz);
    return r;
}
static void IdMapSet(TlsIdMap *this,int id,void *v) {
    TlsGetThat(BIdMap,TlsIdMap);
    unsigned int page = (unsigned int)id>>PageBits;
    if (id<0) return;
    if (page>=that->ndir) {
        unsigned int nn = that->ndir*2;
        void ***nd;
        while (nn<=page) nn *= 2;
        nd = IPush(that,(size_t)nn*sizeof(void **));
        memcpy(nd,that->dir,(size_t)that->ndir*sizeof(void **));
        that->dir = nd;
        that->ndir = nn;
    }
    if (!that->dir[page]) that->dir[page] = IPush(that,(size_t)PageSize*sizeof(void *));
    that->dir[page][id&(PageSize-1)] = v;
}
TlsIdMap *TlsIdMapNew(TlsHeap *Where) {
    static struct TlsIdMap Static = {IdMapGet,IdMapSet};
    TlsHeap *H = Where ? Where : TlsHeapFork();
    BIdMap *m;
    TlsHeapIn(H)
        rPush(m);
    TlsHeapOut
    m->TlsIdMap.Static = &Static;
    m->H = H;
    m->ndir = StartDir;
    m->dir = IPush(m,(size_t)StartDir*sizeof(void **));
    return &m->TlsIdMap;
}
