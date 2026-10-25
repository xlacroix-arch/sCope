/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <stdlib.h>
#include <string.h>
#include <Tools_Pp.h>
#include <Tools_Expr.h>

#define MaxArgs 32
#define MaxFiles 16                     /* #include nested */

typedef struct Frame Frame;
struct Frame {
    Frame *up;
    TlsTok *t;
    TlsMacro *mac;                      /* NULL: the text */
    int first,spc0;                     /* the first token takes the blank of the call */
    int bol;                            /* the next token starts a line (a # is a directive) */
    struct Cond *condBase;              /* the conditionals open when this text began */
    char const *name;                   /* the file of this text (NULL: it has none) */
    TlsCpp0Hook *map;                   /* its LineMap */
    TlsStackNod *layer;                 /* a file that #include opened: its layer of P, popped when it is read */
    TlsBuffer0 *src;                    /* what is closed when the text is read (NULL: the caller's) */
    int sdelta;                         /* the #line of the including text, put back when this one is read */
    char const *spname;
};
typedef struct Cond Cond;
struct Cond {                           /* an #ifdef ... that is open */
    Cond *up;
    int taking,seen,parent,elsed;       /* its group is read; a group was; the text around is read; #else came */
    unsigned int line;
};
typedef struct {
    TlsPp TlsPp;
    TlsPpCtx *ctx;
    TlsDict *d;
    TlsMacroLib *lib;
    char const *name;
    TlsCpp0Hook *map;
    TlsHeap *S;                         /* the room of the expansions */
    TlsOutBuf *ob;                      /* for the tokenizers */
    TlsOutBuf *ab;                      /* for the arguments: the tokenizers use ob while they are read */
    TlsStackNod *layer;
    TlsHeap *P;                         /* the files that #include opened, one layer each */
    TlsHeap *C;                         /* what lives as long as the Pp: the conditionals */
    int nfile;
    TlsCpp0Hook *tmap;                  /* where the last token comes from: its LineMap and its line */
    unsigned int tline;
    Frame *top;
    Cond *cond,*freec;
    int depth,flags,active;             /* active: the text is read (not in an #ifdef group that is off) */
    unsigned long iPos,iEnd;            /* the call that is being expanded */
    unsigned int iLine;
    int delta;                          /* #line: the number of a line is its original one + delta */
    char const *pname;                  /* #line "x": the name the messages and __FILE__ give (NULL: name) */
    long fixed;                         /* >= 0: the line of everything here (the text of an argument that is expanded) */
    int tdelta;
    char lbuf[24],fbuf[520];            /* the text of the last __LINE__ and __FILE__ */
    char const *fbufFor;
} BPp;

static unsigned char const NoText[1] = {0};

static int LabelStart(int b) { return b>='a' && b<='z' ? 1 : b>='A' && b<='Z' ? 1 : b=='_' || b=='$' || b>=128; }

static char const *PName(BPp *p) { return p->pname ? p->pname : p->name; }
/* the line the messages and __LINE__ give: the original one, as #line says */
static unsigned int PLine(BPp *p,unsigned int line) {
    if (p->fixed>=0) return (unsigned int)p->fixed;
    if (p->map) line = TlsLineMapOrig(p->map,line);
    return (unsigned int)((int)line+p->delta);
}
static void Say(BPp *p,int kind,unsigned int line,char const *msg) {
    x_(p->ctx,Report,kind,PName(p),PLine(p,line),msg);
}
static void Err(BPp *p,unsigned int line,char *msg) { Say(p,PpMsgError,line,msg); }

static void PushCond(BPp *p,int take,unsigned int line) {
    Cond *c = p->freec;
    if (c) p->freec = c->up;
    else TlsHeapIn(p->C)
        rPush(c);
    TlsHeapOut
    c->up = p->cond; c->parent = p->active; c->taking = take; c->seen = take; c->elsed = 0; c->line = line;
    p->cond = c;
    p->active = take;
}
static void PopCond(BPp *p) {
    Cond *c = p->cond;
    p->cond = c->up;
    c->up = p->freec; p->freec = c;
    p->active = !p->cond || p->cond->taking;
}

