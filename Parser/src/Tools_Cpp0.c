/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <string.h>
#include <Tools_Core.h>
#include <Tools_Cpp0.h>

/*_____________________________________________________________________________
 |
 | Cpp0, see Tools_Cpp0.h. The input goes thru three small layers, one byte at a
 | time, with a fast path for the ordinary bytes of the code:
 |
 |   Feed0  the BOM                (bytes held until it is known not to be one)
 |   Feed1  line ends, continued lines          -> Lex(c,line of c)
 |   Lex    comments, literals, blanks          -> Emit / WsTok / Newline
 |   Put    the output block, and the line marks
 |
 | Everything a layer must remember between two bytes is in the filter, so a
 | block may end anywhere (inside a comment, between a backslash and its line
 | end ...). One byte of input gives at most 4 bytes of output: the block has 8
 | bytes of slack beyond its usable size.
 |_____________________________________________________________________________
*/

#ifndef CppBlock
#define CppBlock 512
#endif
#define CppSlack 8
#define EOB ((unsigned char *)TlsBufferEob)
#define WordRound(n) ((((size_t)(n))+sizeof(void *)-1)&~(sizeof(void *)-1))

/* 0 ordinary, 1 letter or _, 2 digit, 3 special (0 \t \v \f ' ' \n \r \\ / " ') */
#define K_OTHER 0
#define K_ALPHA 1
#define K_DIGIT 2
#define K_SPEC  3
static unsigned char const Cl[256] = {
    3,0,0,0,0,0,0,0,0,3,3,3,3,3,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    3,0,3,0,0,0,0,3,0,0,0,0,0,0,0,3,
    2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,0,3,0,0,1,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

enum { S_N,S_SL,S_LC,S_BC,S_BCS,S_STR,S_STRE,S_CHR,S_CHRE };

static unsigned char const CppStart[1] = {0};
#define START ((unsigned char *)CppStart)

typedef struct {
    TlsBuffer0 TlsBuffer0;
    TlsBuffer0 *Src;
    TlsCpp0Hook *Hook;
    int Flags;
    unsigned char *blk;
    unsigned int cap;
    /* from here, Init */
    unsigned int n;                     /* bytes in blk */
    unsigned long base;                 /* output offset of blk[0] */
    unsigned char *q;                   /* next byte of Src (a 0 is to be resolved) */
    int started,fin;
    int bom;                            /* 0..2 bytes of the BOM held, 3 done */
    int cr,bs;                          /* a CR was just seen / a backslash is held */
    unsigned int bsLine,line;           /* line of the held backslash, current line */
    int st;
    unsigned int slLine,bcLine;
    int inWord,numWord;                 /* in a word, in a number */
    int wsp;                            /* a blank is held */
    unsigned int wsLine;
    int lineOut;                        /* the current output line has a byte */
    unsigned int outLine,prevOrig;
} BCpp;

static void Init(BCpp *f) {
    f->n = 0; f->base = 0; f->q = EOB;
    f->started = f->fin = 0;
    f->bom = 0; f->cr = f->bs = 0; f->bsLine = 0; f->line = 1;
    f->st = S_N; f->slLine = f->bcLine = 0;
    f->inWord = f->numWord = 0;
    f->wsp = 0; f->wsLine = 0;
    f->lineOut = 0; f->outLine = 1; f->prevOrig = 0;
}

/* the output */
static void Put(BCpp *f,unsigned char c,unsigned int line) {
    if (!f->lineOut) {                  /* the first byte of an output line */
        f->lineOut = 1;
        if (line!=f->prevOrig+1 && f->Hook && f->Hook->Static->Mark)
            f->Hook->Static->Mark(f->Hook,f->base+f->n,f->outLine,line);
        f->prevOrig = line;
    }
    f->blk[f->n++] = c;
    if (c=='\n') { f->lineOut = 0; f->outLine++; }
}
static void WsTok(BCpp *f,unsigned int line) {
    if (!f->wsp) { f->wsp = 1; f->wsLine = line; }
    f->inWord = f->numWord = 0;
}
static void Emit(BCpp *f,unsigned char c,unsigned int line) {
    if (f->wsp) {
        if (!((f->Flags&Cpp0Trim) && !f->lineOut)) Put(f,' ',f->wsLine);
        f->wsp = 0;
    }
    Put(f,c,line);
}
static void Newline(BCpp *f,unsigned int line) {
    if (f->wsp) {
        if (!(f->Flags&Cpp0Trim)) Put(f,' ',f->wsLine);
        f->wsp = 0;
    }
    Put(f,'\n',line);
}

/* comments, literals, blanks. c is a byte after continuation, line its original line. */
static void Lex(BCpp *f,unsigned char c,unsigned int line) {
again:
    switch (f->st) {
    case S_N:
        switch (Cl[c]) {
        case K_DIGIT:
            Emit(f,c,line);
            if (!f->inWord) { f->inWord = 1; f->numWord = 1; }
            return;
        case K_ALPHA:
            Emit(f,c,line);
            f->inWord = 1;
            return;
        case K_OTHER:
            Emit(f,c,line);
            if (c!='.') f->inWord = f->numWord = 0;                     /* 1.5'3 is one number */
            return;
        }
        if (c=='/') { f->st = S_SL; f->slLine = line; f->inWord = f->numWord = 0; return; }
        if (c=='\n') { Newline(f,line); f->inWord = f->numWord = 0; return; }
        if (c=='"') { Emit(f,c,line); f->st = S_STR; f->inWord = f->numWord = 0; return; }
        if (c=='\'') {
            Emit(f,c,line);
            if (!((f->Flags&Cpp0DigitSep) && f->numWord)) { f->st = S_CHR; f->inWord = f->numWord = 0; }
            return;                     /* else a separator: still in the number */
        }
        if (c=='\\') { Emit(f,c,line); f->inWord = f->numWord = 0; return; }   /* a backslash that is not a continuation */
        WsTok(f,line);                  /* ' ' \t \v \f 0 */
        return;
    case S_SL:
        if (c=='/') { f->st = S_LC; WsTok(f,f->slLine); return; }
        if (c=='*') { f->st = S_BC; f->bcLine = f->slLine; WsTok(f,f->slLine); return; }
        Emit(f,'/',f->slLine);          /* it was just a slash */
        f->st = S_N;
        goto again;
    case S_LC:
        if (c=='\n') { f->st = S_N; Newline(f,line); }
        return;
    case S_BC:
        if (c=='*') f->st = S_BCS;
        return;
    case S_BCS:
        if (c=='/') f->st = S_N;
        else if (c!='*') f->st = S_BC;
        return;
    case S_STR: case S_CHR: {
        unsigned char q = f->st==S_STR ? '"' : '\'';
        if (c=='\n') { f->st = S_N; Newline(f,line); return; }          /* not closed */
        Emit(f,c,line);
        if (c==q) f->st = S_N;
        else if (c=='\\') f->st = f->st==S_STR ? S_STRE : S_CHRE;
        return;
    }
    case S_STRE: case S_CHRE:
        if (c=='\n') { f->st = S_N; Newline(f,line); return; }
        Emit(f,c,line);
        f->st = f->st==S_STRE ? S_STR : S_CHR;
        return;
    }
}

/* line ends and continued lines */
static void Feed1(BCpp *f,unsigned char c) {
    if (f->cr) { f->cr = 0; if (c=='\n') return; }
    if (c=='\r') { c = '\n'; f->cr = 1; }
    if (c=='\n') {
        unsigned int l = f->line++;
        if (f->bs) { f->bs = 0; return; }                               /* continued */
        Lex(f,'\n',l);
        return;
    }
    if (f->bs) { f->bs = 0; Lex(f,'\\',f->bsLine); }
    if (c=='\\') { f->bs = 1; f->bsLine = f->line; return; }
    Lex(f,c,f->line);
}

static unsigned char const Bom[3] = {0xEF,0xBB,0xBF};
static void Feed0(BCpp *f,unsigned char c) {
    if (c==0) c = ' ';                  /* the output holds no 0, a literal's included */
    if (f->bom<3) {
        if (c==Bom[f->bom]) { f->bom++; return; }                       /* held, or the BOM is over */
        {   int i,k = f->bom;
            f->bom = 3;
            for (i=0;i<k;i++) Feed1(f,Bom[i]);                          /* it was not one */
        }
    }
    Feed1(f,c);
}

/* the end of the input */
static void Finish(BCpp *f) {
    if (f->bom>0 && f->bom<3) {
        int i,k = f->bom;
        f->bom = 3;
        for (i=0;i<k;i++) Feed1(f,Bom[i]);
    }
    f->bom = 3;
    if (f->bs) { f->bs = 0; Lex(f,'\\',f->bsLine); }
    if (f->st==S_SL) Emit(f,'/',f->slLine);
    else if ((f->st==S_BC || f->st==S_BCS) && f->Hook && f->Hook->Static->Warn)
        f->Hook->Static->Warn(f->Hook,f->bcLine,"unterminated comment");
    f->st = S_N;
    if (f->lineOut) Newline(f,f->line);
    else if (f->wsp && !(f->Flags&Cpp0Trim)) Newline(f,f->line);
    f->wsp = 0;
    f->fin = 1;
}

/* Produces the next block: stops when it is full or when the input is over. */
static void Run(BCpp *f) {
    f->base += f->n;
    f->n = 0;
    while (f->n<f->cap && !f->fin) {
        unsigned char *q = f->q,c;
        if (f->st==S_N && !f->cr && !f->bs && !f->wsp && f->lineOut && f->bom>=3) {
            while (f->n<f->cap) {                                       /* the fast path */
                unsigned char k;
                c = *q;
                k = Cl[c];
                if (k==K_SPEC) break;
                if (k==K_DIGIT) { if (!f->inWord) { f->inWord = 1; f->numWord = 1; } }
                else if (k==K_ALPHA) f->inWord = 1;
                else if (c!='.') f->inWord = f->numWord = 0;
                f->blk[f->n++] = c;
                q++;
            }
            f->q = q;
            if (f->n>=f->cap) break;
        }
        c = *q;
        if (c==0) {
            unsigned char *r = x_(f->Src,Check0,q);
            if (r==EOB) { f->q = EOB; Finish(f); break; }
            if (r==q) { f->q = q+1; Feed0(f,0); }                       /* a data 0 */
            else f->q = r;
        } else {
            f->q = q+1;
            Feed0(f,c);
        }
    }
    f->blk[f->n] = 0;
}

static unsigned char *CppCheck0(TlsBuffer0 *this,unsigned char *pos);
static unsigned char *CppReset(TlsBuffer0 *this);
static void CppClose(TlsBuffer0 *this);
static struct TlsBuffer0 CppStatic = {CppCheck0,CppReset,CppClose};

static unsigned char *CppCheck0(TlsBuffer0 *this,unsigned char *pos) {
    TlsGetThat(BCpp,TlsBuffer0);
    if (pos==EOB) return EOB;
    if (!that->started) { that->started = 1; that->q = that->Src->p; }
    if (!that->fin) {
        Run(that);
        if (that->n) return that->TlsBuffer0.p = that->blk;
    }
    that->TlsBuffer0.p = EOB;
    return EOB;
}
static unsigned char *CppReset(TlsBuffer0 *this) {
    TlsGetThat(BCpp,TlsBuffer0);
    x_(that->Src,Reset);
    Init(that);
    if (that->Hook && that->Hook->Static->Reset) that->Hook->Static->Reset(that->Hook);
    return that->TlsBuffer0.p = START;
}
static void CppClose(TlsBuffer0 *this) {
    TlsGetThat(BCpp,TlsBuffer0);
    x_(that->Src,Close);
    that->fin = 1;
    that->n = 0;
    that->TlsBuffer0.p = EOB;
}

TlsBuffer0 *TlsCpp0Filter(TlsBuffer0 *Src,int Flags,TlsCpp0Hook *Hook) {
    BCpp *r;
    size_t sz = WordRound(CppBlock+CppSlack+1);
    rPush(r);
    rnPush(r->blk,sz);
    r->TlsBuffer0.Static = &CppStatic;
    r->TlsBuffer0.p = START;
    r->Src = Src ? Src : TlsBuffer0Null;
    r->Hook = Hook;
    r->Flags = Flags;
    r->cap = CppBlock;
    Init(r);
    return &r->TlsBuffer0;
}

unsigned long TlsCpp0OutPos(TlsBuffer0 *Cpp0,unsigned char *p) {
    BCpp *f;
    if (!Cpp0 || Cpp0->Static!=&CppStatic) return (unsigned long)-1;
    f = (BCpp *)Cpp0;
    if (p<f->blk || p>f->blk+f->n) return (unsigned long)-1;
    return f->base+(unsigned long)(p-f->blk);
}
