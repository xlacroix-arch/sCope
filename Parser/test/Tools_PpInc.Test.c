/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Pp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static char Root[64];
static void Put(char const *rel,char const *text) {
    char path[400],*sl;
    FILE *f;
    snprintf(path,sizeof path,"%s/%s",Root,rel);
    sl = strrchr(path,'/');
    *sl = 0;
    { char cmd[500]; snprintf(cmd,sizeof cmd,"mkdir -p %s",path); CHECK(!system(cmd)); }
    *sl = '/';
    f = fopen(path,"w");
    CHECK(f);
    fputs(text,f);
    fclose(f);
}
static char *P(char const *rel) {
    static char b[4][400];
    static int i;
    i = (i+1)&3;
    snprintf(b[i],sizeof b[i],"%s/%s",Root,rel);
    return b[i];
}
static int OpenFds(void) {
    int n = 0;
    DIR *d = opendir("/proc/self/fd");
    while (readdir(d)) n++;
    closedir(d);
    return n;
}

typedef struct { TlsPpHook TlsPpHook; int n,opens; int kinds[32]; unsigned int lines[32]; char texts[32][80],files[32][400],openp[16][400]; unsigned int openl[16]; } Rec;
static void RMessage(TlsPpHook *this,int Kind,char const *File,unsigned int Line,char const *Text) {
    Rec *r = (Rec *)this;
    if (r->n<32) { r->kinds[r->n] = Kind; r->lines[r->n] = Line; strncpy(r->texts[r->n],Text,79); strncpy(r->files[r->n],File ? File : "(none)",399); }
    r->n++;
}
static void ROpen(TlsPpHook *this,char const *File,unsigned int Line,char const *Path) {
    Rec *r = (Rec *)this;
    (void)File;
    if (r->opens<16) { strncpy(r->openp[r->opens],Path,399); r->openl[r->opens] = Line; }
    r->opens++;
}
static Rec R;
static char Out[100000];
static TlsPpCtx *NewCtx(void) {
    static struct TlsPpHook H = {RMessage,0,ROpen};
    TlsPpCtx *cx = TlsPpCtxNew(NULL,NULL);
    memset(&R,0,sizeof R);
    R.TlsPpHook.Static = &H;
    cx->Hook = &R.TlsPpHook;
    return cx;
}
/* the tokens, with the file and the original line of each as  text@file:line  when asked */
static char *Read(TlsPp *p,int where) {
    char *w = Out;
    int c,first = 1;
    while ((c = x_(p,Next))!=TokNil) {
        if (!first && p->Spc) *w++ = ' ';
        first = 0;
        if (c==TokNl) *w++ = '|';
        else { memcpy(w,p->Txt,(size_t)p->Len); w += p->Len; }
        if (where) {
            char const *f,*sl;
            unsigned int l;
            x_(p,Where,&f,&l);
            sl = f ? strrchr(f,'/') : 0;
            w += sprintf(w,"@%s:%u",sl ? sl+1 : f ? f : "-",l);
        }
    }
    *w = 0;
    return Out;
}
#define EQ(pp,where,res) do { char *r_ = Read(pp,where); if (strcmp(r_,res)) { printf("line %d: gave [%s], wanted [%s]\n",__LINE__,r_,res); exit(1); } } while (0)