static void PopFrame(BPp *p) {
    Frame *f = p->top;
    p->top = f->up;
    f->mac->Busy = 0;
    if (--p->depth==0)
        TlsHeapIn(p->S)
            TlsHeapLayerPop(p->layer);
        TlsHeapOut
}

/* the arguments of a call, from the tokens after the ( : text, cut at the commas */
static int Args(BPp *p,TlsTok *t,TlsMacro *m,unsigned char **ab,int *an,unsigned int line) {
    int k = 0,depth = 0,first = 1,sp = 0,c;
    x_(p->ab,Reset);
    for (;;) {
        c = x_(t,Next);
        if (c==TokNil) { Err(p,line,"macro call not closed"); break; }
        if (c=='(') depth++;
        else if (c==')') { if (!depth) break; depth--; }
        else if (c==',' && !depth && !(m->Variadic && k==m->Params-1)) {
            if (k<MaxArgs) ab[k] = x_(p->ab,Take,&an[k]);
            k++; x_(p->ab,Reset); first = 1; sp = 0;
            continue;
        }
        else if (c==TokNl) { sp = 1; continue; }
        if (!first && (t->Spc || sp)) TlsOutPut(p->ab,' ');
        TlsOutWrite(p->ab,t->Txt,(unsigned int)t->Len);
        first = 0; sp = 0;
    }
    if (k<MaxArgs) ab[k] = x_(p->ab,Take,&an[k]);
    return k+1;
}

/* #x : the text in a string */
static unsigned char *Quote(BPp *p,unsigned char *s,int n,int *len) {
    int i;
    x_(p->ab,Reset);
    TlsOutPut(p->ab,'"');
    for (i=0;i<n;i++) {
        if (s[i]=='"' || s[i]=='\\') TlsOutPut(p->ab,'\\');
        TlsOutPut(p->ab,s[i]);
    }
    TlsOutPut(p->ab,'"');
    return x_(p->ab,Take,len);
}

static int IsWord(int b) { return LabelStart(b) || (b>='0' && b<='9'); }

/* an argument with its macros expanded, as text (the same text if there is none) */
static unsigned char *PreExpand(BPp *p,unsigned char *s,int n,int *len,unsigned int line) {
    TlsTok *t = TlsTokNew(TlsBufferBuffer((char *)s,(char *)s+n),p->d,p->ab);
    TlsPp *q;
    int c,hit = 0,first = 1,pl = 0;
    *len = n;
    while (!hit && (c = x_(t,Next))!=TokNil)
        if (c==TokLabel) {
            TlsMacro *m = x_(p->lib,Get,(int)t->Val);
            int w = m ? 0 : x_(p->ctx,Word,(int)t->Val);
            hit = m ? !m->Busy : (w==PpWord(LineMac) || w==PpWord(FileMac));
        }
    if (!hit) return s;
    q = TlsPpNew(p->ctx,TlsBufferBuffer((char *)s,(char *)s+n),NULL,NULL,0);
    ((BPp *)q)->top->bol = 0;                           /* a # in an argument is not a directive */
    ((BPp *)q)->name = p->name;
    ((BPp *)q)->pname = PName(p);
    ((BPp *)q)->fixed = (long)PLine(p,line);            /* __LINE__ in an argument is the line of the call */
    x_(p->ab,Reset);
    while ((c = x_(q,Next))!=TokNil) {
        if (!first && (q->Spc || IsWord(pl)==IsWord(q->Txt[0]))) TlsOutPut(p->ab,' ');
        TlsOutWrite(p->ab,q->Txt,(unsigned int)q->Len);
        pl = q->Txt[q->Len-1];
        first = 0;
    }
    return x_(p->ab,Take,len);
}

