/* Real threads: gate threads over a Shared pool, each with its own Buffered pool.
 * A merged client process must run on the thread of the server, and be back on
 * its own thread when the request is over. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <Tools_Core.h>
#include <Tools_Gate.h>
#include "MallocPool.h"
 
TlsMemPool *TlsMemPoolShared(TlsMemPool *child);
TlsMemPool *TlsMemPoolBuffered(TlsMemPool *child,unsigned int buffersize);
unsigned long TlsMemPoolSharedLocks(TlsMemPool *Handle);
 
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); fflush(stdout); exit(1); } } while (0)
#include <sys/syscall.h>
/* pthread_self() is "const" for the compiler: it may be computed once and reused
 * after a migration. The kernel thread id is an opaque call each time. */
static long Tid(void) { return syscall(SYS_gettid); }
#define ON(t) (Tid()==(t))
 
typedef struct { TlsGateGuest G; int Arg,Res; long Tid; } Msg;
 
/*---- server: serves calls "Res = 2*Arg" to the merged client; Arg<0 ends ----*/
typedef struct { TlsGateEntry E; long Tid; atomic_int Served; TlsGate *Self; TlsGate *Parent; } ServerEntry;
static void ServerFn(TlsGateEntry *this,TlsGate *Parent) {
    ServerEntry *se = (ServerEntry *)this;
    TlsGate *self = TlsGateSelf();
    se->Tid = Tid();
    se->Self = self;
    se->Parent = Parent;
    while (x_(self,Wait)) {
        TlsGateGuest *r = x_(self,Invite),*c;
        TlsStackNod *l;
        Msg s;
        if (!r) continue;
        l = TlsHeapLayer();
        c = TlsGateGuestSwitch(r,&s.G);
        while (c) {
            Msg *q = (Msg *)c;
            int *m;
            if (q->Arg<0) { TlsGateGuestSendOff(&q->G); break; }
            rnPush(m,4);                    /* the server heap is the one in use here */
            m[0] = q->Arg;
            q->Res = 2*m[0];
            q->Tid = Tid();
            c = TlsGateGuestSwitch(&q->G,&s.G);
        }
        TlsHeapLayerPop(l);
        atomic_fetch_add(&se->Served,1);
    }
}
static struct TlsGateEntry ServerStatic = {ServerFn};
 
/*---- client ----*/
typedef struct {
    TlsGateEntry E;
    TlsGate *Server;
    ServerEntry *SE;
    int Reps;                               /* 0: until refused */
    atomic_int Done,Refused;
} ClientEntry;
static atomic_int TinyN;
static void Tiny(TlsFiber *parent) {
    atomic_fetch_add(&TinyN,1);
    TlsFiberSwitch(parent);
    atomic_fetch_add(&TinyN,1);
}
static void ClientCalls(ClientEntry *ce,int id) {
    long me = Tid();
    Msg req,q;
    TlsFiber tf;
    TlsStackNod *layer = TlsHeapLayer();    /* taken at home, popped at home */
    TlsGateGuest *sv = x_(ce->Server,Knock,&req.G);
    int *mine,i,n = 1+id%4;
    if (sv==&TlsGateEndShutdown.Guest) { atomic_fetch_add(&ce->Refused,1); TlsHeapLayerPop(layer); return; }
    CHECK(sv);
    CHECK(ON(ce->SE->Tid));                 /* merged: this code now runs on the server thread */
    CHECK(TlsFiberLaunch(16384,Tiny,&tf));  /* a fiber born on the server thread ... */
    rnPush(mine,32);                        /* allocated on the client heap, from the server thread */
    for (i=0;i<32;i++) mine[i] = id*1000+i;
    for (i=0;i<n;i++) {
        q.Arg = id+i;
        sv = TlsGateGuestSwitch(sv,&q.G);
        CHECK(sv && q.Res==2*(id+i) && ON(ce->SE->Tid) && q.Tid==ce->SE->Tid);
    }
    if (id&1) {
        TlsGateGuestSendOff(sv);            /* ended by the client */
    } else {
        q.Arg = -1;
        CHECK(TlsGateGuestSwitch(sv,&q.G)==0);   /* ended by the server */
    }
    CHECK(ON(me));                          /* and home again */
    for (i=0;i<32;i++) CHECK(mine[i]==id*1000+i);
    TlsFiberSwitch(&tf);                    /* ... that ends on the home thread */
    CHECK(tf.SwitchNod==0);
    TlsHeapLayerPop(layer);
    atomic_fetch_add(&ce->Done,1);
}
static void ClientFn(TlsGateEntry *this,TlsGate *Parent) {
    ClientEntry *ce = (ClientEntry *)this;
    int j,id = (int)((Tid()>>2)&0xFF);
    (void)Parent;
    if (ce->Reps) { for (j=0;j<ce->Reps;j++) ClientCalls(ce,id+j); }
    else { while (!atomic_load(&ce->Refused)) ClientCalls(ce,id++); }
}
static struct TlsGateEntry ClientStatic = {ClientFn};
 
