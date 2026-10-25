/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <string.h>
#include <Tools_Tok.h>

/* class bits */
#define cAlpha 1        /* letters, _ $ and the bytes >= 128 */
#define cDigit 2
#define cDot 8          /* . and ' : inside a number */
#define cSp 16          /* blanks (not the end of line) */
#define cLine 32        /* any byte of a line: all but 0 and \n */
#define cLabel (cAlpha|cDigit)
#define cNum (cLabel|cDot)

static unsigned char Cl[256];
static unsigned char const EndZ[2] = {0,0};

typedef struct {
    TlsTok TlsTok;
    TlsBuffer0 *src;
    TlsDict *dict;
    TlsOutBuf *ob;
    unsigned char *p,*b0;               /* read position (never on a 0, but at the end), start of its piece */
    unsigned long base;                 /* bytes before the piece */
    int end,pend,nl;                    /* nl: the line ended, count it before the next token */
    unsigned char Buf[4];               /* the characters of an operator */
} BTok;

static void ClInit(void) {
    int i;
    if (Cl[1]) return;
    for (i=1;i<256;i++) Cl[i] = cLine;
    Cl['\n'] = 0;
    for (i=0;i<256;i++) if (i>=128 || i=='_' || i=='$' || ((i|32)>='a' && (i|32)<='z')) Cl[i] |= cAlpha;
    for (i='0';i<='9';i++) Cl[i] |= cDigit;
    Cl['.'] |= cDot; Cl['\''] |= cDot;
    Cl[' '] |= cSp; Cl['\t'] |= cSp; Cl['\r'] |= cSp; Cl['\f'] |= cSp; Cl['\v'] |= cSp;
}

#define Off(t) ((t)->base+(unsigned long)((t)->p-(t)->b0))

/* p is on a 0: get the next data, or the end */
static void Fill(BTok *t) {
    while (!*t->p) {
        unsigned char *r;
        if (t->end) return;
        r = x_(t->src,Check0,t->p);
        if (r==TlsBufferEob) {
            t->end = 1;
            t->base += (unsigned long)(t->p-t->b0);
            t->b0 = t->p = (unsigned char *)EndZ;
            return;
        }
        if (r==t->p) { t->p++; continue; }              /* a 0 that is data: a byte, nothing more */
        t->base += (unsigned long)(t->p-t->b0);
        t->b0 = t->p = r;
    }
}
#define Norm(t) ((void)(*(t)->p || (Fill(t),0)))

/* the token ends at q: keep it where it is, or copy it if it spans pieces */
static void Finish(BTok *t,int copy,unsigned char *s,unsigned char *q,int drop) {
    TlsTok *k = &t->TlsTok;
    t->p = q;
    Norm(t);
    if (drop) { k->Txt = (unsigned char *)EndZ; k->Len = 0; }
    else if (copy) {
        TlsOutWrite(t->ob,s,(unsigned int)(q-s));
        if (!t->ob->full) { k->Txt = t->ob->cb; k->Len = (int)(t->ob->p-t->ob->cb); }    /* in the first chunk: as it is */
        else k->Txt = x_(t->ob,Take,&k->Len);
    } else { k->Txt = s; k->Len = (int)(q-s); }
    k->End = Off(t);
}
#define Cross(t,copy,s,q,drop,pre) do {\
    if (!(copy)) { x_((t)->ob,Reset); (copy) = 1; }\
    if (!(drop)) TlsOutWrite((t)->ob,s,(unsigned int)((q)-(s)));\
    (t)->p = (q); Fill(t); (q) = (s) = (t)->p;\
} while (0)

/* a token made of bytes of the classes in mask; num: a sign after e E p P is in it;
 * pre: a byte that came before (0: none); drop: copy nothing */
static void Scan(BTok *t,int mask,int num,int pre,int drop) {
    unsigned char *s = t->p,*q = s;
    int copy = 0,last = 0,f0 = pre ? pre : *s,f1 = pre ? -1 : s[1] ? s[1] : -1;
    if (pre) { x_(t->ob,Reset); TlsOutPut(t->ob,pre); copy = 1; last = pre; }
    for (;;) {
        while (Cl[*q]&mask) q++;
        if (num && (*q=='+' || *q=='-')) {              /* 1e+5 and 0x1p-3, but 0xe+1 is a sum */
            int c = (q>s ? q[-1] : last)|32;
            if ((c=='e' && !(f0=='0' && (f1|32)=='x')) || c=='p') { q++; continue; }
        }
        if (*q) break;
        if (q>s) last = q[-1];
        Cross(t,copy,s,q,drop,pre);
        if (f1<0 && !pre && q==s && !t->end) f1 = *q;
        if (t->end || (!(Cl[*q]&mask) && !(num && (*q=='+' || *q=='-')))) break;
    }
    Finish(t,copy,s,q,drop);
}
static void ScanStr(BTok *t) {
    unsigned char *s = t->p,*q = s+1,qc = *s;
    int copy = 0,esc = 0;
    for (;;) {
        if (esc) { esc = 0; if (*q && *q!='\n') q++; }
        while (*q && *q!=qc && *q!='\n') {
            if (*q=='\\') { q++; if (!*q) { esc = 1; break; } if (*q=='\n') break; }
            q++;
        }
        if (*q==qc) { q++; break; }
        if (*q=='\n') break;
        Cross(t,copy,s,q,0,0);                          /* a 0: the end of a piece, or of the text */
        if (t->end) break;
    }
    Finish(t,copy,s,q,0);
}