/* t is on the name of macro m: expand it, or leave it (not a call) */
static int Expand(BPp *p,Frame *cur,TlsMacro *m) {
    TlsTok *t = cur->t;
    unsigned char *ab[MaxArgs];
    int an[MaxArgs],na = 0,np,i,spc0 = cur->first ? cur->spc0 : t->Spc;
    unsigned long pos = t->Pos;
    unsigned int line = p->depth ? p->iLine : t->Line;
    TlsBuffer0 **Var = 0;
    TlsBuffer0 *r;
    Frame *f;
    if (m->Params>=0 && x_(t,Peek,1)!='(') return 0;
    TlsHeapIn(p->S)
        if (!p->depth) p->layer = TlsHeapLayer();
        if (m->Params>=0) {
            x_(t,Next);                                 /* the ( */
            na = Args(p,t,m,ab,an,line);
            if (m->Params==0 && na==1 && !an[0]) na = 0;
            if (na>m->Params) Err(p,line,"too many arguments");
            else if (na<m->Params && !(m->Variadic && na==m->Params-1)) Err(p,line,"too few arguments");
        }
        np = m->Np;
        if (np) {
            rnPush(Var,np);
            for (i=0;i<np;i++) Var[i] = 0;
            for (i=0;i<m->Params && i<np;i++) {
                unsigned char *s = i<na && i<MaxArgs ? ab[i] : (unsigned char *)NoText;
                int n = i<na && i<MaxArgs ? an[i] : 0;
                unsigned char *e = s;
                int en = n;
                if (!(m->Raw>>i&1) && n) e = PreExpand(p,s,n,&en,line);
                Var[i] = TlsBufferBuffer((char *)e,(char *)e+en);
                if ((m->Str>>i&1) && m->Params+i<np) {
                    int ql;
                    unsigned char *q = Quote(p,s,n,&ql);
                    Var[m->Params+i] = TlsBufferBuffer((char *)q,(char *)q+ql);
                }
            }
        }
        r = x_(m->Body,Apply,Var);
        rPush(f);
        f->t = TlsTokNew(r,p->d,p->ob);
        f->mac = m; f->first = 1; f->spc0 = spc0;
    TlsHeapOut
    if (!p->depth) { p->iPos = pos; p->iLine = line; p->iEnd = t->End; }
    f->up = p->top;
    p->top = f;
    p->depth++;
    m->Busy = 1;
    return 1;
}


/* #include: the name is the rest of the line, "x" or <x>; a file that opens becomes the text */
static void Include(BPp *p,Frame *f,unsigned int line) {
    TlsTok *t = f->t;
    int n,i = 0,q,e;
    unsigned char *s = x_(t,Rest,&n);
    char nm[512],*path;
    TlsStackNod *l;
    TlsBuffer0 *src;
    Frame *nf;
    while (i<n && s[i]==' ') i++;
    q = i<n ? s[i] : 0;
    if (q!='"' && q!='<') { Say(p,PpMsgError,line,"bad #include"); return; }
    for (e=i+1;e<n && s[e]!=(q=='"' ? '"' : '>');e++) ;
    if (e>=n || e==i+1 || e-i-1>=(int)sizeof nm) { Say(p,PpMsgError,line,e==i+1 ? "empty #include name" : "bad #include"); return; }
    memcpy(nm,s+i+1,(size_t)(e-i-1));
    nm[e-i-1] = 0;
    if (p->nfile>=MaxFiles) { Say(p,PpMsgError,line,"#include nested too deep"); return; }
    TlsHeapIn(p->P)
        l = TlsHeapLayer();
        path = x_(p->ctx,Resolve,nm,q=='"',p->name);
        if (!path) { Say(p,PpMsgError,line,"#include file not found"); TlsHeapLayerPop(l); }
        else if (x_(p->ctx,OnceHas,path)) TlsHeapLayerPop(l);                  /* #pragma once: it was read */
        else if (!(src = TlsFileBuffer(path))) { Say(p,PpMsgError,line,"#include file cannot be read"); TlsHeapLayerPop(l); }
        else {
            TlsCpp0Hook *map = TlsLineMapNew(NULL);
            TlsBuffer0 *flt = TlsCpp0Filter(src,Cpp0Default,map);
            rPush(nf);
            nf->t = TlsTokNew(flt,p->d,p->ob);
            nf->mac = 0; nf->first = nf->spc0 = 0; nf->bol = 1; nf->condBase = p->cond;
            nf->name = path; nf->map = map; nf->layer = l; nf->src = flt;
            nf->sdelta = p->delta; nf->spname = p->pname; p->delta = 0; p->pname = 0;
            x_(p->ctx,Opened,p->name,p->map ? TlsLineMapOrig(p->map,line) : line,path);
            nf->up = p->top;
            p->top = nf;
            p->name = path; p->map = map;
            p->nfile++;
        }
    TlsHeapOut
}
/* the text of a file that #include opened is over */
static void EndFile(BPp *p,Frame *f) {
    TlsStackNod *l = f->layer;
    Frame *up = f->up;
    x_(f->src,Close);
    p->top = up;
    p->name = up->name; p->map = up->map;
    p->delta = f->sdelta; p->pname = f->spname;
    p->nfile--;
    TlsHeapIn(p->P)
        TlsHeapLayerPop(l);
    TlsHeapOut
}


