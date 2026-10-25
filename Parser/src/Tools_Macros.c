/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <string.h>
#include <Tools_Macros.h>

typedef struct {
    TlsMacroLib TlsMacroLib;
    TlsDict *d;
    TlsHeap *H,*S;                      /* H keeps the macros, S is the room of a definition at work */
    TlsIdMap *map;
    TlsOutBuf *ob,*ob2;                 /* two: the body is read while the line is, and Txt may be in either */
    int VaArgs;
} BLib;

#define MaxParams 32

/* the next token of the line: at its end there is none, and the end is not read (the Pp needs it) */
static int NextOnLine(TlsTok *t) {
    int b = x_(t,Peek,1);
    return b && b!='\n' ? x_(t,Next) : TokNil;
}

static int LibDefine(TlsMacroLib *this,TlsTok *t) {
    TlsGetThat(BLib,TlsMacroLib);
    int ids[MaxParams],n = -1,var = 0,ok = 1,c,id,i,len,nn,dn,comma = 0;
    unsigned int strm = 0,raw = 0;
    unsigned char *s;
    TlsStackNod *l;
    TlsMacro *m = 0;
    c = NextOnLine(t);
    if (c!=TokLabel) { if (c!=TokNl) x_(t,Rest,NULL); return 0; }
    id = (int)t->Val;
    if (x_(t,Peek,0)=='(') {
        x_(t,Next);
        n = 0;
        for (;ok;) {
            c = NextOnLine(t);
            if (c==')') { if (comma) ok = 0; break; }   /* NAME() */
            if (n>=MaxParams) { ok = 0; break; }
            if (c==TokLabel) ids[n++] = (int)t->Val;
            else if (c==TokOp(Ellipsis)) { ids[n++] = that->VaArgs; var = 1; }
            else { ok = 0; break; }
            c = NextOnLine(t);
            if (c==')') break;
            if (c!=',' || var) ok = 0;
            comma = 1;
        }
    }
    if (!ok) { if (t->Code!=TokNl) x_(t,Rest,NULL); return 0; }
    s = x_(t,Rest,&len);
    while (len>0 && (s[len-1]==' ' || s[len-1]=='\t')) len--;
    nn = n<0 ? 0 : n;                                   /* the parameters: 0..n-1, then the # ones, then "nothing" */
    dn = 2*nn;
    TlsHeapIn(that->S)
        TlsBuffMacroDef *def;
        TlsTok *b;
        int prevEnd = -1,hash = -1,paste = -1,lastk = -1,after = 0;
        l = TlsHeapLayer();
        def = TlsBuffMacroDefNew(TlsBufferBuffer((char *)s,(char *)s+len));
        b = TlsTokNew(TlsBufferBuffer((char *)s,(char *)s+len),that->d,that->ob2);
        for (;;) {
            int k = -1;
            c = x_(b,Next);
            if (c==TokNil) break;
            if (c==TokLabel) for (i=0;i<n;i++) if (ids[i]==(int)b->Val) { k = i; break; }
            if (paste>=0 && c!=TokOp(Paste)) { x_(def,AddSubstitution,paste,(int)b->Pos,dn); paste = -1; }
            if (c==TokOp(Paste)) { if (prevEnd>=0) paste = prevEnd; if (lastk>=0) raw |= 1u<<lastk; after = 1; continue; }
            if (after && k>=0) raw |= 1u<<k;                    /* an operand of ## stays as written */
            after = 0;
            if (hash>=0) {                              /* # x */
                int h = hash;
                hash = -1;
                if (k>=0) { x_(def,AddSubstitution,h,(int)b->End,n+k); strm |= 1u<<k; prevEnd = (int)b->End; lastk = -1; continue; }
            }
            if (n>=0 && c=='#') { hash = (int)b->Pos; prevEnd = (int)b->End; lastk = -1; continue; }
            if (k>=0) x_(def,AddSubstitution,(int)b->Pos,(int)b->End,k);
            prevEnd = (int)b->End;
            lastk = k;
        }
        TlsHeapIn(that->H)
            rPush(m);
            m->Body = x_(def,Generate);
        TlsHeapOut
        TlsHeapLayerPop(l);
    TlsHeapOut
    m->Params = n; m->Variadic = var; m->Busy = 0; m->Str = strm; m->Raw = raw;
    m->Np = x_(m->Body,ParamNumber);
    x_(that->map,Set,id,m);
    return id;
}
static int LibDefineStr(TlsMacroLib *this,char const *s) {
    TlsGetThat(BLib,TlsMacroLib);
    TlsStackNod *l;
    int r;
    TlsHeapIn(that->S)
        l = TlsHeapLayer();
        r = LibDefine(this,TlsTokNew(TlsStringBuffer((unsigned char *)s),that->d,that->ob));
        TlsHeapLayerPop(l);
    TlsHeapOut
    return r;
}
static TlsMacro *LibGet(TlsMacroLib *this,int id) {
    TlsGetThat(BLib,TlsMacroLib);
    return x_(that->map,Get,id);
}
static int LibUndef(TlsMacroLib *this,int id) {
    TlsGetThat(BLib,TlsMacroLib);
    int was = x_(that->map,Get,id)!=0;
    x_(that->map,Set,id,0);
    return was;
}

TlsMacroLib *TlsMacroLibNew(TlsDict *D,TlsHeap *Where) {
    static struct TlsMacroLib Static = {LibDefine,LibDefineStr,LibGet,LibUndef};
    BLib *r;
    rPush(r);
    r->TlsMacroLib.Static = &Static;
    r->d = D;
    r->H = Where ? Where : TlsHeapFork();
    r->S = TlsHeapFork();
    r->map = TlsIdMapNew(r->H);
    r->ob = TlsOutBufNew(NULL);
    r->ob2 = TlsOutBufNew(NULL);
    r->VaArgs = x_(D,Id,(unsigned char *)"__VA_ARGS__",11);
    return &r->TlsMacroLib;
}
