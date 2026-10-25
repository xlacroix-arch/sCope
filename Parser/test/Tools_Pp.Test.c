/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Pp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

/* The tokens of a text, as one string: the texts of the tokens, with a space
 * where the token has Spc. Newlines are shown as | */
static char Out[100000];
static TlsPp *LastPp;
static char *Run2(TlsBuffer0 *src,TlsPpCtx *cx,int flags) {
    TlsPp *p = TlsPpNew(cx,src,NULL,NULL,flags);
    char *w = Out;
    int first = 1,c;
    LastPp = p;
    while ((c = x_(p,Next))!=TokNil) {
        if (!first && p->Spc) *w++ = ' ';
        if (c==TokNl) *w++ = '|'; else { memcpy(w,p->Txt,(size_t)p->Len); w += p->Len; }
        first = 0;
    }
    *w = 0;
    return Out;
}
static char *Ex(char const **defs,char const *text) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    for (;defs && *defs;defs++) CHECK(x_(cx->Macros,DefineStr,*defs)!=0);
    return Run2(TlsStringBuffer((unsigned char *)text),cx,0);
}
#define EQ(defs,text,res) do { char *r_ = Ex(defs,text); if (strcmp(r_,res)) { printf("line %d: [%s] gave [%s], wanted [%s]\n",__LINE__,text,r_,res); exit(1); } } while (0)

static void TestExpand(void) {
    static char const *M1[] = {"N 42","ADD(x,y) x + y","F(x) G(x)+1","G(y) y*2","A A+1","X Y","Y X","SQ(x) x*x","S(x) #x","CAT(a,b) a##b","T int ## _t",
        "V(f,...) f(__VA_ARGS__)","E(x) [x]","Z() zero","TWO(a,b) a b","W  spaced   body ",0};
    EQ(M1,"a N b","a 42 b");
    EQ(M1,"ADD(1,2)","1 + 2");
    EQ(M1,"ADD(1,2)*3","1 + 2*3");
    EQ(M1,"F(3)","3*2+1");
    EQ(M1,"A","A+1");
    EQ(M1,"X","X");
    EQ(M1,"Y","Y");
    EQ(M1,"F","F");
    EQ(M1,"F + 1","F + 1");
    EQ(M1,"F (1)","1*2+1");
    EQ(M1,"F\n(1)","F (1)");                        /* the call is on the same line only */
    EQ(M1,"SQ(a+b)","a+b*a+b");
    EQ(M1,"ADD(N,N)","42 + 42");
    EQ(M1,"ADD(F(1),2)","1*2+1 + 2");
    EQ(M1,"ADD((a,b),c)","(a,b) + c");
    EQ(M1,"ADD(1,\n2)","1 + 2");
    EQ(M1,"S( a   \"b\\n\" )","\"a \\\"b\\\\n\\\"\"");
    EQ(M1,"CAT(foo,bar)","foobar");
    EQ(M1,"CAT(1,2)+CAT(x, y)","12+xy");
    EQ(M1,"T","int_t");
    EQ(M1,"V(g,1,2,3)","g(1,2,3)");
    EQ(M1,"V(g)","g()");
    EQ(M1,"E()","[]");
    EQ(M1,"E(  )","[]");
    EQ(M1,"Z()","zero");
    EQ(M1,"Z( )","zero");
    EQ(M1,"TWO(,)","");
    EQ(M1,"TWO(1,)+1","1+1");
    EQ(M1,"W","spaced body");
    EQ(M1,"x W y","x spaced body y");
    EQ(M1,"N;N","42;42");
    EQ(M1,"N N","42 42");
    EQ(M1,"\"N\" 'N' N1 _N","\"N\" 'N' N1 _N");
}
static void TestPreExpand(void) {
    static char const *M[] = {"N 42","S(x) #x","XS(x) S(x)","CAT(a,b) a##b","XCAT(a,b) CAT(a,b)","P(x) x x","ID(x) x","MN 3","ONE 1","TWO ONE+ONE",
        "F(x,y) [x|y]","V(...) <__VA_ARGS__>","SAME(x) x,x","WRAP(x) (x)","MIX(x) x #x x##1",0};
    EQ(M,"XS(N) S(N)","\"42\" \"N\"");
    EQ(M,"CAT(N,N) XCAT(N,N) XCAT(M,N)","NN 4242 M42");
    EQ(M,"XCAT(MN,ONE)","31");
    EQ(M,"P(N)","42 42");
    EQ(M,"P(ID(N)) ID(ID(ID(N)))","42 42 42");
    EQ(M,"F(TWO,ID(N))","[1+1|42]");
    EQ(M,"V(N,TWO)","<42,1+1>");
    EQ(M,"WRAP(SAME(N))","(42,42)");
    EQ(M,"ID(ONE ID(ONE))","1 1");
    EQ(M,"ID(-ID(-1))","- -1");
    EQ(M,"MIX(N)","42 \"N\" N1");
    EQ(M,"ID(S(N))","\"N\"");
    EQ(M,"ID(ID)(1)","ID(1)");
}
static void TestErrors(void) {
    static char const *M[] = {"F(x) [x]","TWO(a,b) a b",0};
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    TlsMacroLib *lib = cx->Macros;
    char *r;
    x_(lib,DefineStr,"F(x) [x]"); x_(lib,DefineStr,"TWO(a,b) a b");
    r = Run2(TlsStringBuffer((unsigned char *)"F(1"),cx,0);
    CHECK(cx->Errors==1 && !strcmp(r,"[1]"));
    cx->Errors = 0;
    r = Run2(TlsStringBuffer((unsigned char *)"TWO(1,2,3) ok"),cx,0);
    CHECK(cx->Errors==1 && !strcmp(r,"1 2 ok"));
    cx->Errors = 0;
    r = Run2(TlsStringBuffer((unsigned char *)"TWO(1) ok"),cx,0);
    CHECK(cx->Errors==1 && !strcmp(r,"1 ok"));
    cx->Errors = 0;
    x_(lib,DefineStr,"_same(x) x,x"); x_(lib,DefineStr,"_prod(x,y) x*y");
    r = Run2(TlsStringBuffer((unsigned char *)"_same(3) _prod(2,3) _prod(_same(2))"),cx,0);
    CHECK(cx->Errors==1 && !strcmp(r,"3,3 2*3 2,2*"));
    cx->Errors = 0;
    CHECK(!x_(lib,DefineStr,"1bad x") && !x_(lib,DefineStr,"G(a,) x") && !x_(lib,DefineStr,"G(a b) x") && !x_(lib,DefineStr,""));
    (void)M;
}
static void TestLib(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    TlsMacroLib *lib = cx->Macros;
    int a = x_(lib,DefineStr,"A 1"),b = x_(lib,DefineStr,"B(x,y) x"),c = x_(lib,DefineStr,"C(x,...) x"),e = x_(lib,DefineStr,"EMPTY");
    CHECK(a && b && c && e && a!=b);
    CHECK(x_(lib,Get,a)->Params==-1 && x_(lib,Get,b)->Params==2 && x_(lib,Get,c)->Params==2 && x_(lib,Get,c)->Variadic && !x_(lib,Get,b)->Variadic);
    CHECK(x_(lib,Get,999)==NULL);
    CHECK(x_(lib,Undef,a)==1 && x_(lib,Undef,a)==0 && x_(lib,Get,a)==NULL);
    CHECK(x_(lib,DefineStr,"A(q) q")==a && x_(lib,Get,a)->Params==1);
    CHECK(x_(lib,DefineStr,"A 2")==a && x_(lib,Get,a)->Params==-1);       /* defined again */
    CHECK(!strcmp(Run2(TlsStringBuffer((unsigned char *)"A EMPTY A"),cx,0),"2 2"));
}
static void TestNl(void) {
    static char const *M[] = {"N 1",0};
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    TlsMacroLib *lib = cx->Macros;
    x_(lib,DefineStr,"N 1");
    CHECK(!strcmp(Run2(TlsStringBuffer((unsigned char *)"a N\nb\n"),cx,PpNl),"a 1|b|"));
    CHECK(!strcmp(Run2(TlsStringBuffer((unsigned char *)"a N\nb\n"),cx,0),"a 1 b"));
    (void)M;
}
static void TestPlaces(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    TlsMacroLib *lib = cx->Macros;
    TlsPp *p;
    x_(lib,DefineStr,"ADD(x,y) x + y");
    p = TlsPpNew(cx,TlsStringBuffer((unsigned char *)"a\n  ADD(1,\n2) b"),NULL,NULL,0);
    CHECK(x_(p,Next)=='l' && p->Line==1 && p->Pos==0 && p->Exp==0);
    CHECK(x_(p,Next)=='i' && p->Line==2 && p->Pos==4 && p->End==13 && p->Exp==1 && p->Spc==1);        /* the whole call */
    CHECK(x_(p,Next)=='+' && p->Pos==4 && p->End==13 && p->Exp==1 && p->Spc==1);
    CHECK(x_(p,Next)=='i' && p->Pos==4 && p->Exp==1);
    CHECK(x_(p,Next)=='l' && p->Line==3 && p->Pos==14 && p->Exp==0 && p->Spc==1);
    CHECK(x_(p,Next)==TokNil && x_(p,Next)==TokNil);
}