static int IsDef(BPp *p,int id) {
    int w;
    if (x_(p->lib,Get,id)) return 1;
    w = x_(p->ctx,Word,id);
    return w==PpWord(LineMac) || w==PpWord(FileMac);
}

/* #line N or #line N "file": the next line is N (the macros of the line are expanded) */
static void DoLine(BPp *p,TlsTok *t,unsigned int line) {
    int n,c,bad = 0;
    unsigned char *s;
    unsigned long num = 0;
    char *nm = 0;
    TlsStackNod *l;
    s = x_(t,Rest,&n);
    TlsHeapIn(p->S)
        TlsTok *q;
        l = TlsHeapLayer();
        s = PreExpand(p,s,n,&n,line);
        q = TlsTokNew(TlsBufferBuffer((char *)s,(char *)s+n),p->d,NULL);
        if (x_(q,Next)!=TokInt) bad = 1;
        else {
            num = q->Val;
            c = x_(q,Next);
            if (c==TokStr) {
                int i,k = 0;
                if (q->Len<2) bad = 1;
                else {
                    size_t sz = ((size_t)q->Len+sizeof(void *))&~(sizeof(void *)-1);
                    TlsHeapIn(p->C)
                        rnPush(nm,sz);
                    TlsHeapOut
                    for (i=1;i<q->Len-1;i++) {
                        if (q->Txt[i]=='\\' && i+1<q->Len-1) i++;
                        nm[k++] = (char)q->Txt[i];
                    }
                    nm[k] = 0;
                }
                c = x_(q,Next);
            }
            if (c!=TokNil || num>2147483647ul) bad = 1;
        }
        TlsHeapLayerPop(l);
    TlsHeapOut
    if (bad) { Say(p,PpMsgError,line,"bad #line"); return; }
    p->delta = (int)num-(int)(PLine(p,line)-(unsigned int)p->delta)-1;
    if (nm) p->pname = nm;
}

/* the names that are left after the expansion have no value: 0, as in C */
static int NoValue(TlsExprEnv *this,int id,long long *v) { (void)this; (void)id; *v = 0; return 1; }

/* #if, #elif: the rest of the line, with defined X and defined(X) done, its macros expanded,
   then its value. An error is reported, and the value is 0 (the group is off) */
