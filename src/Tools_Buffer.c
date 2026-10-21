/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <Tools_Core.h>
#include <Tools_Buffer.h>

/*_____________________________________________________________________________
 |
 | Buffer0 readers, see Tools_Buffer.h for the contract. Everything is pushed on
 | the current heap; the only things that are not are the two static readers.
 |_____________________________________________________________________________
*/

unsigned char const TlsBufferEob[1] = {0};
unsigned char const TlsBufferEof[1] = {0};
#define EOB ((unsigned char *)TlsBufferEob)

/* The heap is assumed aligned on entry, and every push must leave it so: the
 * structures are pushed as they are (their size is a multiple of their own
 * alignment), and the byte blocks are rounded up here, the only place where
 * alignment is done. The rounding unit is the pointer size, not int: on a 64
 * bits target a struct holding pointers pushed after an int-rounded block would
 * be misaligned (UBSan reports it). On a target where int is as large as a
 * pointer this is the same thing. */
#define WordRound(n) ((((size_t)(n))+sizeof(void *)-1)&~(sizeof(void *)-1))

/*_____________________________________________________________________________
 |
 | Null and Empty: over from the start. Nothing is written, they are shared.
 |_____________________________________________________________________________
*/

static unsigned char *NullCheck0(TlsBuffer0 *this,unsigned char *pos) { (void)this; (void)pos; return EOB; }
static unsigned char *NullReset(TlsBuffer0 *this) { (void)this; return EOB; }
static void NullClose(TlsBuffer0 *this) { (void)this; }
static struct TlsBuffer0 NullStatic = {NullCheck0,NullReset,NullClose};
TlsBuffer0 TlsBuffer0Null_ = {&NullStatic,(unsigned char *)TlsBufferEob};
TlsBuffer0 TlsEmptyBuffer0 = {&NullStatic,(unsigned char *)TlsBufferEob};

/*_____________________________________________________________________________
 |
 | A C string, read in place: its only 0 is its end.
 |_____________________________________________________________________________
*/

typedef struct {
    TlsBuffer0 TlsBuffer0;
    unsigned char *s;
} BStr;
static unsigned char *StrCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BStr,TlsBuffer0);
    (void)pos;
    that->TlsBuffer0.p = EOB;
    return EOB;
}
static unsigned char *StrReset(TlsBuffer0 *this) {
    TlsGetThat(BStr,TlsBuffer0);
    return that->TlsBuffer0.p = that->s;
}
static void StrClose(TlsBuffer0 *this) {
    this->p = EOB;
}
TlsBuffer0 *TlsStringBuffer(unsigned char *str) {
    static struct TlsBuffer0 Static = {StrCheck0,StrReset,StrClose};
    BStr *r;
    rPush(r);
    r->TlsBuffer0.Static = &Static;
    r->TlsBuffer0.p = r->s = str;
    return &r->TlsBuffer0;
}

/*_____________________________________________________________________________
 |
 | A range, copied on the heap with a terminator: it may hold data 0.
 |_____________________________________________________________________________
*/

typedef struct {
    TlsBuffer0 TlsBuffer0;
    unsigned char *s,*e;
    int Open;
} BBuf;
static unsigned char *BufCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BBuf,TlsBuffer0);
    if (that->Open && pos!=EOB && pos>=that->s && pos<that->e) {   /* a data 0 */
        that->TlsBuffer0.p = pos;
        return pos;
    }
    that->TlsBuffer0.p = EOB;
    return EOB;
}
static unsigned char *BufReset(TlsBuffer0 *this) {
    TlsGetThat(BBuf,TlsBuffer0);
    that->Open = 1;
    return that->TlsBuffer0.p = that->s;
}
static void BufClose(TlsBuffer0 *this) {
    TlsGetThat(BBuf,TlsBuffer0);
    that->Open = 0;
    that->TlsBuffer0.p = EOB;
}
TlsBuffer0 *TlsBufferBuffer(char *b,char *e) {
    static struct TlsBuffer0 Static = {BufCheck0,BufReset,BufClose};
    BBuf *r;
    unsigned char *c;
    size_t n = e>b ? (size_t)(e-b) : 0,sz = WordRound(n+1);
    rPush(r);
    rnPush(c,sz);
    if (n) memcpy(c,b,n);
    c[n] = 0;
    r->TlsBuffer0.Static = &Static;
    r->TlsBuffer0.p = r->s = c;
    r->e = c+n;
    r->Open = 1;
    return &r->TlsBuffer0;
}