static void Setup(void) {
    snprintf(Root,sizeof Root,"/tmp/ppincXXXXXX");
    CHECK(mkdtemp(Root));
    Put("main.c","#define BEFORE 1\nstart\n#include \"a.h\"\nmid BEFORE FROM_A\n#include <lib/b.h>\n#include <lib/b.h>\nend\n");
    Put("a.h","#ifndef A_H\n#define A_H\n#define FROM_A 10\na1 BEFORE\n#include \"sub/c.h\"\n#endif\n");
    Put("sub/c.h","c1 \\\n c2\n#include \"../a.h\"\n");
    Put("inc/lib/b.h","#pragma once\nb1\n#include \"d.h\"\n");
    Put("inc/lib/d.h","\n\n/* c\n c */\nd1 /* x\n y */ d2\n");
    Put("bad.c","x\n#include \"unclosed.h\"\ny\n");
    Put("unclosed.h","#ifdef X\nstill\n");
    Put("rec.h","r\n#include \"rec.h\"\n");
    Put("cases.c","#include foo\n#include \"nope\n#include <>\n#include \"missing.h\"\n#include <missing.h>\n#include\nz\n");
    Put("nl.c","a\n#include \"x.h\"\nb\n");
    Put("x.h","X\n");
    Put("inside.c","#define M(x) [x]\n#include \"useM.h\"\nM(2)\n#undef M\n");
    Put("useM.h","M(1) #ifdef-not\n#ifdef M\nM(yes)\n#endif\n#define FROM_H 5\n");
    Put("both.c","#include \"x.h\"\n#include <x.h>\n");
    Put("inc/x.h","FROM_PATH\n");
    Put("lineinc.c","a __LINE__ __FILE__\n#line 100 \"fake.c\"\nb __LINE__ __FILE__\n#include \"li.h\"\nc __LINE__ __FILE__\n");
    Put("li.h","x __LINE__ __FILE__\n#line 7 \"li2.h\"\ny __LINE__ __FILE__\n");
    Put("lw.c","x\n#line 40 \"z.c\"\ny\n#line 9 \"q.c\"\n#error e\n#line 3\nw\n");
    Put("empty.c","");
    Put("onlynl.h","\n\n");
    Put("withempty.c","a\n#include \"empty.c\"\n#include \"onlynl.h\"\nb\n");
}
static void TestBasic(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p;
    int fds = OpenFds();
    x_(cx,AddPath,P("inc"));
    p = TlsPpNewFile(cx,P("main.c"),0);
    CHECK(p);
    EQ(p,0,"start a1 1 c1 c2 mid 1 10 b1 d1 d2 end");           /* a.h once (its guard), b.h once (#pragma once) */
    CHECK(OpenFds()==fds && cx->Errors==0 && cx->Warnings==0);
    CHECK(R.opens==6 && !strcmp(R.openp[0],P("main.c")) && R.openl[0]==0);
    CHECK(!strcmp(R.openp[1],P("a.h")) && R.openl[1]==3);                      /* the line of the #include */
    CHECK(!strcmp(R.openp[2],P("sub/c.h")) && R.openl[2]==5);
    CHECK(!strcmp(R.openp[3],P("sub/../a.h")));                                       /* opened again, and read to its guard */
    CHECK(!strcmp(R.openp[4],P("inc/lib/b.h")) && R.openl[4]==5);
    CHECK(!strcmp(R.openp[5],P("inc/lib/d.h")) && R.openl[5]==3);
    CHECK(x_(cx,OnceHas,P("inc/lib/b.h")) && !x_(cx,OnceHas,P("a.h")));
}
static void TestWhere(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p;
    x_(cx,AddPath,P("inc"));
    p = TlsPpNewFile(cx,P("main.c"),0);
    EQ(p,1,"start@main.c:2 a1@a.h:4 1@a.h:4 c1@c.h:1 c2@c.h:1 mid@main.c:4 1@main.c:4 10@main.c:4 b1@b.h:2 d1@d.h:5 d2@d.h:5 end@main.c:7");
}
static void TestNl(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p = TlsPpNewFile(cx,P("nl.c"),PpNl);
    EQ(p,0,"a|X||b|");
    cx = NewCtx();
    p = TlsPpNewFile(cx,P("withempty.c"),PpNl);
    EQ(p,0,"a|||||b|");                                           /* an empty file and one of blank lines: nothing but the line ends */
    CHECK(cx->Errors==0);
}
static void TestErrors(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p;
    int fds = OpenFds();
    p = TlsPpNewFile(cx,P("cases.c"),0);
    EQ(p,0,"z");
    CHECK(R.n==6 && cx->Errors==6);
    CHECK(!strcmp(R.texts[0],"bad #include") && R.lines[0]==1);
    CHECK(!strcmp(R.texts[1],"bad #include") && R.lines[1]==2);
    CHECK(!strcmp(R.texts[2],"empty #include name") && R.lines[2]==3);
    CHECK(!strcmp(R.texts[3],"#include file not found") && R.lines[3]==4 && !strcmp(R.texts[4],"#include file not found") && R.lines[4]==5);
    CHECK(!strcmp(R.texts[5],"bad #include") && R.lines[5]==6);
    CHECK(!strcmp(R.files[0],P("cases.c")));
    CHECK(OpenFds()==fds);
    /* a conditional left open by an included file is reported there, and does not leak out */
    cx = NewCtx();
    p = TlsPpNewFile(cx,P("bad.c"),0);
    EQ(p,0,"x y");
    CHECK(R.n==1 && !strcmp(R.texts[0],"conditional not closed") && R.lines[0]==1 && !strcmp(R.files[0],P("unclosed.h")));
    /* a file that includes itself stops at the limit, once */
    cx = NewCtx();
    p = TlsPpNewFile(cx,P("rec.h"),0);
    EQ(p,0,"r r r r r r r r r r r r r r r r r");
    CHECK(R.n==1 && !strcmp(R.texts[0],"#include nested too deep") && R.lines[0]==2 && R.opens==17);
    CHECK(OpenFds()==fds);
    CHECK(TlsPpNewFile(cx,P("nothere.c"),0)==NULL);
}
static void TestMacros(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p = TlsPpNewFile(cx,P("inside.c"),0);
    EQ(p,0,"[1] #ifdef-not [yes] [2]");                          /* the macros of the includer are there in the included file, and the other way */
    CHECK(x_(cx->Macros,Get,x_(cx->Dict,Id,(unsigned char *)"FROM_H",6))!=NULL && x_(cx->Macros,Get,x_(cx->Dict,Id,(unsigned char *)"M",1))==NULL);
}
static void TestOrder(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p;
    x_(cx,AddPath,P("inc"));
    p = TlsPpNewFile(cx,P("both.c"),0);
    EQ(p,0,"X FROM_PATH");                                       /* "x.h": the directory of the file first, <x.h>: the paths only */
}
static void TestLine(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p;
    char want[800];
    int fds = OpenFds();
    p = TlsPpNewFile(cx,P("lineinc.c"),0);
    snprintf(want,sizeof want,"a 1 \"%s\" b 100 \"fake.c\" x 1 \"%s\" y 7 \"li2.h\" c 102 \"fake.c\"",P("lineinc.c"),P("li.h"));
    EQ(p,0,want);                                       /* an included file has its own #line, and gives it back */
    CHECK(OpenFds()==fds && cx->Errors==0);
    cx = NewCtx();
    p = TlsPpNewFile(cx,P("lw.c"),0);
    EQ(p,1,"x@lw.c:1 y@z.c:40 w@q.c:3");
    CHECK(R.n==1 && R.lines[0]==9 && !strcmp(R.files[0],"q.c") && !strcmp(R.texts[0],"e"));
}
static long Rss(void) {
    long a = 0,b = 0;
    FILE *f = fopen("/proc/self/statm","r");
    if (!f) return 0;
    if (fscanf(f,"%ld %ld",&a,&b)!=2) b = 0;
    fclose(f);
    return b*4;
}
static void TestMany(void) {
    TlsPpCtx *cx = NewCtx();
    TlsPp *p;
    FILE *f;
    int i,fds = OpenFds(),c;
    long r0,r1,n = 0;
    Put("many.h","k \\\n k /* c */\n#ifndef NEVER\n  k\n#endif\n");
    f = fopen(P("many.c"),"w");
    CHECK(f);
    for (i=0;i<30000;i++) fputs("#include \"many.h\"\n",f);
    fclose(f);
    p = TlsPpNewFile(cx,P("many.c"),0);
    for (i=0;i<2000;i++) { x_(p,Next); n++; }
    r0 = Rss();
    while ((c = x_(p,Next))!=TokNil) n++;
    r1 = Rss();
    printf("includes: %ld tokens, rss growth %ld KB\n",n,r1-r0);
    CHECK(n==90000 && cx->Errors==0 && OpenFds()==fds);
    if (OutChunk>=256) CHECK(r1-r0<4096);
}
int main(void) {
    TlsStackNod *l;
    TlsMainHeapOpen(1<<20);
    Setup();
    l = TlsHeapLayer(); TestBasic(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestWhere(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestNl(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestErrors(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestMacros(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestOrder(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestLine(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestMany(); TlsHeapLayerPop(l);
    { char cmd[100]; snprintf(cmd,sizeof cmd,"rm -rf %s",Root); CHECK(!system(cmd)); }
    printf("all passed\n");
    return 0;
}