/* what goes wrong reaches the hook, with the file and the line of the original */
typedef struct { TlsPpHook TlsPpHook; int n,opens; unsigned int line; int kind; char file[40],text[60]; } Cap1;
static void CMessage(TlsPpHook *this,int Kind,char const *File,unsigned int Line,char const *Text) {
    Cap1 *c = (Cap1 *)this;
    c->n++; c->line = Line; c->kind = Kind;
    strncpy(c->file,File ? File : "(none)",39); strncpy(c->text,Text,59);
}
static void COpen(TlsPpHook *this,char const *File,unsigned int Line,char const *Path) {
    Cap1 *c = (Cap1 *)this;
    (void)File; (void)Line; (void)Path;
    c->opens++;
}
static void TestMessages(void) {
    static struct TlsPpHook H = {CMessage,0,COpen};
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    Cap1 c;
    TlsCpp0Hook *map = TlsLineMapNew(NULL);
    TlsBuffer0 *f;
    TlsPp *p;
    memset(&c,0,sizeof c);
    c.TlsPpHook.Static = &H;
    cx->Hook = &c.TlsPpHook;
    x_(cx->Macros,DefineStr,"F(x) [x]");
    f = TlsCpp0Filter(TlsStringBuffer((unsigned char *)"x \\\n y /* a\n b */\nF(1\n"),Cpp0Default,map);
    p = TlsPpNew(cx,f,"dir/t.c",map,0);
    CHECK(c.opens==1);
    while (x_(p,Next)) ;
    CHECK(c.n==1 && c.kind==PpMsgError && c.line==4 && !strcmp(c.file,"dir/t.c") && !strcmp(c.text,"macro call not closed"));
    CHECK(cx->Errors==1);
    c.n = 0;
    p = TlsPpNew(cx,TlsStringBuffer((unsigned char *)"\n\nF(1"),NULL,NULL,0);        /* no name, no map */
    while (x_(p,Next)) ;
    CHECK(c.n==1 && c.line==3 && !strcmp(c.file,"(none)") && c.opens==1);
}
/* the # lines */
typedef struct { TlsPpHook TlsPpHook; int n,kinds[16]; unsigned int lines[16]; char texts[16][80]; int dirs,dword,did,dret; } Log;
static void LMessage(TlsPpHook *this,int Kind,char const *File,unsigned int Line,char const *Text) {
    Log *l = (Log *)this;
    (void)File;
    if (l->n<16) { l->kinds[l->n] = Kind; l->lines[l->n] = Line; strncpy(l->texts[l->n],Text,79); }
    l->n++;
}
static int LDirective(TlsPpHook *this,int Word,int Id,TlsTok *t,char const *File,unsigned int Line) {
    Log *l = (Log *)this;
    (void)File; (void)Line;
    l->dirs++; l->dword = Word; l->did = Id;
    if (l->dret) x_(t,Next);                              /* it reads a word of the line (the rest is skipped by the Pp) */
    return l->dret;
}
static struct TlsPpHook LogHook = {LMessage,LDirective,0};
static Log *Fresh(TlsPpCtx *cx) {
    static Log l;
    memset(&l,0,sizeof l);
    l.TlsPpHook.Static = &LogHook;
    cx->Hook = &l.TlsPpHook;
    return &l;
}
#define OUT(ctx,text,flags,res) do { char *r_ = Run2(TlsStringBuffer((unsigned char *)(text)),ctx,flags); if (strcmp(r_,res)) { printf("line %d: [%s] gave [%s], wanted [%s]\n",__LINE__,text,r_,res); exit(1); } } while (0)