/*_____________________________________________________________________________
 |
 | A file, by blocks. blk[n] is the sentinel; a short read is the end of the
 | file, which is closed at once.
 |_____________________________________________________________________________
*/

/* Small on purpose: past 512-1024 bytes a bigger block buys little, while a lazy
 * stack of buffers (cat of cats of files) stays in cache if each one is tight. */
#ifndef FileBlock
#define FileBlock 1024
#endif
#define FileBlockSz WordRound(FileBlock+1)

typedef struct {
    TlsBuffer0 TlsBuffer0;
    FILE *f;
    unsigned char *blk;
    unsigned int n;                         /* bytes in blk */
    char *name;
} BFile;
static unsigned int FileFill(BFile *F) {
    size_t n = fread(F->blk,1,FileBlock,F->f);
    F->blk[n] = 0;
    F->n = (unsigned int)n;
    if (n<FileBlock) { fclose(F->f); F->f = 0; }
    return F->n;
}
static unsigned char *FileCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BFile,TlsBuffer0);
    if (pos==EOB) return EOB;
    if (pos>=that->blk && pos<that->blk+that->n) {      /* a data 0 */
        that->TlsBuffer0.p = pos;
        return pos;
    }
    if (that->f && FileFill(that)) return that->TlsBuffer0.p = that->blk;
    that->TlsBuffer0.p = EOB;
    return EOB;
}
static void FileClose(TlsBuffer0 *this) {
    TlsGetThat(BFile,TlsBuffer0);
    if (that->f) { fclose(that->f); that->f = 0; }
    that->n = 0;
    that->TlsBuffer0.p = EOB;
}
static unsigned char *FileReset(TlsBuffer0 *this) {
    TlsGetThat(BFile,TlsBuffer0);
    FileClose(this);
    that->f = fopen(that->name,"rb");
    if (!that->f || !FileFill(that)) {
        FileClose(this);
        return EOB;
    }
    return that->TlsBuffer0.p = that->blk;
}
static void FileHook(void *arg) {                       /* the layer is popped */
    BFile *F = arg;
    if (F->f) { fclose(F->f); F->f = 0; }
}
TlsBuffer0 *TlsFileBuffer(char *FileName) {
    static struct TlsBuffer0 Static = {FileCheck0,FileReset,FileClose};
    BFile *r;
    char *nm;
    size_t len = strlen(FileName)+1,nsz = WordRound(len);
    FILE *f = fopen(FileName,"rb");
    if (!f) return 0;
    rPush(r);
    rnPush(nm,nsz);
    memcpy(nm,FileName,len);
    rnPush(r->blk,FileBlockSz);
    r->TlsBuffer0.Static = &Static;
    r->f = f;
    r->name = nm;
    r->n = 0;
    TlsHeapHook(FileHook,r);                            /* after the block: released before it */
    if (FileFill(r)) r->TlsBuffer0.p = r->blk;
    else { FileClose(&r->TlsBuffer0); }
    return &r->TlsBuffer0;
}

/*_____________________________________________________________________________
 |
 | Concatenation. The head is the current piece's own: p is that of the piece
 | the caller is reading in. When a piece says Eob, the next one takes over;
 | an empty piece is just one whose first byte is a 0.
 |_____________________________________________________________________________
*/

