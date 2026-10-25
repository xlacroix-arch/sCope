/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Expr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static TlsDict *D;
static int Calls;
static int EnvLabel(TlsExprEnv *this,int id,long long *v) {
    unsigned char *n = x_(D,Name,id,NULL);
    (void)this;
    Calls++;
    if (!strcmp((char *)n,"A")) { *v = 5; return 1; }
    if (!strcmp((char *)n,"B")) { *v = -3; return 1; }
    return 0;
}
static struct TlsExprEnv EnvVt = {EnvLabel};
static TlsExprEnv Env = {&EnvVt};

/* 0 and the value, or 1 and the message */
static int Ev(char const *src,long long *v,char const **err,TlsExprEnv *env) {
    TlsStackNod *l = TlsHeapLayer();
    TlsTok *t = TlsTokNew(TlsStringBuffer((unsigned char *)src),D,NULL);
    *err = TlsExprEval(t,env,v);
    TlsHeapLayerPop(l);
    return *err!=0;
}
#define VAL(s,want) do { long long v_; char const *e_; if (Ev(s,&v_,&e_,&Env) || v_!=(want)) { printf("line %d: [%s] gave %lld (%s), wanted %lld\n",__LINE__,s,v_,e_ ? e_ : "ok",(long long)(want)); exit(1); } } while (0)
#define BAD(s) do { long long v_; char const *e_; if (!Ev(s,&v_,&e_,&Env)) { printf("line %d: [%s] gave %lld, wanted an error\n",__LINE__,s,v_); exit(1); } } while (0)

static void TestFixed(void) {
    long long v;
    char const *e;
    VAL("1",1); VAL("  42  ",42); VAL("0x1F",31); VAL("'a'",97); VAL("'\\n'",10); VAL("10u",10); VAL("7L",7);
    VAL("1+2*3",7); VAL("(1+2)*3",9); VAL("10-3-2",5); VAL("100/10/5",2); VAL("7%4",3); VAL("-7/2",-3); VAL("-7%4",-3);
    VAL("2<<3",16); VAL("-16>>2",-4); VAL("1<<63<0",1); VAL("1<<64",0); VAL("1>>64",0); VAL("-1>>64",-1); VAL("1<<-1",0);
    VAL("1<2",1); VAL("2<=2",1); VAL("3>4",0); VAL("3>=4",0); VAL("1==1",1); VAL("1!=1",0); VAL("1<2<3",1); VAL("3>2>1",0);
    VAL("6&3",2); VAL("6|3",7); VAL("6^3",5); VAL("1|2&3",3); VAL("1^3&2",3); VAL("1&&2",1); VAL("0||0",0); VAL("1||0&&0",1);
    VAL("!0",1); VAL("!5",0); VAL("~0",-1); VAL("-(-3)",3); VAL("+4",4); VAL("- -4",4); VAL("!!7",1); VAL("-2*-3",6);
    VAL("1?2:3",2); VAL("0?2:3",3); VAL("1?2:0?3:4",2); VAL("0?2:0?3:4",4); VAL("0?1:1?5:6",5); VAL("(1?2:3)+1",3);
    VAL("A+B",2); VAL("A*B",-15); VAL("A==5&&B==-3",1);
    VAL("9223372036854775807+1",(long long)(-9223372036854775807LL-1)); VAL("-9223372036854775807-1-1",9223372036854775807LL);
    VAL("(-9223372036854775807-1)/-1",(long long)(-9223372036854775807LL-1)); VAL("(-9223372036854775807-1)%-1",0);
    /* what is not evaluated is not an error, nor asked for */
    Calls = 0;
    VAL("0 && 1/0",0); VAL("1 || 1/0",1); VAL("1 ? 2 : 1/0",2); VAL("0 ? 1/0 : 3",3); VAL("0 && Z",0); VAL("1 || Z",1); VAL("0 ? Z : 4",4);
    CHECK(Calls==0);
    VAL("1 || (0 && 1/0)",1); VAL("0 ? 1 ? 1/0 : 2 : 3",3);
    BAD(""); BAD("   "); BAD("1+"); BAD("*2"); BAD("(1"); BAD("1)"); BAD("1 2"); BAD("1/0"); BAD("1%0"); BAD("1 ? 2"); BAD("1 ? 2 : "); BAD("Z"); BAD("1+Z");
    BAD("1.5"); BAD("1e3"); BAD("\"s\""); BAD("1,2"); BAD("1 = 1"); BAD("1 ++"); BAD("()"); BAD("1 ? 2 : 3 : 4"); BAD("0 ? 1 : 1/0");
    BAD("1 && ");
    CHECK(Ev("A",&v,&e,NULL) && e);                       /* no env: a name is an error */
    CHECK(!Ev("1+1",&v,&e,NULL) && v==2);
    CHECK(Ev("",&v,&e,&Env) && e && *e);
    /* the end of a line ends the expression: the rest is for the caller */
    {
        TlsStackNod *l = TlsHeapLayer();
        TlsTok *t = TlsTokNew(TlsStringBuffer((unsigned char *)"1+2\nrest"),D,NULL);
        CHECK(!TlsExprEval(t,&Env,&v) && v==3);
        CHECK(x_(t,Next)==TokLabel);
        TlsHeapLayerPop(l);
    }
}