static void TestDirectives(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    Log *l = Fresh(cx);
    OUT(cx,"#define N 42\na N\n",0,"a 42");
    OUT(cx,"#define N 42\na N\n",PpNl,"|a 42|");
    OUT(cx,"#define ADD(x,y) x + y\n  # undef N\nADD(1,2) N\n",0,"1 + 2 N");
    OUT(cx,"#define K 1\n#define K 2\nK\n",0,"2");
    OUT(cx,"#define E\nE x E\n",0,"x");
    OUT(cx,"#define M(a) [a]\n#undef M\nM(1)\n",0,"M(1)");
    OUT(cx,"   #   define   SP   a   b  \nSP\n",0,"a b");
    OUT(cx,"#\n#define Z 1\n#\nZ\n",0,"1");
    CHECK(l->n==0 && cx->Errors==0);
    /* #ifdef family */
    OUT(cx,"#define A\n#ifdef A\nyes\n#else\nno\n#endif\n",0,"yes");
    OUT(cx,"#ifdef A\nyes\n#else\nno\n#endif\n",0,"yes");              /* A is defined by the first line of the context above: macros live in the context */
    OUT(cx,"#undef A\n#ifdef A\nyes\n#else\nno\n#endif\n",0,"no");
    OUT(cx,"#ifndef A\nyes\n#else\nno\n#endif\n#ifndef B\nb\n#endif\n#ifdef B\nnot\n#endif\n",0,"yes b");
    OUT(cx,"#define X 1\n#ifdef X\n#ifndef Y\nin\n#else\nnot\n#endif\n#else\nnot2\n#endif\nafter\n",0,"in after");
    /* an inactive group is only looked at for its # lines */
    OUT(cx,"#ifdef NOPE\n'unclosed \"quote /* ( \n#define LEAK 1\n#error no\n#bogus\n#ifdef X\n#else\nz\n#endif\n#else\nok\n#endif\nLEAK\n",0,"ok LEAK");
    CHECK(l->n==0);
    OUT(cx,"#ifdef NOPE\nno\n#endif\n#ifndef NOPE\nyes\n#endif\n",PpNl,"||||yes||");
    OUT(cx,"a\n#ifdef NOPE\nb\nc\n#endif\nd\n",PpNl,"a|||||d|");          /* the lines stay: a line end for each */
    /* macros defined in a group that is read */
    OUT(cx,"#ifndef GUARD\n#define GUARD\n#define G 7\n#endif\n#ifndef GUARD\n#define G 8\n#endif\nG\n",0,"7");
    /* the end of the text, no end of line */
    OUT(cx,"#define Q 5\nQ",0,"5");
    OUT(cx,"#define Q2 6",0,"");
    OUT(cx,"#ifdef Q\nq",0,"q");
    CHECK(cx->Errors==1 && l->n==1 && !strcmp(l->texts[0],"conditional not closed") && l->lines[0]==1);
}
static void TestDirectiveErrors(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    Log *l = Fresh(cx);
    int ido,idf;
    OUT(cx,"#else\nx\n#endif\n#elif\ny\n",0,"x y");
    CHECK(cx->Errors==3 && !strcmp(l->texts[0],"#else without #if") && l->lines[0]==1 && !strcmp(l->texts[1],"#endif without #if") && l->lines[1]==3 && !strcmp(l->texts[2],"#elif without #if") && l->lines[2]==4);
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#ifdef U\na\n#else\nb\n#else\nc\n#endif\n",0,"b c");
    CHECK(cx->Errors==1 && !strcmp(l->texts[0],"#else after #else") && l->lines[0]==5);
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#define\nx\n#define 3\ny\n#define F(a,\nz\n#undef\nw\n#undef 4\n#ifdef\nv\n#endif\n",0,"x y z w");   /* the line that fails is skipped, and only it */
    CHECK(cx->Errors==6 && l->n==6 && !strcmp(l->texts[0],"bad #define") && !strcmp(l->texts[2],"bad #define") && !strcmp(l->texts[3],"bad #undef") && !strcmp(l->texts[5],"bad #ifdef"));
    CHECK(l->lines[0]==1 && l->lines[1]==3 && l->lines[2]==5 && l->lines[3]==7 && l->lines[4]==9 && l->lines[5]==10);
    OUT(cx,"#define\n#define Y 1\nY\n",PpNl,"||1|");                 /* the line end of a bad line is still there: the next # is a directive */
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#error  stop here\n#warning careful\ntext\n#ifdef NOPE\n#error hidden\n#endif\n",0,"text");
    CHECK(cx->Errors==1 && cx->Warnings==1 && l->n==2 && l->kinds[0]==PpMsgError && !strcmp(l->texts[0],"stop here") && l->kinds[1]==PpMsgWarning && !strcmp(l->texts[1],"careful") && l->lines[1]==2);
    cx->Errors = 0; l->n = 0;
    ido = x_(cx->Dict,Id,(unsigned char *)"other",5); idf = x_(cx->Dict,Id,(unsigned char *)"frobnicate",10);
    OUT(cx,"#pragma other\nz\n",0,"z");        /* (#pragma once is the Pp's own) */
    CHECK(l->dirs==1 && l->dword==PpWord(Pragma) && l->did==ido && cx->Errors==0);
    OUT(cx,"#pragma\nz\n",0,"z");
    CHECK(l->dirs==2 && l->dword==PpWord(Pragma) && l->did==0 && cx->Errors==0);
    OUT(cx,"#frobnicate x y\nz\n",0,"z");
    CHECK(l->dirs==3 && l->dword==0 && l->did==idf && cx->Errors==1 && !strcmp(l->texts[0],"unknown directive"));
    cx->Errors = 0; l->n = 0; l->dret = 1;
    OUT(cx,"#frobnicate x y\n#pragma other a b\nz\n",0,"z");
    CHECK(l->dirs==5 && cx->Errors==0 && l->n==0);                   /* the hook had it; the rest of the line is skipped */
    OUT(cx,"#ifdef NOPE\n#frobnicate\n#pragma x\n#endif\n",0,"");
    CHECK(l->dirs==5);                                               /* not asked in a group that is off */
    l->dret = 0; l->n = 0;
    OUT(cx,"# 12 \"x\"\nq\n",0,"q");
    CHECK(cx->Errors==1 && !strcmp(l->texts[0],"bad directive"));
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#include \"a.h\"\nb\n",0,"b");
    CHECK(cx->Errors==1 && !strcmp(l->texts[0],"#include file not found"));
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#define S\n#ifdef S\na\n#elif 1\nb\n#endif\n",0,"a");
    CHECK(cx->Errors==0);
    OUT(cx,"#ifdef NOPE\n#if 0\nx\n#endif\ny\n#endif\nok\n",0,"ok");     /* nested in a group that is off: silent, and balanced */
    CHECK(cx->Errors==0);
}

