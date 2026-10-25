/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_OutBuf.h>
#include <Tools_Dict.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static void TestOut(void) {
    TlsOutBuf *o = TlsOutBufNew(NULL);
    unsigned char ref[5000],*t;
    int i,n,round;
    CHECK(TlsOutLen(o)==0);
    for (round=0;round<3;round++) {
        x_(o,Reset);
        for (i=0;i<5000;i++) { ref[i] = (unsigned char)(1+i%250); TlsOutPut(o,ref[i]); }
        CHECK(TlsOutLen(o)==5000);
        t = x_(o,Take,&n);
        CHECK(n==5000 && !memcmp(t,ref,5000) && t[5000]==0);
    }
    x_(o,Reset);
    TlsOutWrite(o,"abc",3);
    TlsOutWrite(o,ref,4000);
    TlsOutWrite(o,"",0);
    CHECK(TlsOutLen(o)==4003);
    t = x_(o,Take,&n);
    CHECK(n==4003 && !memcmp(t,"abc",3) && !memcmp(t+3,ref,4000));
    for (i=0;i<=3*OutChunk;i++) {       /* sizes around the chunk edges */
        x_(o,Reset);
        TlsOutWrite(o,ref,i);
        t = x_(o,Take,&n);
        CHECK(n==i && TlsOutLen(o)==i && !memcmp(t,ref,i) && t[i]==0);
    }
    x_(o,Reset);
    t = x_(o,Take,NULL);
    CHECK(t[0]==0);
}
static void TestDict(void) {
    TlsDict *d = TlsDictNew(NULL);
    char buf[32];
    int i,n,N = 20000;
    unsigned char *s;
    CHECK(x_(d,Count)==0 && x_(d,Find,(unsigned char *)"a",1)==0);
    CHECK(x_(d,Name,0,&n)==NULL && x_(d,Name,1,&n)==NULL);
    CHECK(x_(d,Id,(unsigned char *)"",0)==1);
    for (i=0;i<N;i++) {
        n = sprintf(buf,"lbl%d",i);
        CHECK(x_(d,Id,(unsigned char *)buf,n)==i+2);
    }
    CHECK(x_(d,Count)==N+1);
    for (i=0;i<N;i++) {
        n = sprintf(buf,"lbl%d",i);
        CHECK(x_(d,Id,(unsigned char *)buf,n)==i+2);
        CHECK(x_(d,Find,(unsigned char *)buf,n)==i+2);
        s = x_(d,Name,i+2,&n);
        CHECK(s && n==(int)strlen(buf) && !strcmp((char *)s,buf));
    }
    CHECK(x_(d,Find,(unsigned char *)"nope",4)==0 && x_(d,Count)==N+1);
    CHECK(x_(d,Name,N+2,&n)==NULL && x_(d,Name,-1,&n)==NULL);
    s = x_(d,Name,1,&n);
    CHECK(n==0 && s[0]==0);
    CHECK(x_(d,Find,(unsigned char *)"lbl1x",4)==x_(d,Find,(unsigned char *)"lbl1",4));
}
int main(void) {
    TlsStackNod *l;
    TlsMainHeapOpen(65536);
    l = TlsHeapLayer(); TestOut(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestDict(); TlsHeapLayerPop(l);
    printf("all passed\n");
    return 0;
}