/* random expressions with the fewest parentheses, against the same thing worked out as a tree */
static char const *Ops[] = {"||","&&","|","^","&","==","!=","<",">","<=",">=","<<",">>","+","-","*","/","%"};
static int Pr[] = {1,2,3,4,5,6,6,7,7,7,7,8,8,9,9,10,10,10};
static long long Gen(char *o,int depth,int ctx) {
    int k = depth ? rand()%7 : 0;
    long long a,b;
    char tmp[2000];
    if (k==0) {
        int c = rand()%5;
        if (c==0) { strcat(o,"A"); return 5; }
        if (c==1) { strcat(o,"B"); return -3; }
        if (c==2) { strcat(o,"'a'"); return 97; }
        a = rand()%100; sprintf(tmp,"%lld",a); strcat(o,tmp); return a;
    }
    if (k==1) {
        int u = rand()%3;
        strcat(o,u==0 ? "- (" : u==1 ? "! (" : "~ (");
        a = Gen(o,depth-1,0);
        strcat(o,")");
        return u==0 ? (long long)(0-(unsigned long long)a) : u==1 ? !a : ~a;
    }
    if (k==2) {
        long long c;
        strcat(o,"(");
        c = Gen(o,depth-1,1);
        strcat(o," ? ");
        a = Gen(o,depth-1,0);
        strcat(o," : ");
        b = Gen(o,depth-1,0);
        strcat(o,")");
        return c ? a : b;
    } else {
        int op = rand()%18,p = Pr[op],par = p<ctx;
        unsigned long long ua,ub;
        if (par) strcat(o,"(");
        a = Gen(o,depth-1,p);
        strcat(o," "); strcat(o,Ops[op]); strcat(o," ");
        if (op==16 || op==17) { b = 1+rand()%9; sprintf(tmp,"%lld",b); strcat(o,tmp); }
        else if (op==11 || op==12) { b = rand()%72; sprintf(tmp,"%lld",b); strcat(o,tmp); }
        else b = Gen(o,depth-1,p+1);
        if (par) strcat(o,")");
        ua = (unsigned long long)a; ub = (unsigned long long)b;
        switch (op) {
        case 0: return a || b;   case 1: return a && b;
        case 2: return a|b;      case 3: return a^b;     case 4: return a&b;
        case 5: return a==b;     case 6: return a!=b;
        case 7: return a<b;      case 8: return a>b;     case 9: return a<=b;    case 10: return a>=b;
        case 11: return b>=64 ? 0 : (long long)(ua<<b);
        case 12: return b>=64 ? (a<0 ? -1 : 0) : a>>b;
        case 13: return (long long)(ua+ub);
        case 14: return (long long)(ua-ub);
        case 15: return (long long)(ua*ub);
        case 16: return a/b;
        default: return a%b;
        }
    }
}
static void TestFuzz(void) {
    int i;
    srand(12345);
    for (i=0;i<30000;i++) {
        char s[20000];
        long long want,v;
        char const *e;
        s[0] = 0;
        want = Gen(s,1+i%6,0);
        if (Ev(s,&v,&e,&Env) || v!=want) { printf("case %d: [%s] gave %lld (%s), wanted %lld\n",i,s,v,e ? e : "ok",want); exit(1); }
    }
}

int main(void) {
    TlsStackNod *l;
    TlsMainHeapOpen(1<<20);
    l = TlsHeapLayer();
    D = TlsDictNew(NULL);
    x_(D,Id,(unsigned char *)"A",1); x_(D,Id,(unsigned char *)"B",1); x_(D,Id,(unsigned char *)"Z",1);
    TestFixed();
    TestFuzz();
    TlsHeapLayerPop(l);
    printf("all passed\n");
    return 0;
}