/* the directives */
typedef struct { TlsPpHook TlsPpHook; int n; int kinds[16]; unsigned int lines[16]; char texts[16][80]; int dirs,lastWord,lastId,take; } DRec;
static void DMessage(TlsPpHook *this,int Kind,char const *File,unsigned int Line,char const *Text) {
    DRec *r = (DRec *)this;
    (void)File;
    if (r->n<16) { r->kinds[r->n] = Kind; r->lines[r->n] = Line; strncpy(r->texts[r->n],Text,79); }
    r->n++;
}
static int DDirective(TlsPpHook *this,int Word,int Id,TlsTok *t,char const *File,unsigned int Line) {
    DRec *r = (DRec *)this;
    (void)File; (void)Line;
    r->dirs++; r->lastWord = Word; r->lastId = Id;
    if (r->take) x_(t,Rest,NULL);
    return r->take;
}
static DRec D;
static char *Pre(char const *text,int flags) {
    static struct TlsPpHook H = {DMessage,DDirective,0};
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    memset(&D,0,sizeof D);
    D.TlsPpHook.Static = &H;
    cx->Hook = &D.TlsPpHook;
    return Run2(TlsStringBuffer((unsigned char *)text),cx,flags);
}
#define PRE(text,res) do { char *r_ = Pre(text,0); if (strcmp(r_,res)) { printf("line %d: [%s] gave [%s], wanted [%s]\n",__LINE__,text,r_,res); exit(1); } } while (0)
#define PREN(text,res,msgs) do { char *r_ = Pre(text,0); if (strcmp(r_,res) || D.n!=(msgs)) { printf("line %d: [%s] gave [%s] with %d messages, wanted [%s] and %d\n",__LINE__,text,r_,D.n,res,msgs); exit(1); } } while (0)