#define NC 4
/*---- A: many clients, plus the main thread, over one server ----*/
static void TestA(void) {
    ServerEntry se = {{&ServerStatic}};
    ClientEntry ce[NC],mc;
    TlsGate *sg,*cg[NC];
    int i,j,reps = 400;
    atomic_init(&se.Served,0);
    sg = TlsGateSet(32768,&se.E);
    for (i=0;i<NC;i++) {
        memset(&ce[i],0,sizeof(ce[i]));
        ce[i].E.Static = &ClientStatic; ce[i].Server = sg; ce[i].SE = &se; ce[i].Reps = reps;
        cg[i] = TlsGateSet(32768,&ce[i].E);
    }
    memset(&mc,0,sizeof(mc));
    mc.E.Static = &ClientStatic; mc.Server = sg; mc.SE = &se; mc.Reps = reps;
    for (j=0;j<reps;j++) ClientCalls(&mc,1000+j);   /* the main thread is a client too */
    for (i=0;i<NC;i++) x_(cg[i],Shutdown);          /* joins them */
    x_(sg,Shutdown);
    for (i=0;i<NC;i++) CHECK(atomic_load(&ce[i].Done)==reps && atomic_load(&ce[i].Refused)==0);
    CHECK(atomic_load(&mc.Done)==reps);
    CHECK(atomic_load(&se.Served)==(NC+1)*reps);
    printf("A: %d requests over real threads, merged code ran on the server thread: ok\n",(NC+1)*reps);
}
 
/*---- B: shutdown while clients knock in a loop ----*/
static void TestB(void) {
    ServerEntry se = {{&ServerStatic}};
    ClientEntry ce[NC];
    TlsGate *sg,*cg[NC];
    int i,total = 0;
    atomic_init(&se.Served,0);
    sg = TlsGateSet(32768,&se.E);
    for (i=0;i<NC;i++) {
        memset(&ce[i],0,sizeof(ce[i]));
        ce[i].E.Static = &ClientStatic; ce[i].Server = sg; ce[i].SE = &se; ce[i].Reps = 0;
        cg[i] = TlsGateSet(32768,&ce[i].E);
    }
    usleep(100000);
    x_(sg,Shutdown);                        /* refuses, the server ends, joined */
    for (i=0;i<NC;i++) x_(cg[i],Shutdown);  /* the clients end when refused */
    for (i=0;i<NC;i++) { CHECK(atomic_load(&ce[i].Refused)==1); total += atomic_load(&ce[i].Done); }
    CHECK(total==atomic_load(&se.Served) && total>0);
    CHECK(atomic_load(&TinyN)>=2*total);
    printf("B: shutdown amid %d served requests, every client refused or served: ok\n",total);
}
 
