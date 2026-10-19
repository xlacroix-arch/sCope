#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <Tools_Core.h>
TlsMemPool *TlsMainPoolOpen(TlsIntCounter *c);
TlsMemPool *TlsMemPoolPush(void);
#define CHECK(c) do{ if(!(c)){ printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); exit(1);} }while(0)
static TlsHeap *Main;
static int Cnt(TlsIntCounter *c){ return x_(c,Add,0); }
 
static void Contract(TlsMemPool *p) {
    unsigned sizes[]={1,3,7,8,9,24,100,256,257,1000,4096,5000,70000};
    void *ptr[16]; int i;
    for (i=0;i<(int)(sizeof sizes/sizeof*sizes);i++) {
        unsigned sz=sizes[i];
        TlsMemPool *h = x_(p,SizeSelect,sz);
        void *m = x_(h,Allocate);
        unsigned al = (sz&(sz-1))==0?sz:(sz%8==0?8:sz%4==0?4:sz%2==0?2:1);
        if (al>4096) al=4096;
        CHECK(m && ((uintptr_t)m&(al-1))==0);
        memset(m,0xA5,sz);
        ptr[i]=m;
    }
    for (i=0;i<(int)(sizeof sizes/sizeof*sizes);i++) {
        TlsMemPool *h = x_(p,SizeSelect,sizes[i]);
        x_(h,Free,ptr[i],sizes[i]);
    }
    { TlsMemPool *z=x_(p,SizeSelect,0); void *m=x_(z,Allocate); CHECK(m); x_(z,Free,m,0); }
}
 
static void TestLayerPop(unsigned growth) {
    TlsMemPool *root; x_(Main,Set); root = TlsMainPoolOpen(0);
    TlsIntCounter *rc = TlsIntCounterDirect(0);
    TlsStack *S; TlsHeap *h; TlsStackNod *l; TlsMemPool *p; int base,i;
    x_(root,CountSwitch,rc);
    S = TlsStackOpen(root,growth,0); h = TlsStackHeap(S); x_(h,Set);
    base = Cnt(rc);
    l = TlsHeapLayer();
    p = TlsMemPoolPush();
    Contract(p);
    { void *ptr[2000]; unsigned seed=1;
      for (i=0;i<2000;i++){ seed=seed*1103515245u+12345u; unsigned sz=1+(seed>>8)%700; ptr[i]=x_(x_(p,SizeSelect,sz),Allocate); ((unsigned char*)ptr[i])[sz-1]=7; }
      /* free in odd order */
      seed=1; for (i=1999;i>=0;i--) { }
      seed=1;
      { unsigned szs[2000]; seed=1; for (i=0;i<2000;i++){ seed=seed*1103515245u+12345u; szs[i]=1+(seed>>8)%700; }
        for (i=0;i<2000;i+=2) { TlsMemPool *q=x_(p,SizeSelect,szs[i]); x_(q,Free,ptr[i],szs[i]); }
        for (i=1;i<2000;i+=2) { TlsMemPool *q=x_(p,SizeSelect,szs[i]); x_(q,Free,ptr[i],szs[i]); }
      }
    }
    TlsHeapLayerPop(l);
    CHECK(Cnt(rc)==base);
    TlsHeapClose(h);
    CHECK(Cnt(rc)==0);
    x_(root,Close);
}
static void TestClose(void) {
    TlsMemPool *root; x_(Main,Set); root = TlsMainPoolOpen(0);
    TlsIntCounter *rc = TlsIntCounterDirect(0);
    TlsStack *S; TlsHeap *h; TlsStackNod *l; TlsMemPool *p; int base,mid;
    x_(root,CountSwitch,rc);
    S = TlsStackOpen(root,4096,0); h = TlsStackHeap(S); x_(h,Set);
    base = Cnt(rc);
    l = TlsHeapLayer();
    p = TlsMemPoolPush();
    mid = Cnt(rc);
    { int i; for (i=0;i<500;i++) x_(x_(p,SizeSelect,40+i),Allocate); }
    CHECK(Cnt(rc)>mid);
    x_(p,Close);                 /* whole pool released at once */
    CHECK(Cnt(rc)<=mid+4096);
    TlsHeapLayerPop(l);          /* the fork goes with the parent layer */
    CHECK(Cnt(rc)==base);
    TlsHeapClose(h); x_(root,Close);
}
static void TestRecycle(void) {
    TlsMemPool *root; x_(Main,Set); root = TlsMainPoolOpen(0);
    TlsIntCounter *rc = TlsIntCounterDirect(0);
    TlsStack *S; TlsHeap *h; TlsMemPool *p; int i,a,b;
    x_(root,CountSwitch,rc);
    S = TlsStackOpen(root,4096,0); h = TlsStackHeap(S); x_(h,Set);
    { TlsStackNod *l=TlsHeapLayer();
      p = TlsMemPoolPush();
      for (i=0;i<20000;i++) { unsigned sz=10+(i%900); TlsMemPool *q=x_(p,SizeSelect,sz); void *m=x_(q,Allocate); memset(m,1,sz); x_(q,Free,m,sz); }
      a=Cnt(rc);
      for (i=0;i<200000;i++) { unsigned sz=10+(i%900); TlsMemPool *q=x_(p,SizeSelect,sz); void *m=x_(q,Allocate); memset(m,1,sz); x_(q,Free,m,sz); }
      b=Cnt(rc);
      CHECK(a==b);
      TlsHeapLayerPop(l);
    }
    TlsHeapClose(h); x_(root,Close);
}
static void TestCounter(void) {
    TlsMemPool *root; x_(Main,Set); root = TlsMainPoolOpen(0);
    TlsStack *S; TlsHeap *h; TlsMemPool *p; TlsStackNod *l;
    TlsIntCounter *c=TlsIntCounterDirect(0);
    S = TlsStackOpen(root,4096,0); h = TlsStackHeap(S); x_(h,Set);
    l=TlsHeapLayer(); p=TlsMemPoolPush();
    x_(p,CountSwitch,c);
    { void *m=x_(x_(p,SizeSelect,1000),Allocate); CHECK(Cnt(c)>=1000); x_(x_(p,SizeSelect,1000),Free,m,1000); CHECK(Cnt(c)==0); }
    { void *m=x_(x_(p,SizeSelect,1000),Allocate); (void)m; CHECK(Cnt(c)>=1000); }
    TlsHeapLayerPop(l);
    TlsHeapClose(h); x_(root,Close);
}
int main(void) {
    TlsMainHeapOpen(65536); Main=TlsHeap_.Tgt;
    TestLayerPop(1024); TestLayerPop(4096); TestLayerPop(65536);
    TestClose(); TestRecycle(); TestCounter();
    printf("Push OK\n");
    return 0;
}
 