static long Rss(void);
static void TestIf(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    Log *l = Fresh(cx);
    OUT(cx,"#if 1\nin\n#else\nout\n#endif\n",0,"in");
    OUT(cx,"#if 0\nin\n#else\nout\n#endif\n",0,"out");
    OUT(cx,"#if 1+2*3==7 && (4>>1)==2 && -1<0 && !0 && ~0==-1\nyes1\n#endif\n#if 0\nno\n#elif 5%3==2\nyes2\n#endif\n",0,"yes1 yes2");
    CHECK(cx->Errors==0);
    OUT(cx,"#define N 5\n#define F(x) ((x)+1)\n#if defined(N) && defined N && !defined(M) && F(N)==6\nyes\n#endif\n",0,"yes");
    OUT(cx,"#if defined ( N ) + defined M\none\n#endif\n#if N > 4 && M\nno\n#else\nyes\n#endif\n",0,"one yes");   /* a name left over is 0 */
    OUT(cx,"#if 0\n#if 1\nno\n#else\nno\n#endif\n#elif 1\nyes\n#if 0\nno\n#elif 0\nno\n#else\nyes\n#endif\n#else\nno\n#endif\n",0,"yes yes");
    OUT(cx,"#if 1\na\n#elif 1\nb\n#elif 1\nc\n#else\nd\n#endif\n#if 0\na\n#elif 0\nb\n#elif 2\nc\n#elif 3\nd\n#else\ne\n#endif\n",0,"a c");
    CHECK(cx->Errors==0);
    OUT(cx,"#if 0\n#define Z 1\n#error not read\n#bogus\n#if (\n#endif\n#elif 1\n#elif 1/0\n#endif\n#ifdef Z\nno\n#else\nyes\n#endif\n",0,"yes");
    CHECK(cx->Errors==0);                                           /* nothing is evaluated in a group that is off, nor after a group was taken */
    OUT(cx,"#if 1\n#elif (\n#elif 1/0\n#endif\nok\n",0,"ok");
    CHECK(cx->Errors==0);
    OUT(cx,"#define A 1\n#define B 2\n#if A+B==3\nx\n#endif\n#undef A\n#ifdef A\nno\n#endif\n#if A+B==2\ny\n#endif\n",0,"x y");
    OUT(cx,"#define ON 1\n#define PLUS(a,b) a+b\n#if PLUS(ON,ON)==2 && PLUS(1,3)==4\nyes\n#endif\n",0,"yes");
    OUT(cx,"#define EMPTY\n#define LT <\n#if 1 LT 2 EMPTY\nyes\n#endif\n",0,"yes");
    OUT(cx,"#if 'a'==97 && 0x10==16 && 10u==10\nyes\n#endif\n",0,"yes");
    OUT(cx,"#if 1\n#if 1\nin\n#endif\nafter\n#endif\n",0,"in after");
    OUT(cx,"#if 0\na\n#endif\n#if 1\nb\n#endif\n",PpNl,"||||b||");  /* the lines stay */
    /* the errors: the group is off, and the line is still read */
    OUT(cx,"#if\na\n#else\nb\n#endif\n",0,"b");
    CHECK(cx->Errors==1 && !strcmp(l->texts[0],"expression expected"));
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#if 1 +\na\n#else\nb\n#endif\n",0,"b");
    CHECK(cx->Errors==1 && l->lines[0]==1);
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#if defined\na\n#endif\n#if defined(X\nb\n#endif\n#if 1/0\nc\n#endif\n#if 1.5\nd\n#endif\n#if 1 2\ne\n#endif\nz\n",0,"z");
    CHECK(cx->Errors==5);
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#if 0\na\n#elif\nb\n#elif 1\nc\n#endif\n",0,"c");
    CHECK(cx->Errors==1 && l->lines[0]==3);
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#if 1\n#else\n#elif 1\n#endif\n",0,"");
    CHECK(cx->Errors==1 && !strcmp(l->texts[0],"#elif after #else"));
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#if 1\na\n",0,"a");
    CHECK(cx->Errors==1 && !strcmp(l->texts[0],"conditional not closed"));
    {   /* the room of an #if is given back: lines and lines of them do not grow the process */
        int i,reps = 5000,per = (int)strlen("#if defined(F) && F(1)==2\nx\n#elif 0\n#endif\n");
        char *text = malloc((size_t)per*(size_t)reps+1);
        TlsPp *p;
        long r0,r1;
        for (i=0;i<reps;i++) memcpy(text+(size_t)i*(size_t)per,"#if defined(F) && F(1)==2\nx\n#elif 0\n#endif\n",(size_t)per);
        text[(size_t)per*(size_t)reps] = 0;
        x_(cx->Macros,DefineStr,"F(a) (a+1)");
        p = TlsPpNew(cx,TlsBufferBuffer(text,text+(size_t)per*(size_t)reps),NULL,NULL,0);
        for (i=0;i<2000;i++) x_(p,Next);
        r0 = Rss();
        while (x_(p,Next)!=TokNil) ;
        r1 = Rss();
        printf("#if: rss growth %ld KB\n",r1-r0);
        if (OutChunk>=256) CHECK(r1-r0<2048);
        free(text);
    }
}
static char *RunN(char const *text,char const *name,TlsPpCtx *cx) {
    TlsPp *p = TlsPpNew(cx,TlsStringBuffer((unsigned char *)text),name,NULL,0);
    char *w = Out;
    int first = 1;
    while (x_(p,Next)!=TokNil) {
        if (!first && p->Spc) *w++ = ' ';
        memcpy(w,p->Txt,(size_t)p->Len); w += p->Len;
        first = 0;
    }
    *w = 0;
    return Out;
}
#define NM(text,name,res) do { TlsPpCtx *c_ = TlsPpCtxNew(NULL,NULL); char *r_ = RunN(text,name,c_); if (strcmp(r_,res)) { printf("line %d: [%s] gave [%s], wanted [%s]\n",__LINE__,text,r_,res); exit(1); } } while (0)
static void TestLine(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    Log *l = Fresh(cx);
    NM("__LINE__ __LINE__\n__LINE__\n\n__LINE__","a.c","1 1 2 4");
    NM("#define L __LINE__\n\nL L\n","a.c","3 3");
    NM("#define ID(x) x\nID(__LINE__)\nID(\n__LINE__) ID(ID(__LINE__))","a.c","2 3 4");
    NM("#define S(x) #x\n#define XS(x) S(x)\nS(__LINE__) XS(__LINE__) S(__FILE__)","a.c","\"__LINE__\" \"3\" \"__FILE__\"");
    NM("__FILE__\n__FILE__","a.c","\"a.c\" \"a.c\"");
    NM("__FILE__","dir/with \"q\" \\x.c","\"dir/with \\\"q\\\" \\\\x.c\"");
    NM("__FILE__","","\"\"");
    NM("#define F __FILE__ __LINE__\nF\n#define G(x) x __FILE__\nG(__LINE__)","a.c","\"a.c\" 2 4 \"a.c\"");
    NM("#line 100 \"b.c\"\n__FILE__ __LINE__\n__LINE__","a.c","\"b.c\" 100 101");
    NM("x\n#line 50\n__LINE__ __FILE__\n#line 7\n__LINE__","a.c","x 50 \"a.c\" 7");
    NM("#line 10 \"x\"\n#line 20\n__FILE__ __LINE__","a.c","\"x\" 20");               /* a #line with no name keeps the last one */
    NM("#define N 20\n#define NAME \"m.c\"\n#line N NAME\n__LINE__ __FILE__","a.c","20 \"m.c\"");
    NM("#line 1 \"a\\\\b\\\"c\"\n__FILE__","a.c","\"a\\\\b\\\"c\"");                     /* the escapes are read, and written again */
    NM("#line 5\n#ifdef __LINE__\nyes\n#endif\n#if defined(__FILE__) && __LINE__==8 && !defined(__FILES__)\nyes2\n#endif","a.c","yes yes2");
    NM("#if __LINE__==1\na\n#elif __LINE__==2\nb\n#endif\n#if 0\n#elif __LINE__==7\nc\n#endif","a.c","a c");
    NM("#define __LINE__ 3\n__LINE__","a.c","3");                                      /* a macro of the same name comes first */
    OUT(cx,"#line 100\n#error boom\n#warning w\n#line 7 \"q\"\n#pragma x\n",0,"");
    CHECK(l->n>=2 && l->lines[0]==100 && l->lines[1]==101);
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#line\n#line x\n#line 1 2\n#line 1 \"a\" b\n#line 99999999999\n#line 3 \"a\n#line 0x10\nz __LINE__\n",0,"z 16");
    CHECK(cx->Errors==5 && l->lines[0]==1 && l->lines[4]==5 && !strcmp(l->texts[0],"bad #line"));
    cx->Errors = 0; l->n = 0;
    OUT(cx,"#ifdef NOPE\n#line 1 2 3 (\n#endif\n__LINE__",0,"4");
    CHECK(cx->Errors==0);
    OUT(cx,"#line 16\n__LINE__\n#line 1\n__LINE__",PpNl,"|16||1");
}
static void TestDirectivesMore(void) {
    PRE("#define N 42\na N\n","a 42");
    PRE("a\n#define N 42\nN b\n","a 42 b");
    PRE("N\n#define N 1\nN\n","N 1");                                /* it counts from the line on */
    PRE("#define F(x,y) x+y\nF(1,2) F\n","1+2 F");
    PRE("#define N 1\n#undef N\nN\n","N");
    PRE("#define N 1\n#undef N\n#undef N\n#undef NEVER\nN\n","N");
    PRE("#define N 1\n#define N 2\nN\n","2");                        /* again: it replaces */
    PRE("#define E\nE a E\n","a");
    PRE("  #  define   N   7  \nN\n","7");                            /* blanks around the # and the word */
    PRE("a # b\n","a # b");                                            /* a # that does not start a line */
    PRE("#define S(x) #x\nS(a) #x\n","\"a\" #x");
    PRE("#\n#\na\n","a");                                            /* a # alone */
    PRE("#ifdef N\nyes\n#else\nno\n#endif\nend\n","no end");
    PRE("#define N\n#ifdef N\nyes\n#else\nno\n#endif\nend\n","yes end");
    PRE("#ifndef N\nyes\n#else\nno\n#endif\n","yes");
    PRE("#define N\n#ifndef N\nyes\n#else\nno\n#endif\n","no");
    PRE("#ifdef N\nyes\n#endif\nend\n","end");
    PRE("#ifndef N\n#define N 3\n#endif\nN\n#ifndef N\n#define N 4\n#endif\nN\n","3 3");        /* a guard */
    PRE("#define A\n#ifdef A\n#ifdef B\nab\n#else\na\n#endif\n#else\nnone\n#endif\n","a");
    PRE("#ifdef A\n#ifdef B\nab\n#else\n#else\n#endif\n#endif\nz\n","z");
    PRE("#ifdef A\n#define N 1\n#undef M\n#include <x>\n#error no\n#pragma p\n#bogus\ndon't \"x\nq\n#else\nN\n#endif\n","N");  /* not done, not even read, in a group that is off */
    PRE("#ifdef A\n#ifndef B\nx\n#else\ny\n#endif\n#else\nz\n#endif\n","z");               /* the nested ones count in a group that is off */
    PRE("#define A\n#ifdef A\n#define B 1\n#endif\nB\n","1");
    PRE("a\n#ifdef Q\nb\n#endif\nc\n","a c");
    PREN("#define N 1\n#ifdef N\nx\n#endif\n","x",0);
    /* messages */
    PREN("#error stop it  now\nok\n","ok",1);
    CHECK(D.kinds[0]==PpMsgError && D.lines[0]==1 && !strcmp(D.texts[0],"stop it  now"));
    PREN("a\n#warning careful\nb\n","a b",1);
    CHECK(D.kinds[0]==PpMsgWarning && D.lines[0]==2 && !strcmp(D.texts[0],"careful"));
    PREN("#ifdef A\n#error no\n#endif\n","",0);
    PREN("#else\n","",1); CHECK(D.lines[0]==1 && !strcmp(D.texts[0],"#else without #if"));
    PREN("#endif\nx\n","x",1);
    PREN("#ifdef A\n#else\n#else\n#endif\n","",1); CHECK(D.lines[0]==3 && !strcmp(D.texts[0],"#else after #else"));
    PREN("x\n#ifdef A\n y\n","x",1); CHECK(D.lines[0]==2 && !strcmp(D.texts[0],"conditional not closed"));
    PREN("#ifdef A\n#ifdef B\n#endif\n","",1);
    PREN("#define\nx\n","x",1); CHECK(!strcmp(D.texts[0],"bad #define"));              /* the next line is not eaten */
    PREN("#define 1x y\nx\n","x",1);
    PREN("#define F(a b) x\nF\n","F",1);
    PREN("#define F(a\nF\n","F",1);
    PREN("#undef\nx\n","x",1); PREN("#undef 3\nx\n","x",1);
    PREN("#ifdef\nx\n#endif\n","",1);
    PREN("#ifdef 3\nx\n#else\ny\n#endif\n","y",1);
    PREN("#if 1\nx\n#endif\ny\n","x y",0);
    PREN("#if 1 +\nx\n#endif\ny\n","y",1); CHECK(D.lines[0]==1);
    PREN("#include <x>\nz\n","z",1);
    PREN("#+\nz\n","z",1); CHECK(!strcmp(D.texts[0],"bad directive"));
    /* the hook: #pragma and the unknown */
    PREN("#pragma foo\nx\n","x",0); CHECK(D.dirs==1 && D.lastWord==PpWord(Pragma) && D.lastId!=0);
    PREN("#pragma\nx\n","x",0); CHECK(D.dirs==1 && D.lastId==0);
    D.take = 0;
    PREN("#bogus a b\nx\n","x",1); CHECK(!strcmp(D.texts[0],"unknown directive") && D.dirs==1 && D.lastWord==0);
    { static struct TlsPpHook H = {DMessage,DDirective,0};
      TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
      memset(&D,0,sizeof D); D.TlsPpHook.Static = &H; D.take = 1; cx->Hook = &D.TlsPpHook;
      CHECK(!strcmp(Run2(TlsStringBuffer((unsigned char *)"#import a b\nx\n"),cx,0),"x") && D.n==0 && D.dirs==1);   /* the language's own */
    }
    /* the ends of line */
    { char *r = Pre("a\n#define X 1\nX\n",PpNl); CHECK(!strcmp(r,"a||1|")); }
    { char *r = Pre("a\n#ifdef Q\nb\n#endif\nc\n",PpNl); CHECK(!strcmp(r,"a||||c|")); }        /* a line that is not read keeps its end: the lines stay where they are */
    /* lines of the original in the messages */
    { TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
      static struct TlsPpHook H = {DMessage,0,0};
      TlsCpp0Hook *map = TlsLineMapNew(NULL);
      TlsBuffer0 *f = TlsCpp0Filter(TlsStringBuffer((unsigned char *)"a /* x\ny */ b \\\nc\n#error here\n#define F(x) \\\n x\n#warning there\n"),Cpp0Default,map);
      TlsPp *p = TlsPpNew(cx,f,"m.c",map,0);
      memset(&D,0,sizeof D); D.TlsPpHook.Static = &H; cx->Hook = &D.TlsPpHook;
      while (x_(p,Next)) ;
      CHECK(D.n==2 && D.lines[0]==4 && D.lines[1]==7);
    }
}
/* directives in pieces */
static void TestDirSplit(int seed) {
    static char const *Text[] = {
        "#define A 1\n#ifdef A\nyes A\n#else\nno\n#endif\n#undef A\n#ifndef A\n#define F(x,y) x+y\nF(1,2)\n#endif\n#ifdef Z\n#bogus don't\n#else\nz\n#endif\n",
        "  # define  S(x)  #x \n#ifdef S\nS(a b) #\n#endif\n#error er ror\n#pragma foo bar\n#ifdef Q\n#ifndef R\n#else\n#endif\n#endif\n",
    };
    int ti,it;
    srand((unsigned)seed);
    for (ti=0;ti<2;ti++) {
        char const *s = Text[ti];
        int len = (int)strlen(s);
        static char Ref[100000];
        strcpy(Ref,Pre(s,PpNl));
        for (it=0;it<200;it++) {
            TlsStackNod *l = TlsHeapLayer();
            TlsBuffer0 *pc[128];
            char *cp[128];
            int np = 0,at = 0;
            TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
            while (at<len && np<120) {
                int k = rand()%(it<100 ? 3 : 9);
                if (at+k>len) k = len-at;
                cp[np] = malloc((size_t)k+1);
                memcpy(cp[np],s+at,(size_t)k);
                pc[np] = TlsBufferBuffer(cp[np],cp[np]+k);
                np++; at += k;
            }
            if (at<len) { cp[np] = strdup(s+at); pc[np] = TlsBufferBuffer(cp[np],cp[np]+strlen(s+at)); np++; }
            if (strcmp(Run2(TlsBufferCat(pc,np),cx,PpNl),Ref)) { printf("dir split %d/%d: [%s] vs [%s]\n",ti,it,Out,Ref); exit(1); }
            { int j; for (j=0;j<np;j++) free(cp[j]); }
            TlsHeapLayerPop(l);
        }
    }
}