typedef struct {
    TlsBuffer0 TlsBuffer0;
    TlsBuffer0 **a;
    int n,i;
} BCat;
/* The caller is at pos, in piece i: settles on the next byte to read. */
static unsigned char *CatStep(BCat *c,unsigned char *pos) {
    for (;;) {
        TlsBuffer0 *s;
        unsigned char *r;
        if (c->i>=c->n) { c->TlsBuffer0.p = EOB; return EOB; }
        if (*pos) { c->TlsBuffer0.p = pos; return pos; }
        s = c->a[c->i];
        r = x_(s,Check0,pos);
        if (r==EOB) {                   /* first: pos itself may be the Eob of an over piece */
            if (++c->i<c->n) pos = c->a[c->i]->p;
            continue;
        }
        if (r==pos) { c->TlsBuffer0.p = pos; return pos; }      /* a data 0 */
        pos = r;
    }
}
static unsigned char *CatCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BCat,TlsBuffer0);
    if (pos==EOB) return EOB;
    return CatStep(that,pos);
}
static unsigned char *CatReset(TlsBuffer0 *this) {
    TlsGetThat(BCat,TlsBuffer0);
    unsigned char *first = EOB;
    int i;
    for (i=that->n-1;i>=0;i--) first = x_(that->a[i],Reset);
    that->i = 0;
    if (!that->n) return that->TlsBuffer0.p = EOB;
    return CatStep(that,first);
}
static void CatClose(TlsBuffer0 *this) {
    TlsGetThat(BCat,TlsBuffer0);
    int i;
    for (i=0;i<that->n;i++) x_(that->a[i],Close);
    that->i = that->n;
    that->TlsBuffer0.p = EOB;
}
/* a is already on the heap. */
static TlsBuffer0 *CatMake(TlsBuffer0 **a,int n) {
    static struct TlsBuffer0 Static = {CatCheck0,CatReset,CatClose};
    BCat *r;
    rPush(r);
    r->TlsBuffer0.Static = &Static;
    r->a = a;
    r->n = n;
    r->i = 0;
    r->TlsBuffer0.p = n ? CatStep(r,a[0]->p) : EOB;
    return &r->TlsBuffer0;
}
TlsBuffer0 *TlsBufferCat(TlsBuffer0 **a,int n) {
    TlsBuffer0 **c;
    int i,cnt = n>0 ? n : 1;                /* not inline: rnPush does not parenthesize its count */
    rnPush(c,cnt);
    for (i=0;i<n;i++) c[i] = a[i];
    return CatMake(c,n);
}
TlsBuffer0 *TlsBufferCat0(TlsBuffer0 *a,...) {
    va_list ap,aq;
    TlsBuffer0 **arr;
    int n = 0,i;
    if (!a) return TlsBufferCat(0,0);
    va_start(ap,a);
    va_copy(aq,ap);
    n = 1;
    while (va_arg(ap,TlsBuffer0 *)) n++;
    va_end(ap);
    rnPush(arr,n);
    arr[0] = a;
    for (i=1;i<n;i++) arr[i] = va_arg(aq,TlsBuffer0 *);
    va_end(aq);
    return CatMake(arr,n);
}

/*_____________________________________________________________________________
 |
 | Take and Skip. Both start on an empty piece (Zero0) and do their work at
 | their first Check0, from the position Src has then.
 |_____________________________________________________________________________
*/

static unsigned char Zero0[8];

#ifndef TakeBlock
#define TakeBlock 256
#endif

typedef struct {
    TlsBuffer0 TlsBuffer0;
    TlsBuffer0 *Src;
    unsigned char *cur;                 /* where we are in Src, once started */
    unsigned char *blk;
    unsigned int n,Left,Fill,B;
    int Started;
} BTake;