static int HexVal(int c) { return c<='9' ? c-'0' : (c|32)-'a'+10; }
/* the value of an integer; returns 1 if it is not one (a float) */
static int ParseNum(unsigned char *s,int n,unsigned long *v) {
    int i = 0,base = 10,fl = 0;
    unsigned long r = 0;
    if (n>1 && s[0]=='0') {
        if ((s[1]|32)=='x') { base = 16; i = 2; }
        else if ((s[1]|32)=='b') { base = 2; i = 2; }
        else base = 8;
    }
    for (;i<n;i++) {
        int c = s[i],d;
        if (c=='\'') continue;
        if (c=='.') { fl = 1; continue; }
        if (base!=16 && (c|32)=='e') { fl = 1; break; }
        if (base==16 && (c|32)=='p') { fl = 1; break; }
        d = (c>='0' && c<='9') || (base==16 && (c|32)>='a' && (c|32)<='f') ? HexVal(c) : 99;
        if (d>=(base==8 ? 10 : base)) break;            /* a suffix; (09 stays decimal digits) */
        r = r*(unsigned long)base+(unsigned long)d;
    }
    if (!fl) for (;i<n;i++) if (s[i]=='.' || (base!=16 && (s[i]|32)=='e') || (base==16 && (s[i]|32)=='p')) { fl = 1; break; }
    *v = r;
    return fl;
}
static unsigned long CharVal(unsigned char *s,int n) {
    unsigned long r = 0;
    int i = 1,k = 0;
    while (i<n && s[i]!=s[0] && k<4) {
        unsigned long c = s[i++];
        if (c=='\\' && i<n) {
            c = s[i++];
            switch (c) {
            case 'n': c = 10; break;
            case 't': c = 9; break;
            case 'r': c = 13; break;
            case 'a': c = 7; break;
            case 'b': c = 8; break;
            case 'f': c = 12; break;
            case 'v': c = 11; break;
            case 'x': for (c=0;i<n && ((s[i]>='0' && s[i]<='9') || ((s[i]|32)>='a' && (s[i]|32)<='f'));i++) c = c*16+(unsigned long)HexVal(s[i]); break;
            default:
                if (c>='0' && c<='7') { int j; c -= '0'; for (j=1;j<3 && i<n && s[i]>='0' && s[i]<='7';j++) c = c*8+(unsigned long)(s[i++]-'0'); }
            }
        }
        r = r<<8|(c&255);
        k++;
    }
    return r;
}

#define Eat(t,c) ((t)->Buf[(t)->TlsTok.Len++] = (c), (t)->p++, Norm(t))
#define Match(t,c) (*(t)->p==(c) && (Eat(t,c),1))

static void Skip(BTok *t) {
    for (;;) {
        while (Cl[*t->p]&cSp) t->p++;
        if (*t->p || t->end) return;
        Fill(t);
    }
}