/* the same text in pieces anywhere: the same result */
static void TestSplit(int seed) {
    static char const *Text[] = {
        "ADD(1,2) F(ADD(N,3)) S(a \"q\" b) CAT(foo,bar_baz) V(g,1,(2,3),4) A X N(x) E() TWO( a ,\n b ) W xW\n",
        "ADD ( 1 , 2 )ADD(F(F(1)),CAT(1,2)) #x ##y .5e+2 0x1p-3 'c' \"s\\\"s\" ... .. N\n",
        "#define AA 1\n#ifdef AA\nyes AA\n#else\nno 'q \" a long inactive line to cross pieces ///\n#endif\n#ifndef BB\n#define BB(x) x##x\nBB(7)\n# define CC two words\n#undef AA\nAA CC\n#frob\n#pragma w\n  #  ifdef CC\n#else\n#error not here\n#endif\n#endif\nend",
    };
    static char const *Defs[] = {"N 42","ADD(x,y) x + y","F(x) G(x)+1","G(y) y*2","A A+1","X Y","Y X","S(x) #x","CAT(a,b) a##b","V(f,...) f(__VA_ARGS__)","E(x) [x]","TWO(a,b) a b","W  spaced   body ",0};
    int ti,it;
    srand((unsigned)seed);
    for (ti=0;ti<3;ti++) {
        char const *s = Text[ti];
        int len = (int)strlen(s);
        static char Ref[100000];
        strcpy(Ref,Ex(Defs,s));
        for (it=0;it<200;it++) {
            TlsStackNod *l = TlsHeapLayer();
            TlsBuffer0 *pc[64];
            char *cp[64];
            int np = 0,at = 0;
            TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
            TlsMacroLib *lib = cx->Macros;
            char const **df;
            for (df=Defs;*df;df++) x_(lib,DefineStr,*df);
            while (at<len && np<60) {
                int k = rand()%(it<100 ? 3 : 9);
                if (at+k>len) k = len-at;
                cp[np] = malloc((size_t)k+1);
                memcpy(cp[np],s+at,(size_t)k);
                pc[np] = TlsBufferBuffer(cp[np],cp[np]+k);
                np++; at += k;
            }
            if (at<len) { cp[np] = strdup(s+at); pc[np] = TlsBufferBuffer(cp[np],cp[np]+strlen(s+at)); np++; }
            if (strcmp(Run2(TlsBufferCat(pc,np),cx,0),Ref)) { printf("split %d/%d: [%s] vs [%s]\n",ti,it,Out,Ref); exit(1); }
            { int j; for (j=0;j<np;j++) free(cp[j]); }
            TlsHeapLayerPop(l);
        }
    }
}
/* the room of the expansions comes back: many calls, no growth */
static long Rss(void) {
    long a = 0,b = 0;
    FILE *f = fopen("/proc/self/statm","r");
    if (!f) return 0;
    if (fscanf(f,"%ld %ld",&a,&b)!=2) b = 0;
    fclose(f);
    return b*4;                                          /* KB */
}
static void TestMemory(void) {
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    TlsMacroLib *lib = cx->Macros;
    TlsPp *p;
    long r0,r1;
    int i,c;
    long n = 0;
    char *text;
    int per = (int)strlen("MAX(alpha_beta,gamma+delta) ");
    int reps = 200000;
    x_(lib,DefineStr,"MAX(a,b) ((a)>(b)?(a):(b)) /* a big enough body to show */ + #a + S(a,b,a,b)");
    x_(lib,DefineStr,"S(a,b,c,d) a##b##c##d");
    text = malloc((size_t)per*(size_t)reps+1);
    for (i=0;i<reps;i++) memcpy(text+(size_t)i*(size_t)per,"MAX(alpha_beta,gamma+delta) ",(size_t)per);
    text[(size_t)per*(size_t)reps] = 0;
    p = TlsPpNew(cx,TlsBufferBuffer(text,text+(size_t)per*(size_t)reps),NULL,NULL,0);
    for (i=0;i<20000/ 4;i++) x_(p,Next);                 /* warm up */
    r0 = Rss();
    while ((c = x_(p,Next))!=TokNil) n++;
    r1 = Rss();
    printf("expansions: %ld tokens, rss growth %ld KB\n",n,r1-r0);
    CHECK(n>reps*30L);
    if (OutChunk>=256) CHECK(r1-r0<2048);              /* (tokens longer than a chunk are copied on the heap) */
    CHECK(cx->Errors==0);
    free(text);
}
int main(void) {
    TlsStackNod *l;
    int s;
    TlsMainHeapOpen(1<<20);
    l = TlsHeapLayer(); TestExpand(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestPreExpand(); TestErrors(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestLib(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestMessages(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestDirectives(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestLine(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestIf(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestDirectivesMore(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestDirectiveErrors(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestNl(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestPlaces(); TlsHeapLayerPop(l);
    for (s=1;s<=3;s++) { l = TlsHeapLayer(); TestSplit(s); TlsHeapLayerPop(l); }
    for (s=1;s<=3;s++) { l = TlsHeapLayer(); TestDirSplit(s); TlsHeapLayerPop(l); }
    l = TlsHeapLayer(); TestMemory(); TlsHeapLayerPop(l);
    printf("all passed\n");
    return 0;
}