static unsigned int TakeFill(BTake *t) {
    unsigned int k = 0,want = t->Left<t->B ? t->Left : t->B;
    unsigned char *cur = t->cur;
    int over = 0;
    while (k<want) {
        if (*cur) {
            unsigned int room = want-k,m;
            unsigned char *z = memchr(cur,0,room);
            m = z ? (unsigned int)(z-cur) : room;
            memcpy(t->blk+k,cur,m);
            k += m;
            cur += m;
        } else {
            unsigned char *r = x_(t->Src,Check0,cur);
            if (r==EOB) { over = 1; cur = EOB; break; }
            if (r==cur) { t->blk[k++] = 0; cur++; }                 /* a data 0 */
            else cur = r;
        }
    }
    t->blk[k] = 0;
    t->Fill = k;
    t->cur = cur;
    t->Left = over ? 0 : t->Left-k;
    if (!t->Left) t->Src->p = cur;                                  /* hand over */
    return k;
}
static unsigned char *TakeCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BTake,TlsBuffer0);
    if (pos==EOB) return EOB;
    if (!that->Started) { that->Started = 1; that->cur = that->Src->p; }
    else if (pos>=that->blk && pos<that->blk+that->Fill) {          /* a data 0 */
        that->TlsBuffer0.p = pos;
        return pos;
    }
    if (that->Left && TakeFill(that)) return that->TlsBuffer0.p = that->blk;
    that->Fill = 0;
    that->TlsBuffer0.p = EOB;
    return EOB;
}
static unsigned char *TakeReset(TlsBuffer0 *this) {
    TlsGetThat(BTake,TlsBuffer0);
    x_(that->Src,Reset);
    that->Started = 0;
    that->Left = that->n;
    that->Fill = 0;
    return that->TlsBuffer0.p = Zero0;
}
static void TakeClose(TlsBuffer0 *this) {
    TlsGetThat(BTake,TlsBuffer0);
    x_(that->Src,Close);
    that->Left = that->Fill = 0;
    that->TlsBuffer0.p = EOB;
}
TlsBuffer0 *TlsBufferTake(TlsBuffer0 *Src,int n) {
    static struct TlsBuffer0 Static = {TakeCheck0,TakeReset,TakeClose};
    BTake *r;
    unsigned int nn = n>0 ? (unsigned int)n : 0;
    unsigned int B = nn<TakeBlock ? nn : TakeBlock,sz = WordRound(B+1);
    rPush(r);
    rnPush(r->blk,sz);
    r->TlsBuffer0.Static = &Static;
    r->TlsBuffer0.p = Zero0;
    r->Src = Src ? Src : TlsBuffer0Null;
    r->n = r->Left = nn;
    r->B = B;
    r->Fill = 0;
    r->Started = 0;
    return &r->TlsBuffer0;
}

typedef struct {
    TlsBuffer0 TlsBuffer0;
    TlsBuffer0 *Src;
    unsigned int n,Left;
    int Started;
} BSkip;
static unsigned char *SkipCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BSkip,TlsBuffer0);
    if (pos==EOB) return EOB;
    if (!that->Started) {
        unsigned char *cur = that->Src->p;
        that->Started = 1;
        while (that->Left) {
            if (*cur) {
                unsigned int k = (unsigned int)strlen((char *)cur);
                if (k<=that->Left) { that->Left -= k; cur += k; }
                else { cur += that->Left; that->Left = 0; }
            } else {
                unsigned char *r = x_(that->Src,Check0,cur);
                if (r==EOB) { that->Left = 0; that->TlsBuffer0.p = EOB; return EOB; }
                if (r==cur) { that->Left--; cur++; }                /* a data 0 */
                else cur = r;
            }
        }
        that->Src->p = cur;
        return that->TlsBuffer0.p = cur;
    }
    return that->TlsBuffer0.p = x_(that->Src,Check0,pos);
}
static unsigned char *SkipReset(TlsBuffer0 *this) {
    TlsGetThat(BSkip,TlsBuffer0);
    x_(that->Src,Reset);
    that->Started = 0;
    that->Left = that->n;
    return that->TlsBuffer0.p = Zero0;
}
static void SkipClose(TlsBuffer0 *this) {
    TlsGetThat(BSkip,TlsBuffer0);
    x_(that->Src,Close);
    that->TlsBuffer0.p = EOB;
}
TlsBuffer0 *TlsBufferSkip(TlsBuffer0 *Src,int n) {
    static struct TlsBuffer0 Static = {SkipCheck0,SkipReset,SkipClose};
    BSkip *r;
    rPush(r);
    r->TlsBuffer0.Static = &Static;
    r->TlsBuffer0.p = Zero0;
    r->Src = Src ? Src : TlsBuffer0Null;
    r->n = r->Left = n>0 ? (unsigned int)n : 0;
    r->Started = 0;
    return &r->TlsBuffer0;
}

TlsBuffer0 *TlsBufferReplace(TlsBuffer0 *Base,TlsBuffer0 *Edit,int b,int e) {
    if (b<0) b = 0;
    if (e<b) e = b;
    if (!Base) Base = TlsBuffer0Null;
    if (!Edit) Edit = TlsBuffer0Null;
    return TlsBufferCat0(TlsBufferTake(Base,b),Edit,TlsBufferSkip(Base,e-b),(TlsBuffer0 *)0);
}

