/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Tok.h>
#include <Tools_LineMap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

/* Lines that start with a tag Ln (n: its original line), then continuations and
 * multi line comments. The first token of each filtered line is its tag. */
static char Src[200000];
static int Gen(int nlines,unsigned seed,int *firstlines) {
    char *w = Src;
    int line = 1,k;
    srand(seed);
    for (k=0;k<nlines;k++) {
        int parts = rand()%4,j;
        firstlines[k] = line;
        w += sprintf(w,"L%d",line);
        for (j=0;j<parts;j++) switch (rand()%4) {
        case 0: w += sprintf(w," x \\\n y"); line++; break;
        case 1: w += sprintf(w," /* c\n c2\n */ z"); line += 2; break;
        case 2: w += sprintf(w," // note"); break;
        default: w += sprintf(w," a+b"); break;
        }
        w += sprintf(w,"\n"); line++;
        if (rand()%5==0) { w += sprintf(w,"\n"); line++; }        /* an empty line has no tag: the next tag is a few lines on */
    }
    *w = 0;
    return (int)(w-Src);
}
static void TestMap(int nlines,unsigned seed) {
    static int first[5000];
    TlsStackNod *l = TlsHeapLayer();
    TlsDict *d = TlsDictNew(NULL);
    TlsCpp0Hook *m = TlsLineMapNew(NULL);
    TlsBuffer0 *f;
    TlsTok *t;
    int n = Gen(nlines,seed,first),seen = 0,lastline = 0,pass;
    (void)n;
    f = TlsCpp0Filter(TlsStringBuffer((unsigned char *)Src),Cpp0Default,m);
    t = TlsTokNew(f,d,NULL);
    for (pass=0;pass<2;pass++) {
        seen = 0; lastline = 0;
        for (;;) {
            int c = x_(t,Next);
            if (c==TokNil) break;
            if ((int)t->Line!=lastline && c==TokLabel && t->Txt[0]=='L') {   /* the first token of a line */
                char num[32];
                int tag;
                memcpy(num,t->Txt+1,(size_t)t->Len-1); num[t->Len-1] = 0;      /* Txt is cut at Len */
                tag = atoi(num);
                CHECK(seen<nlines && tag==first[seen]);
                CHECK(TlsLineMapOrig(m,t->Line)==(unsigned)tag);
                seen++;
            }
            lastline = (int)t->Line;
        }
        CHECK(seen==nlines);
        if (!pass) x_(t,Reset);                          /* the marks come again */
    }
    TlsHeapLayerPop(l);
}
static void TestNone(void) {
    TlsStackNod *l = TlsHeapLayer();
    TlsCpp0Hook *m = TlsLineMapNew(NULL);
    TlsBuffer0 *f = TlsCpp0Filter(TlsStringBuffer((unsigned char *)"a\nb\nc\n"),Cpp0Default,m);
    TlsTok *t = TlsTokNew(f,TlsDictNew(NULL),NULL);
    unsigned int line,w;
    char *msg;
    while (x_(t,Next)) ;
    CHECK(TlsLineMapOrig(m,1)==1 && TlsLineMapOrig(m,3)==3 && TlsLineMapOrig(m,99)==99);
    CHECK(TlsLineMapWarns(m,&line,&msg)==0);
    TlsHeapLayerPop(l);
    l = TlsHeapLayer();
    m = TlsLineMapNew(NULL);
    f = TlsCpp0Filter(TlsStringBuffer((unsigned char *)"a\n\n/* never closed\n\n"),Cpp0Default,m);
    t = TlsTokNew(f,TlsDictNew(NULL),NULL);
    while (x_(t,Next)) ;
    w = (unsigned)TlsLineMapWarns(m,&line,&msg);
    CHECK(w==1 && line==3 && !strcmp(msg,"unterminated comment"));
    TlsHeapLayerPop(l);
}
int main(void) {
    int s;
    TlsMainHeapOpen(1<<22);
    TestNone();
    for (s=1;s<=20;s++) TestMap(s<10 ? s*7 : 1500,(unsigned)s);       /* 1500 lines: many chunks of marks */
    printf("all passed\n");
    return 0;
}