static int EvalIf(BPp *p,TlsTok *t,unsigned int line) {
    static struct TlsExprEnv Vt = {NoValue};
    TlsExprEnv env = {&Vt};
    char const *err = 0;
    unsigned char *s,*x;
    int n,c,r = 0;
    long long v = 0;
    TlsStackNod *l;
    s = x_(t,Rest,&n);
    TlsHeapIn(p->S)
        TlsTok *q,*e;
        l = TlsHeapLayer();
        q = TlsTokNew(TlsBufferBuffer((char *)s,(char *)s+n),p->d,NULL);
        x_(p->ab,Reset);
        while (!err && (c = x_(q,Next))!=TokNil) {
            if (c==TokLabel && x_(p->ctx,Word,(int)q->Val)==PpWord(Defined)) {
                int par = x_(q,Peek,1)=='(',id = 0;
                if (par) x_(q,Next);
                if (LabelStart(x_(q,Peek,1))) { x_(q,Next); id = (int)q->Val; }
                else err = "defined without a name";
                if (!err && par) { if (x_(q,Peek,1)==')') x_(q,Next); else err = "defined( without )"; }
                if (!err) TlsOutPut(p->ab,IsDef(p,id) ? '1' : '0');
            } else TlsOutWrite(p->ab,q->Txt,(unsigned int)q->Len);
            TlsOutPut(p->ab,' ');
        }
        if (!err) {
            x = x_(p->ab,Take,&n);
            x = PreExpand(p,x,n,&n,line);
            e = TlsTokNew(TlsBufferBuffer((char *)x,(char *)x+n),p->d,NULL);
            err = TlsExprEval(e,&env,&v);
            r = v!=0;
        }
        TlsHeapLayerPop(l);
    TlsHeapOut
    if (err) { Say(p,PpMsgError,line,(char *)err); r = 0; }
    return r;
}

/* the # that starts a line was read: t is just after it */
static void Directive(BPp *p,Frame *f) {
    TlsTok *t = f->t;
    unsigned int line = t->Line;
    int w = 0,id = 0,b = x_(t,Peek,1),n;
    char buf[256];
    if (b==0 || b=='\n') return;                                        /* a # alone */
    if (LabelStart(b)) { x_(t,Next); id = (int)t->Val; w = x_(p->ctx,Word,id); }
    else {
        if (p->active) Say(p,PpMsgError,line,"bad directive");
        x_(t,Rest,NULL);
        return;
    }
    switch (w) {
    case PpWord(Define):
        if (p->active) { if (!x_(p->lib,Define,t)) Say(p,PpMsgError,line,"bad #define"); return; }
        break;
    case PpWord(Undef):
        if (p->active) {
            if (LabelStart(x_(t,Peek,1))) { x_(t,Next); x_(p->lib,Undef,(int)t->Val); }
            else Say(p,PpMsgError,line,"bad #undef");
        }
        break;
    case PpWord(Ifdef): case PpWord(Ifndef): {
        int def = 0;
        if (LabelStart(x_(t,Peek,1))) { x_(t,Next); def = IsDef(p,(int)t->Val); }
        else if (p->active) Say(p,PpMsgError,line,w==PpWord(Ifdef) ? "bad #ifdef" : "bad #ifndef");
        PushCond(p,p->active && def==(w==PpWord(Ifdef)),line);
        break; }
    case PpWord(If):
        PushCond(p,p->active && EvalIf(p,t,line),line);                 /* not evaluated in a group that is off */
        break;
    case PpWord(Elif):
        if (!p->cond) Say(p,PpMsgError,line,"#elif without #if");
        else {
            int take = 0;
            if (p->cond->elsed) Say(p,PpMsgError,line,"#elif after #else");
            else if (p->cond->parent && !p->cond->seen) take = EvalIf(p,t,line);
            p->cond->taking = take;
            p->cond->seen |= take;
            p->active = take;
        }
        break;
    case PpWord(Else):
        if (!p->cond) Say(p,PpMsgError,line,"#else without #if");
        else if (p->cond->elsed) Say(p,PpMsgError,line,"#else after #else");
        else {
            p->cond->elsed = 1;
            p->cond->taking = p->cond->parent && !p->cond->seen;
            p->cond->seen |= p->cond->taking;
            p->active = p->cond->taking;
        }
        break;
    case PpWord(Endif):
        if (!p->cond) Say(p,PpMsgError,line,"#endif without #if");
        else PopCond(p);
        break;
    case PpWord(Include):
        if (p->active) Include(p,f,line);
        break;
    case PpWord(Line):
        if (p->active) DoLine(p,t,line);
        break;
    case PpWord(Error): case PpWord(Warning):
        if (p->active) {
            unsigned char *s = x_(t,Rest,&n);
            if (n>(int)sizeof buf-1) n = (int)sizeof buf-1;
            memcpy(buf,s,(size_t)n); buf[n] = 0;
            Say(p,w==PpWord(Error) ? PpMsgError : PpMsgWarning,line,buf);
        }
        break;
    case PpWord(Pragma):
        if (p->active) {
            int id2 = 0;
            if (LabelStart(x_(t,Peek,1))) { x_(t,Next); id2 = (int)t->Val; }
            if (id2 && x_(p->ctx,Word,id2)==PpWord(Once)) { if (p->name) x_(p->ctx,OnceSet,p->name); }
            else x_(p->ctx,Directive,PpWord(Pragma),id2,t,PName(p),PLine(p,line));
        }
        break;
    default:
        if (p->active) {
            if (!x_(p->ctx,Directive,0,id,t,PName(p),PLine(p,line)))
                Say(p,PpMsgError,line,"unknown directive");
        }
    }
    x_(t,Rest,NULL);                                                    /* what is left of the line */
}