/*---- C: a thread that spawns a thread, and calls it ----*/
typedef struct { TlsGateEntry E; ServerEntry *Child; TlsGate *ChildGate; TlsGate *Self,*SawParent; long Tid; } MidEntry;
static void MidFn(TlsGateEntry *this,TlsGate *Parent) {
    MidEntry *me = (MidEntry *)this;
    ClientEntry ce;
    ServerEntry *c = me->Child;
    atomic_init(&c->Served,0);
    me->Self = TlsGateSelf();
    me->Tid = Tid();
    me->SawParent = Parent;
    me->ChildGate = TlsGateSet(32768,&c->E);        /* its Parent is the gate of this thread */
    memset(&ce,0,sizeof(ce));
    ce.Server = me->ChildGate; ce.SE = c;
    ClientCalls(&ce,3);
    ClientCalls(&ce,4);
    CHECK(atomic_load(&ce.Done)==2);
    x_(me->ChildGate,Shutdown);                     /* children go first */
}
static struct TlsGateEntry MidStatic = {MidFn};
static void TestC(void) {
    ServerEntry child = {{&ServerStatic}};
    MidEntry mid;
    TlsGate *mg;
    memset(&mid,0,sizeof(mid));
    mid.E.Static = &MidStatic; mid.Child = &child;
    mg = TlsGateSet(32768,&mid.E);
    x_(mg,Shutdown);                                /* the middle thread returns on its own */
    CHECK(mid.Self==mg && mid.SawParent==0);
    CHECK(child.Parent==mid.Self && child.Self==mid.ChildGate);
    CHECK(atomic_load(&child.Served)==2);
    printf("C: nested thread, Parent and Self gates, children shut down first: ok\n");
}
 
/*---- D: layer pop shuts the threads down, children first, nobody calls Shutdown ----*/
static atomic_int EndedN,ChildEnded;
static void IdleFn(TlsGateEntry *this,TlsGate *Parent) {
    TlsGate *self = TlsGateSelf();
    (void)this; (void)Parent;
    while (x_(self,Wait)) {
        TlsGateGuest *r = x_(self,Invite);
        if (r) TlsGateGuestSendOff(r);
    }
    atomic_fetch_add(&EndedN,1);
}
static struct TlsGateEntry IdleStatic = {IdleFn};
static void ParentFn(TlsGateEntry *this,TlsGate *Parent) {
    TlsGateEntry *grand;
    TlsGate *self = TlsGateSelf(),*g;
    (void)this; (void)Parent;
    TlsHeapAlign(16);
    rPush(grand);                           /* on the heap, not in this frame: the thread is joined after we return */
    grand->Static = &IdleStatic;
    g = TlsGateSet(32768,grand);            /* never shut down here */
    (void)g;
    while (x_(self,Wait)) {
        TlsGateGuest *r = x_(self,Invite);
        if (r) TlsGateGuestSendOff(r);
    }
    atomic_fetch_add(&EndedN,1);            /* the grand child is joined after this, when our heap closes */
    atomic_store(&ChildEnded,atomic_load(&EndedN));
}
static struct TlsGateEntry ParentStatic = {ParentFn};
static void TestD(void) {
    TlsStackNod *l = TlsHeapLayer();
    TlsGateEntry idle = {&IdleStatic},par = {&ParentStatic};
    TlsGate *g[4];
    Msg req;
    int i;
    atomic_store(&EndedN,0);
    for (i=0;i<3;i++) g[i] = TlsGateSet(32768,&idle);
    g[3] = TlsGateSet(32768,&par);
    for (i=0;i<4;i++) CHECK(x_(g[i],Knock,&req.G)==0);   /* all of them answer */
    TlsHeapLayerPop(l);                     /* joins all of them, the grand child too */
    CHECK(atomic_load(&EndedN)==5);
    printf("D: layer pop shut down 4 threads and a grand child: ok\n");
}
 
/*---- main ----*/
static MPPool *MP;
static TlsMemPool *Shared;
static void MainProc(void *arg) {
    TlsStackNod *l;
    (void)arg;
    l = TlsHeapLayer(); TestA(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestB(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestC(); TlsHeapLayerPop(l);
    TestD();
}
int main(void) {
    TlsMemPool *B;
    TlsStack *s;
    TlsHeap *h;
    TlsGateNod *home = malloc(TlsGateHomeSize());
    MP = MallocPoolOpen();
    Shared = TlsMemPoolShared(&MP->Root.TlsMemPool);
    TlsGateSharedSet(Shared);
    B = TlsMemPoolBuffered(Shared,65536);
    s = TlsStackOpen(B,65536,0);
    h = TlsStackHeap(s);
    x_(h,Set);
    TlsGateHomeRun(home,65536,MainProc,0);
    x_(h,Set);
    TlsHeapClose(h);
    x_(B,Close);
    CHECK(MP->Live==0);                     /* every block, of every thread, came back */
    printf("shared pool: %lu lock round trips, nothing left allocated\n",TlsMemPoolSharedLocks(Shared));
    x_(Shared,Close);
    free(home);
    printf("all passed\n");
    return 0;
}
 
