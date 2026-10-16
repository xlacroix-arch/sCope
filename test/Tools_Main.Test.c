/* Tools_Main test: the application is only TlsMain; main() comes from Tools_Main.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <Tools_Main.h>
#define CHECK(c) do{ if(!(c)){ fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(99);} }while(0)
 
static atomic_int Idle,GotParent;
typedef struct { TlsGateEntry E; } IdleEntry;
static void IdleFn(TlsGateEntry *this,TlsGate *Up) {
    (void)this;
    if (Up && Up==TlsGateUp()) atomic_fetch_add(&GotParent,1);
    atomic_fetch_add(&Idle,1);
    while (x_(Up,Wait)) {
        TlsGateGuest *r = x_(Up,Invite);
        if (r) TlsGateGuestSendOff(r);
    }
    atomic_fetch_sub(&Idle,1);
}
static struct TlsGateEntry IdleStatic = {IdleFn};
const TlsMainConfig TlsMainConfigApp = {0,256*1024,0,0};
 
int TlsMain(int argc,char **argv) {
    int *v,i;
    IdleEntry *e;
    TlsGate *g;
    CHECK(argc==2 && !strcmp(argv[1],"hello"));
    CHECK(TlsGateUp()==0);                  /* the main thread is the root: no gate above it */
    rnPush(v,1000);                         /* the heap is there */
    for (i=0;i<1000;i++) v[i]=i;
    for (i=0;i<3;i++) {
        TlsHeapAlign(16);
        rPush(e);
        e->E.Static = &IdleStatic;
        g = TlsGateSet(32768,&e->E);
        CHECK(g && TlsGatePeer(TlsGatePeer(g))==g);
    }
    while (atomic_load(&Idle)<3) ;
    CHECK(atomic_load(&GotParent)==3);
    printf("TlsMain: 3 threads left running on return\n");
    return 42;                              /* the threads are shut down by the heap pop */
}