static void PpWhere(TlsPp *this,char const **File,unsigned int *Line) {
    TlsGetThat(BPp,TlsPp);
    if (File) *File = this->File;
    if (Line) *Line = that->fixed>=0 ? (unsigned int)that->fixed : (unsigned int)((int)(that->tmap ? TlsLineMapOrig(that->tmap,that->tline) : that->tline)+that->tdelta);
}

static int PpNext(TlsPp *this) {
    TlsGetThat(BPp,TlsPp);
    int nl = 0;
    for (;;) {
        Frame *f = that->top;
        TlsTok *t = f->t;
        unsigned char *tx;
        int c,tl;
        unsigned long tv;
        if (!f->mac && !that->active) {                                 /* an #ifdef group that is off: only its # lines count */
            int b = x_(t,Peek,1);
            if (b && b!='\n' && b!='#') { x_(t,Rest,NULL); continue; }
        }
        c = x_(t,Next);
        if (c==TokNil) {
            if (f->mac) { PopFrame(that); continue; }
            while (that->cond!=f->condBase) { Say(that,PpMsgError,that->cond->line,"conditional not closed"); PopCond(that); }
            if (f->up) { EndFile(that,f); continue; }
            if (f->src) x_(f->src,Close);
            this->Code = TokNil; this->Len = 0; this->Txt = (unsigned char *)NoText; this->Val = 0; this->Exp = 0;
            this->Pos = this->End = t->Pos; this->Line = t->Line; this->Spc = 0;
            return TokNil;
        }
        if (c=='#' && f->bol && !f->mac) { f->bol = 0; Directive(that,f); continue; }
        if (c==TokNl) {
            f->bol = 1;
            if (!(that->flags&PpNl)) { nl = 1; continue; }
        } else f->bol = 0;
        tx = t->Txt; tl = t->Len; tv = t->Val;
        if (c==TokLabel) {
            TlsMacro *m = x_(that->lib,Get,(int)t->Val);
            if (m && !m->Busy && Expand(that,f,m)) { f->first = 0; f->bol = 0; continue; }
            if (!m) {
                int w = x_(that->ctx,Word,(int)t->Val);
                if (w==PpWord(LineMac)) {
                    unsigned int v = PLine(that,that->depth ? that->iLine : t->Line);
                    char *e = that->lbuf+sizeof that->lbuf-1,*b = e;
                    do { *--b = (char)('0'+v%10); v /= 10; } while (v);
                    c = TokInt; tx = (unsigned char *)b; tl = (int)(e-b); tv = (unsigned long)atol(b);
                } else if (w==PpWord(FileMac)) {
                    char const *nm = PName(that);
                    if (!nm) nm = "";
                    if (nm!=that->fbufFor) {
                        char *o = that->fbuf;
                        *o++ = '"';
                        for (;*nm && o<that->fbuf+sizeof that->fbuf-4;nm++) {
                            if (*nm=='"' || *nm=='\\') *o++ = '\\';
                            *o++ = *nm;
                        }
                        *o++ = '"'; *o = 0;
                        that->fbufFor = PName(that);
                    }
                    c = TokStr; tx = (unsigned char *)that->fbuf; tl = (int)strlen(that->fbuf); tv = 0;
                }
            }
        }
        this->Code = c; this->Len = tl; this->Txt = tx; this->Val = tv;
        this->Exp = that->depth;
        if (that->depth) {
            this->Pos = that->iPos; this->End = that->iEnd; this->Line = that->iLine;
            this->Spc = f->first ? f->spc0 : t->Spc;
        } else {
            this->Pos = t->Pos; this->End = t->End; this->Line = t->Line; this->Spc = t->Spc;
        }
        this->File = PName(that); that->tmap = that->map; that->tline = this->Line; that->tdelta = that->delta;
        if (nl) this->Spc = 1;                          /* a line end is a blank */
        f->first = 0;
        return c;
    }
}