/*_____________________________________________________________________________
 |
 | Macros, see Tools_Buffer.h. The definition keeps the text and a list of
 | substitutions kept sorted by (b,e), after the equal ones. Generate cuts a
 | copy of the text in literal pieces, each followed by its sentinel, in one
 | block; the pieces and the holes make an array of items. Apply makes a reader
 | that walks that array: a literal is read in place (nothing is copied), a hole
 | is read from its Var.
 |_____________________________________________________________________________
*/

typedef struct {
    unsigned char *s;                   /* a literal: len bytes then a 0 */
    int len;
    int Var;                            /* -1 for a literal, else the parameter */
    int First;                          /* the first use of that parameter */
} MItem;

typedef struct {
    TlsBuffMacro TlsBuffMacro;
    MItem *it;
    int n,np;
} BMacro;

typedef struct {
    TlsBuffer0 TlsBuffer0;
    BMacro *M;
    TlsBuffer0 **Var;
    int k;                              /* the current item */
    int Reused;                         /* Reset was called: every Var is to be reset */
} BApply;

/* Settles on the next byte to read. fresh: we just came to item k (pos is not
 * a 0 to look at); else pos is the 0 the caller read, in item k. */
static unsigned char *ApplyStep(BApply *m,unsigned char *pos,int fresh) {
    for (;;) {
        MItem *it;
        if (m->k>=m->M->n) { m->TlsBuffer0.p = EOB; return EOB; }
        it = &m->M->it[m->k];
        if (it->Var<0) {
            if (fresh) return m->TlsBuffer0.p = it->s;
            if (pos>=it->s && pos<it->s+it->len) return m->TlsBuffer0.p = pos;     /* a data 0 */
            m->k++;
            fresh = 1;
        } else {
            TlsBuffer0 *v = m->Var[it->Var];
            unsigned char *r;
            if (fresh) {
                pos = (!it->First || m->Reused) ? x_(v,Reset) : v->p;
                fresh = 0;
                if (*pos) return m->TlsBuffer0.p = pos;
            }
            r = x_(v,Check0,pos);
            if (r==EOB) { m->k++; fresh = 1; continue; }
            return m->TlsBuffer0.p = r;                                           /* data 0, or moved on */
        }
    }
}
static unsigned char *ApplyCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BApply,TlsBuffer0);
    if (pos==EOB) return EOB;
    return ApplyStep(that,pos,0);
}
static unsigned char *ApplyReset(TlsBuffer0 *this) {
    TlsGetThat(BApply,TlsBuffer0);
    that->k = 0;
    that->Reused = 1;
    return ApplyStep(that,0,1);
}
static void ApplyClose(TlsBuffer0 *this) {
    TlsGetThat(BApply,TlsBuffer0);
    int i;
    for (i=0;i<that->M->np;i++) x_(that->Var[i],Close);
    that->k = that->M->n;
    that->TlsBuffer0.p = EOB;
}
static int MacParamNumber(TlsBuffMacro *this) {
    TlsGetThat(BMacro,TlsBuffMacro);
    return that->np;
}
static TlsBuffer0 *MacApply(TlsBuffMacro *this,TlsBuffer0 **Var) {
    static struct TlsBuffer0 Static = {ApplyCheck0,ApplyReset,ApplyClose};
    TlsGetThat(BMacro,TlsBuffMacro);
    BApply *r;
    TlsBuffer0 **v;
    int i,cnt = that->np>0 ? that->np : 1;
    rPush(r);
    rnPush(v,cnt);
    for (i=0;i<that->np;i++) v[i] = (Var && Var[i]) ? Var[i] : TlsBuffer0Null;
    r->TlsBuffer0.Static = &Static;
    r->M = that;
    r->Var = v;
    r->k = 0;
    r->Reused = 0;
    ApplyStep(r,0,1);
    return &r->TlsBuffer0;
}

typedef struct Sub Sub;
struct Sub { Sub *n; int b,e,Param; };
typedef struct {
    TlsBuffMacroDef TlsBuffMacroDef;
    unsigned char *txt;
    int len,ns,np;
    Sub *l;                             /* sorted by (b,e), stable */
} BDef;