static int TokNext(TlsTok *this) {
    TlsGetThat(BTok,TlsTok);
    unsigned long prev = this->End;
    int c,code;
    if (that->nl) { that->nl = 0; this->Line++; }
    if (that->pend) {
        that->pend = 0;
        this->Pos = prev; this->End = prev+1; this->Spc = 0;
        that->Buf[0] = '.'; that->Buf[1] = 0;
        this->Txt = that->Buf; this->Len = 1; this->Val = 0;
        return this->Code = '.';
    }
    Skip(that);
    this->Pos = Off(that);
    this->Spc = this->Pos!=prev;
    this->Val = 0;
    if (that->end) {
        this->Txt = (unsigned char *)EndZ; this->Len = 0; this->End = this->Pos;
        return this->Code = TokNil;
    }
    c = *that->p;
    if (Cl[c]&cAlpha) {
        Scan(that,cLabel,0,0,0);
        this->Val = (unsigned long)x_(that->dict,Id,this->Txt,this->Len);
        return this->Code = TokLabel;
    }
    if (Cl[c]&cDigit) goto Number;
    if (c=='"' || c=='\'') {
        ScanStr(that);
        code = c=='"' ? TokStr : TokChar;
        if (code==TokChar) this->Val = CharVal(this->Txt,this->Len);
        return this->Code = code;
    }
    this->Len = 0;
    that->Buf[0] = that->Buf[1] = that->Buf[2] = that->Buf[3] = 0;
    code = c;
    if (c=='\n') {
        Eat(that,c);
        that->nl = 1;
        code = TokNl;
    } else if (c=='.') {
        Eat(that,c);
        if (Cl[*that->p]&cDigit) {
            Scan(that,cNum,1,'.',0);
            goto Classify;
        }
        if (*that->p=='.') {
            Eat(that,'.');
            if (Match(that,'.')) code = TokOp(Ellipsis);
            else { that->pend = 1; this->Len = 1; that->Buf[1] = 0; }
        }
    } else if (c<32) {
        Eat(that,c);
        this->Val = (unsigned long)c;
        code = TokStray;
    } else {
        Eat(that,c);
        switch (c) {
        case '-': code = Match(that,'>') ? TokOp(Arrow) : Match(that,'-') ? TokOp(Dec) : Match(that,'=') ? TokOp(SubEq) : c; break;
        case '+': code = Match(that,'+') ? TokOp(Inc) : Match(that,'=') ? TokOp(AddEq) : c; break;
        case '<': code = Match(that,'<') ? (Match(that,'=') ? TokOp(ShlEq) : TokOp(Shl)) : Match(that,'=') ? TokOp(Le) : c; break;
        case '>': code = Match(that,'>') ? (Match(that,'=') ? TokOp(ShrEq) : TokOp(Shr)) : Match(that,'=') ? TokOp(Ge) : c; break;
        case '=': code = Match(that,'=') ? TokOp(Eq) : c; break;
        case '!': code = Match(that,'=') ? TokOp(Ne) : c; break;
        case '&': code = Match(that,'&') ? TokOp(AndAnd) : Match(that,'=') ? TokOp(AndEq) : c; break;
        case '|': code = Match(that,'|') ? TokOp(OrOr) : Match(that,'=') ? TokOp(OrEq) : c; break;
        case '^': code = Match(that,'=') ? TokOp(XorEq) : c; break;
        case '*': code = Match(that,'=') ? TokOp(MulEq) : c; break;
        case '/': code = Match(that,'=') ? TokOp(DivEq) : c; break;
        case '%': code = Match(that,'=') ? TokOp(ModEq) : c; break;
        case ':': code = Match(that,':') ? TokOp(Scope) : c; break;
        case '#': code = Match(that,'#') ? TokOp(Paste) : c; break;
        }
    }
    this->Txt = that->Buf;
    this->End = Off(that);
    if (that->pend) this->End = this->Pos+1;
    return this->Code = code;
Number:
    Scan(that,cNum,1,0,0);
Classify:
    code = ParseNum(this->Txt,this->Len,&this->Val) ? TokFloat : TokInt;
    return this->Code = code;
}

static unsigned char *TokRest(TlsTok *this,int *n) {
    TlsGetThat(BTok,TlsTok);
    int pre = 0;
    if (that->nl) { that->nl = 0; this->Line++; }
    Skip(that);
    this->Pos = Off(that);
    if (that->pend) { that->pend = 0; pre = '.'; }
    Scan(that,cLine,0,pre,n==NULL);
    this->Spc = 0;
    this->Code = TokNil;
    if (n) *n = this->Len;
    return this->Txt;
}
static int TokPeek(TlsTok *this,int skip) {
    TlsGetThat(BTok,TlsTok);
    if (that->pend) return '.';
    if (skip) Skip(that);
    return *that->p;
}
static void TokReset(TlsTok *this) {
    TlsGetThat(BTok,TlsTok);
    x_(that->src,Reset);
    that->p = that->b0 = that->src->p;
    that->base = 0; that->end = 0; that->pend = 0; that->nl = 0;
    this->Code = TokNil; this->Spc = 0; this->Len = 0; this->Txt = (unsigned char *)EndZ;
    this->Val = this->Pos = this->End = 0;
    this->Line = 1;
}

TlsTok *TlsTokNew(TlsBuffer0 *Src,TlsDict *D,TlsOutBuf *Ob) {
    static struct TlsTok Static = {TokNext,TokRest,TokPeek,TokReset};
    BTok *r;
    ClInit();
    rPush(r);
    r->TlsTok.Static = &Static;
    r->src = Src;
    r->dict = D;
    r->ob = Ob ? Ob : TlsOutBufNew(NULL);
    r->p = r->b0 = Src->p;
    r->base = 0; r->end = r->pend = r->nl = 0;
    r->TlsTok.Code = TokNil; r->TlsTok.Spc = 0; r->TlsTok.Len = 0; r->TlsTok.Txt = (unsigned char *)EndZ;
    r->TlsTok.Val = r->TlsTok.Pos = r->TlsTok.End = 0;
    r->TlsTok.Line = 1;
    return &r->TlsTok;
}