TlsPp *TlsPpNew(TlsPpCtx *Ctx,TlsBuffer0 *Src,char const *Name,TlsCpp0Hook *Map,int Flags) {
    static struct TlsPp Static = {PpNext,PpWhere};
    BPp *r;
    Frame *f;
    rPush(r);
    rPush(f);
    r->TlsPp.Static = &Static;
    r->TlsPp.Code = TokNil; r->TlsPp.Exp = 0; r->TlsPp.File = Name;
    r->tmap = Map; r->tline = 0; r->nfile = 0;
    r->ctx = Ctx; r->d = Ctx->Dict; r->lib = Ctx->Macros; r->name = Name; r->map = Map; r->flags = Flags;
    r->S = TlsHeapFork();
    r->P = TlsHeapFork();
    r->C = TlsHeapFork();
    r->cond = r->freec = 0; r->active = 1;
    r->ob = TlsOutBufNew(NULL);
    r->ab = TlsOutBufNew(NULL);
    r->depth = 0;
    r->delta = 0; r->pname = 0; r->fixed = -1; r->tdelta = 0; r->fbufFor = 0;
    f->up = 0; f->mac = 0; f->first = f->spc0 = 0; f->bol = 1; f->condBase = 0; f->name = Name; f->map = Map; f->layer = 0; f->src = 0; f->sdelta = 0; f->spname = 0;
    f->t = TlsTokNew(Src,r->d,r->ob);
    r->top = f;
    if (Name) x_(Ctx,Opened,Name,0,Name);
    return &r->TlsPp;
}

TlsPp *TlsPpNewFile(TlsPpCtx *Ctx,char const *Path,int Flags) {
    size_t n = strlen(Path)+1,sz = (n+sizeof(void *)-1)&~(sizeof(void *)-1);
    TlsBuffer0 *src = TlsFileBuffer((char *)Path),*flt;
    TlsCpp0Hook *map;
    TlsPp *r;
    char *nm;
    if (!src) return 0;
    rnPush(nm,sz);
    memcpy(nm,Path,n);
    map = TlsLineMapNew(NULL);
    flt = TlsCpp0Filter(src,Cpp0Default,map);
    r = TlsPpNew(Ctx,flt,nm,map,Flags);
    ((BPp *)r)->top->src = flt;
    return r;
}
