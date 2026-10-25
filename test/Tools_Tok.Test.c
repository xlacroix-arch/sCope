/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Tok.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

typedef struct { int code,spc,len; unsigned long val,pos,end; unsigned int line; char txt[200]; } Rec;

/* all the tokens of a reader, and the rest of line after each token 'R' (a
 * label named REST): it is how Rest is exercised */
static int Run(TlsBuffer0 *b,TlsDict *d,Rec *r,int max) {
    TlsTok *t = TlsTokNew(b,d,NULL);
    int n = 0;
    for (;n<max;) {
        int c = x_(t,Next);
        Rec *q = &r[n++];
        q->code = c; q->spc = t->Spc; q->len = t->Len; q->val = t->Val; q->pos = t->Pos; q->end = t->End; q->line = t->Line;
        CHECK(t->Len<190);
        memcpy(q->txt,t->Txt,(size_t)t->Len); q->txt[t->Len] = 0;
        if (c==TokNil) break;
        if (c==TokLabel && t->Len==4 && !memcmp(t->Txt,"REST",4)) {
            int k;
            unsigned char *s = x_(t,Rest,&k);
            q = &r[n++];
            q->code = -1; q->spc = t->Spc; q->len = k; q->val = 0; q->pos = t->Pos; q->end = t->End; q->line = t->Line;
            memcpy(q->txt,s,(size_t)k); q->txt[k] = 0;
        }
    }
    CHECK(n<max);
    return n;
}
static int SameRec(Rec *a,Rec *b) {
    return a->code==b->code && a->spc==b->spc && a->len==b->len && a->val==b->val && a->pos==b->pos && a->end==b->end && a->line==b->line && !strcmp(a->txt,b->txt);
}

static Rec R[2000];
static int N;
static void Lex(char const *s) {
    TlsDict *d = TlsDictNew(NULL);
    N = Run(TlsStringBuffer((unsigned char *)s),d,R,2000);
}
#define IS(i,cd,tx) (R[i].code==(cd) && !strcmp(R[i].txt,tx))