static int DefAdd(TlsBuffMacroDef *this,int b,int e,int Param) {
    TlsGetThat(BDef,TlsBuffMacroDef);
    Sub *s,*q,**w;
    if (b<0 || e<b || e>that->len || Param<0) return -1;
    for (q=that->l;q;q=q->n) {
        int bad;
        if (b==e) bad = q->b<b && b<q->e;                       /* an insert inside a range */
        else if (q->b==q->e) bad = b<q->b && q->b<e;
        else bad = b<q->e && q->b<e;
        if (bad) return -1;
    }
    rPush(s);
    s->b = b; s->e = e; s->Param = Param;
    for (w=&that->l;*w && ((*w)->b<b || ((*w)->b==b && (*w)->e<=e));w=&(*w)->n);
    s->n = *w;
    *w = s;
    that->ns++;
    if (Param>=that->np) that->np = Param+1;
    return 0;
}
static TlsBuffMacro *DefGenerate(TlsBuffMacroDef *this) {
    static struct TlsBuffMacro Static = {MacParamNumber,MacApply};
    TlsGetThat(BDef,TlsBuffMacroDef);
    BMacro *r;
    MItem *it;
    unsigned char *tx;
    Sub *q;
    int pos,n = 0,nlit = 0,bytes = 0,i,j,cnt;
    size_t sz;
    /* measure: the gaps between the holes are the literals */
    for (pos=0,q=that->l;;q=q->n) {
        int to = q ? q->b : that->len;
        if (to>pos) { nlit++; bytes += to-pos; n++; }
        if (!q) break;
        n++;
        pos = q->e;
    }
    cnt = n>0 ? n : 1;
    sz = WordRound((size_t)bytes+(size_t)nlit);
    rPush(r);
    rnPush(it,cnt);
    rnPush(tx,sz);
    for (i=0,pos=0,q=that->l;;q=q->n) {
        int to = q ? q->b : that->len;
        if (to>pos) {
            memcpy(tx,that->txt+pos,(size_t)(to-pos));
            tx[to-pos] = 0;
            it[i].s = tx; it[i].len = to-pos; it[i].Var = -1; it[i].First = 0;
            tx += to-pos+1;
            i++;
        }
        if (!q) break;
        it[i].s = 0; it[i].len = 0; it[i].Var = q->Param; it[i].First = 1;
        for (j=0;j<i;j++) if (it[j].Var==q->Param) it[i].First = 0;
        i++;
        pos = q->e;
    }
    r->TlsBuffMacro.Static = &Static;
    r->it = it;
    r->n = n;
    r->np = that->np;
    return &r->TlsBuffMacro;
}
/* Reads the whole of b, from its start: the number of bytes, copied in dst if not NULL. */
static int Drain(TlsBuffer0 *b,unsigned char *dst) {
    unsigned char *p = x_(b,Reset);
    int n = 0;
    for (;;) {
        if (*p) {
            int k = (int)strlen((char *)p);
            if (dst) memcpy(dst+n,p,(size_t)k);
            n += k;
            p += k;
        } else {
            unsigned char *r = x_(b,Check0,p);
            if (r==EOB) break;
            if (r==p) { if (dst) dst[n] = 0; n++; p++; }            /* a data 0 */
            else p = r;
        }
    }
    return n;
}
TlsBuffMacroDef *TlsBuffMacroDefNew(TlsBuffer0 *BaseText) {
    static struct TlsBuffMacroDef Static = {DefAdd,DefGenerate};
    BDef *r;
    unsigned char *t;
    int len;
    size_t sz;
    if (!BaseText) BaseText = TlsBuffer0Null;
    len = Drain(BaseText,0);
    sz = WordRound((size_t)len+1);
    rPush(r);
    rnPush(t,sz);
    if (Drain(BaseText,t)!=len) len = 0;                            /* the source changed under us */
    t[len] = 0;
    r->TlsBuffMacroDef.Static = &Static;
    r->txt = t;
    r->len = len;
    r->ns = r->np = 0;
    r->l = 0;
    return &r->TlsBuffMacroDef;
}