static void TestBasic(void) {
    TlsDict *d = TlsDictNew(NULL);
    int a,b;
    Lex("");
    CHECK(N==1 && R[0].code==TokNil);
    Lex("  foo _bar9  foo\n");
    CHECK(N==5 && IS(0,TokLabel,"foo") && IS(1,TokLabel,"_bar9") && IS(2,TokLabel,"foo") && R[3].code==TokNl && R[4].code==TokNil);
    CHECK(R[0].val==R[2].val && R[0].val!=R[1].val && R[0].val==1 && R[1].val==2);
    CHECK(!R[0].spc==0 && R[1].spc==1 && R[0].pos==2 && R[0].end==5 && R[1].pos==6 && R[2].pos==13 && R[3].pos==16 && R[3].end==17);
    CHECK(R[0].line==1 && R[3].line==1 && R[4].line==2);
    Lex("a+b");
    CHECK(R[1].code=='+' && R[1].spc==0 && R[2].spc==0 && N==4);
    Lex("a - b");
    CHECK(R[1].code=='-' && R[1].spc==1 && R[2].spc==1);
    Lex("-> ++ -- << >> <= >= == != && || :: ... ## += -= *= /= %= &= |= ^= <<= >>=");
    CHECK(N==25);
    {   int e[24] = {TokOp(Arrow),TokOp(Inc),TokOp(Dec),TokOp(Shl),TokOp(Shr),TokOp(Le),TokOp(Ge),TokOp(Eq),TokOp(Ne),TokOp(AndAnd),TokOp(OrOr),TokOp(Scope),TokOp(Ellipsis),TokOp(Paste),TokOp(AddEq),TokOp(SubEq),TokOp(MulEq),TokOp(DivEq),TokOp(ModEq),TokOp(AndEq),TokOp(OrEq),TokOp(XorEq),TokOp(ShlEq),TokOp(ShrEq)};
        int i;
        for (i=0;i<24;i++) CHECK(R[i].code==e[i]);
    }
    CHECK(IS(0,TokOp(Arrow),"->") && IS(12,TokOp(Ellipsis),"...") && IS(22,TokOp(ShlEq),"<<=") && IS(23,TokOp(ShrEq),">>="));
    Lex("(){}[];,~?@\\`");
    CHECK(N==14 && R[0].code=='(' && R[9].code=='?' && R[10].code=='@' && R[11].code=='\\' && R[12].code=='`');
    Lex("a..b .. . ...x");
    CHECK(IS(1,'.',".") && IS(2,'.',".") && R[1].pos==1 && R[2].pos==2 && R[2].end==3 && IS(3,TokLabel,"b") && IS(4,'.',".") && IS(5,'.',".") && R[5].pos==6 && R[6].pos==8 && IS(6,'.',".") && IS(7,TokOp(Ellipsis),"...") && R[7].pos==10 && R[7].end==13);
    Lex("\x01\x7f x");
    CHECK(R[0].code==TokStray && R[0].val==1 && R[1].code==0x7f);
    (void)d; (void)a; (void)b;
}
static void TestNum(void) {
    Lex("0 7 42 0x1F 0b101 017 1'000'000 10UL 3u 0xFFFFFFFFul");
    CHECK(N==11);
    CHECK(R[0].code==TokInt && R[0].val==0 && R[1].val==7 && R[2].val==42 && R[3].val==31 && R[4].val==5 && R[5].val==15 && R[6].val==1000000 && R[7].val==10 && R[8].val==3 && R[9].val==0xFFFFFFFFul);
    CHECK(IS(6,TokInt,"1'000'000") && IS(3,TokInt,"0x1F"));
    Lex("1.5 .5 1e3 1.5e-3f 0x1p+3 1E+9 0xe+1 12e 1.");
    CHECK(R[0].code==TokFloat && IS(1,TokFloat,".5") && IS(2,TokFloat,"1e3") && IS(3,TokFloat,"1.5e-3f") && IS(4,TokFloat,"0x1p+3") && IS(5,TokFloat,"1E+9"));
    CHECK(IS(6,TokInt,"0xe") && R[6].val==14 && R[7].code=='+' && IS(8,TokInt,"1") && R[9].code==TokFloat && IS(9,TokFloat,"12e") && IS(10,TokFloat,"1."));
    Lex("1+2 1-2");
    CHECK(N==7 && IS(0,TokInt,"1") && R[1].code=='+' && R[4].code=='-');
}
static void TestStr(void) {
    Lex("\"abc\" 'a' '\\n' '\\x41' '\\101' 'ab' \"a\\\"b\" \"\" '' x\"\\\\\"y");
    CHECK(IS(0,TokStr,"\"abc\"") && R[1].code==TokChar && R[1].val=='a' && R[2].val==10 && R[3].val==0x41 && R[4].val==65 && R[5].val=='a'*256+'b');
    CHECK(IS(6,TokStr,"\"a\\\"b\"") && IS(7,TokStr,"\"\"") && IS(8,TokChar,"''") && R[8].val==0);
    CHECK(IS(9,TokLabel,"x") && IS(10,TokStr,"\"\\\\\"") && IS(11,TokLabel,"y"));
    Lex("\"open\nx");
    CHECK(IS(0,TokStr,"\"open") && R[1].code==TokNl && IS(2,TokLabel,"x") && R[0].end==5);
    Lex("\"open");
    CHECK(IS(0,TokStr,"\"open") && R[1].code==TokNil);
    Lex("\"a\\");
    CHECK(IS(0,TokStr,"\"a\\") && R[1].code==TokNil);
    Lex("\"a\\\nx");
    CHECK(IS(0,TokStr,"\"a\\") && R[1].code==TokNl);
    Lex("#include <stdio.h> // x\n#define A 1");
    CHECK(R[0].code=='#' && IS(1,TokLabel,"include") && R[2].code=='<');
}
static void TestRest(void) {
    Lex("#error REST   don't  stop here \nx REST\ny REST ..\nz REST");
    /* # error REST <rest> NL x REST <rest> NL ... */
    CHECK(R[3].code==-1 && !strcmp(R[3].txt,"don't  stop here ") && R[4].code==TokNl && IS(5,TokLabel,"x"));
    CHECK(R[7].code==-1 && R[7].len==0 && R[8].code==TokNl);
    CHECK(IS(9,TokLabel,"y") && R[11].code==-1 && !strcmp(R[11].txt,"..") && R[12].code==TokNl);
    CHECK(R[15].code==-1 && R[15].len==0 && R[16].code==TokNil);
    {   TlsDict *d = TlsDictNew(NULL);
        TlsTok *t = TlsTokNew(TlsStringBuffer((unsigned char *)"a REST skipped words\nb"),d,NULL);
        int k;
        CHECK(x_(t,Next)==TokLabel && x_(t,Next)==TokLabel);
        x_(t,Rest,NULL); (void)k;
        CHECK(x_(t,Next)==TokNl && x_(t,Next)==TokLabel && t->Line==2 && x_(t,Next)==TokNil);
        x_(t,Reset);
        CHECK(t->Line==1 && x_(t,Next)==TokLabel && t->Pos==0 && t->End==1);
    }
}

/* the same text cut in pieces anywhere, with empty pieces, must give the same tokens */
static char const *Corpus[] = {
    "#define F(x,y) x##y + (x<<=3) // c\n  foo_bar  123'456 0x1p-3 .5e+2 \"str\\\"ing\" 'c' '\\x41' ... .. -> ->* x\n",
    "#error REST don't stop  \n a REST \nlongerlabel_that_crosses_pieces 12345678901234567890 1.5e+10f \"a long string with \\\\ and \\\" inside\" end",
    "\"unterminated\n'\n.\n..\n...\n1e+\n1e\n0x\n+++---<<<>>>===!!&&&|||::::###\n",
    "x\"abc\"y'q'\"\"''.5.5.5 1.2.3 a.b a->b a<b>c\n\n\n\n",
};
static void TestSplit(int seed) {
    int ci,it;
    srand((unsigned)seed);
    for (ci=0;ci<(int)(sizeof Corpus/sizeof *Corpus);ci++) {
        char const *s = Corpus[ci];
        int len = (int)strlen(s);
        static Rec A[2000],B[2000];
        int na,nb,i;
        TlsDict *d1 = TlsDictNew(NULL),*d2 = TlsDictNew(NULL);
        na = Run(TlsStringBuffer((unsigned char *)s),d1,A,2000);
        for (it=0;it<300;it++) {
            TlsBuffer0 *pc[64];
            char *copy[64];
            int np = 0,at = 0;
            TlsStackNod *l = TlsHeapLayer();
            while (at<len && np<60) {
                int k = rand()%(it<100 ? 3 : 9),m;
                if (k==0 && rand()%3) { copy[np] = NULL; pc[np++] = TlsBufferBuffer((char *)s,(char *)s); continue; }
                if (at+k>len) k = len-at;
                m = k;
                copy[np] = malloc((size_t)m+1);
                memcpy(copy[np],s+at,(size_t)m);
                pc[np] = TlsBufferBuffer(copy[np],copy[np]+m);
                np++;
                at += k;
            }
            if (at<len) {
                copy[np] = strdup(s+at);
                pc[np] = TlsBufferBuffer(copy[np],copy[np]+strlen(s+at));
                np++;
            }
            nb = Run(TlsBufferCat(pc,np),d2,B,2000);
            if (na!=nb) printf("corpus %d iter %d: %d vs %d np=%d\n",ci,it,na,nb,np);
            if (na!=nb) { int j; for (j=0;j<na||j<nb;j++) printf("%d: %d[%s]@%lu  %d[%s]@%lu\n",j,j<na?A[j].code:-9,j<na?A[j].txt:"",j<na?A[j].pos:0,j<nb?B[j].code:-9,j<nb?B[j].txt:"",j<nb?B[j].pos:0); }
            CHECK(na==nb);
            for (i=0;i<na;i++) if (!SameRec(&A[i],&B[i])) {
                printf("corpus %d iter %d token %d: %d/%s vs %d/%s\n",ci,it,i,A[i].code,A[i].txt,B[i].code,B[i].txt);
                exit(1);
            }
            { int j; for (j=0;j<np;j++) if (copy[j]) free(copy[j]); }
            TlsHeapLayerPop(l);
            d2 = TlsDictNew(NULL);
        }
    }
}
int main(void) {
    TlsStackNod *l;
    int s;
    TlsMainHeapOpen(1<<20);
    l = TlsHeapLayer(); TestBasic(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestNum(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestStr(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestRest(); TlsHeapLayerPop(l);
    for (s=1;s<=5;s++) { l = TlsHeapLayer(); TestSplit(s); TlsHeapLayerPop(l); }
    printf("all passed\n");
    return 0;
}
