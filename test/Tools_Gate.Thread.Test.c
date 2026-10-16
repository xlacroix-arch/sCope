/* Real threads: gate threads over a Shared pool, each with its own Buffered pool.
 * A merged client process must run on the thread of the server, and be back on
 * its own thread when the request is over. The threads form a tree, a gate is
 * the edge between a parent and a child, and a thread only talks to its parent
 * and its children: the clients of a server are its children (or its parent). */
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
 
struct ClientEntry;
/*---- server: serves calls "Res = 2*Arg" to the merged client; Arg<0 ends.
 * It serves its Up (the parent as a client) and the ends of its own children
 * (Nc clients it spawns, whose Up is the other end). Force: when its Up is shut
 * it ends at once, children or not; else it waits for the children to end. ----*/
#define NCMAX 8
typedef struct {
    TlsGateEntry E;
    long Tid; atomic_int Served;
    TlsGate *Up;
    struct ClientEntry *Clients; int Nc;
    int Force;
} ServerEntry;
static void ServeOne(ServerEntry *se,TlsGateGuest *r) {
    TlsGateGuest *c;
    TlsStackNod *l;
    Msg s;
    l = TlsHeapLayer();
    c = TlsGateGuestSwitch(r,&s.G);
    while (c) {
        Msg *q = (Msg *)c;
        int *m;
        if (q->Arg<0) { TlsGateGuestSendOff(&q->G); break; }
        rnPush(m,4);                        /* the server heap is the one in use here */
        m[0] = q->Arg;
        q->Res = 2*m[0];
        q->Tid = Tid();
        c = TlsGateGuestSwitch(&q->G,&s.G);
    }
    TlsHeapLayerPop(l);
    atomic_fetch_add(&se->Served,1);
}
static void SpawnClients(ServerEntry *se,TlsGate **e,int *n);
static void ServerFn(TlsGateEntry *this,TlsGate *Up) {
    ServerEntry *se = (ServerEntry *)this;
    TlsGate *e[1+NCMAX];
    int n = 0,i,upShut = 0;
    se->Tid = Tid();
    se->Up = Up;
    CHECK(TlsGateUp()==Up);
    e[n++] = Up;
    SpawnClients(se,e,&n);
    for (;;) {
        TlsGateGuest *r;
        i = TlsGateWaitAny(e,n);
        if (i>=0) {
            r = x_(e[i],Invite);
            if (r) ServeOne(se,r);
            continue;
        }
        for (i=0;i<n;) {                    /* drop the ends that are shut */
            if (TlsGateIsShut(e[i])) {
                if (e[i]==Up) upShut = 1;
                e[i] = e[--n];
            } else i++;
        }
        if (upShut && (se->Force || n==0)) break;
        if (n==0) break;
    }
}
static struct TlsGateEntry ServerStatic = {ServerFn};
 
/*---- client ----*/
typedef struct ClientEntry {
    TlsGateEntry E;
    TlsGate *Server;                        /* where it knocks: its Up, but for the main thread */
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
static void ClientFn(TlsGateEntry *this,TlsGate *Up) {
    ClientEntry *ce = (ClientEntry *)this;
    int j,id = (int)((Tid()>>2)&0xFF);
    ce->Server = Up;
    if (ce->Reps) { for (j=0;j<ce->Reps;j++) ClientCalls(ce,id+j); }
    else { while (!atomic_load(&ce->Refused)) ClientCalls(ce,id++); }
}
static struct TlsGateEntry ClientStatic = {ClientFn};
 
static void SpawnClients(ServerEntry *se,TlsGate **e,int *n) {
    int i;
    ClientEntry *c = se->Clients;
    for (i=0;i<se->Nc;i++) e[(*n)++] = TlsGateSet(32768,&c[i].E);   /* the parent ends: ours */
}
 
#define NC 4
/*---- A: a server thread, its NC children and the main thread as clients ----*/
static void TestA(void) {
    ServerEntry se = {{&ServerStatic}};
    ClientEntry ce[NC],mc;
    TlsGate *sg;
    int i,j,reps = 400;
    atomic_init(&se.Served,0);
    memset(ce,0,sizeof(ce));
    for (i=0;i<NC;i++) { ce[i].E.Static = &ClientStatic; ce[i].SE = &se; ce[i].Reps = reps; }
    se.Clients = ce; se.Nc = NC;
    sg = TlsGateSet(32768,&se.E);
    memset(&mc,0,sizeof(mc));
    mc.E.Static = &ClientStatic; mc.Server = sg; mc.SE = &se; mc.Reps = reps;
    for (j=0;j<reps;j++) ClientCalls(&mc,1000+j);   /* the main thread is a client too, as the parent */
    x_(sg,Shutdown);                        /* the server ends once its children have */
    for (i=0;i<NC;i++) CHECK(atomic_load(&ce[i].Done)==reps && atomic_load(&ce[i].Refused)==0);
    CHECK(atomic_load(&mc.Done)==reps);
    CHECK(atomic_load(&se.Served)==(NC+1)*reps);
    CHECK(TlsGatePeer(se.Up)==sg && TlsGatePeer(sg)==se.Up);
    printf("A: %d requests over real threads, merged code ran on the server thread: ok\n",(NC+1)*reps);
}
 
/*---- B: the server is removed while its children knock in a loop ----*/
static void TestB(void) {
    ServerEntry se = {{&ServerStatic}};
    ClientEntry ce[NC];
    TlsGate *sg;
    int i,total = 0;
    atomic_init(&se.Served,0);
    memset(ce,0,sizeof(ce));
    for (i=0;i<NC;i++) { ce[i].E.Static = &ClientStatic; ce[i].SE = &se; ce[i].Reps = 0; }
    se.Clients = ce; se.Nc = NC; se.Force = 1;
    sg = TlsGateSet(32768,&se.E);
    usleep(100000);
    x_(sg,Shutdown);                        /* the server ends, then its children are refused and joined */
    for (i=0;i<NC;i++) { CHECK(atomic_load(&ce[i].Refused)==1); total += atomic_load(&ce[i].Done); }
    CHECK(total==atomic_load(&se.Served) && total>0);
    CHECK(atomic_load(&TinyN)>=2*total);
    printf("B: removal amid %d served requests, every client refused or served: ok\n",total);
}
 
/*---- C: a thread that spawns a thread, and calls it ----*/
typedef struct { TlsGateEntry E; ServerEntry *Child; TlsGate *ChildGate; TlsGate *Up; long Tid; int EdgeOk; } MidEntry;
static void MidFn(TlsGateEntry *this,TlsGate *Up) {
    MidEntry *me = (MidEntry *)this;
    ClientEntry ce;
    ServerEntry *c = me->Child;
    atomic_init(&c->Served,0);
    me->Up = Up;
    me->Tid = Tid();
    me->ChildGate = TlsGateSet(32768,&c->E);        /* an edge between this thread and the child */
    memset(&ce,0,sizeof(ce));
    ce.Server = me->ChildGate; ce.SE = c;
    ClientCalls(&ce,3);
    ClientCalls(&ce,4);
    CHECK(atomic_load(&ce.Done)==2);
    me->EdgeOk = (TlsGatePeer(c->Up)==me->ChildGate);   /* while the edge lives: it dies with the child */
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
    CHECK(TlsGatePeer(mid.Up)==mg);                 /* the two ends of the edge main-mid */
    CHECK(mid.EdgeOk);                              /* and of the edge mid-child, on the heap of mid */
    CHECK(atomic_load(&child.Served)==2);
    printf("C: nested thread, one edge per thread, children shut down first: ok\n");
}
 
/*---- D: layer pop shuts the threads down, children first, nobody calls Shutdown ----*/
static atomic_int EndedN,ChildEnded;
static void IdleFn(TlsGateEntry *this,TlsGate *Up) {
    (void)this;
    while (x_(Up,Wait)) {
        TlsGateGuest *r = x_(Up,Invite);
        if (r) TlsGateGuestSendOff(r);
    }
    atomic_fetch_add(&EndedN,1);
}
static struct TlsGateEntry IdleStatic = {IdleFn};
static void ParentFn(TlsGateEntry *this,TlsGate *Up) {
    TlsGateEntry *grand;
    TlsGate *g;
    (void)this;
    TlsHeapAlign(16);
    rPush(grand);                           /* on the heap, not in this frame: the thread is joined after we return */
    grand->Static = &IdleStatic;
    g = TlsGateSet(32768,grand);            /* never shut down here */
    (void)g;
    while (x_(Up,Wait)) {
        TlsGateGuest *r = x_(Up,Invite);
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
 
/*---- E: the child has ended: the edge is still there, closed, until the parent removes it ----*/
static void QuitFn(TlsGateEntry *this,TlsGate *Up) { (void)this; (void)Up; }
static struct TlsGateEntry QuitStatic = {QuitFn};
static void TestE(void) {
    TlsGateEntry quit = {&QuitStatic};
    TlsGate *g = TlsGateSet(32768,&quit);
    Msg req;
    while (!TlsGateIsShut(g)) usleep(1000); /* its end of the edge: the thread is over */
    CHECK(x_(g,Knock,&req.G)==&TlsGateEndShutdown.Guest);   /* a late knock is refused, not lost */
    CHECK(x_(g,Check)==0);
    x_(g,Shutdown);                         /* joins and releases */
    printf("E: a late Knock on an ended thread is refused: ok\n");
}
 
/*---- F: the child shuts its own end: the parent's Knock is refused ----*/
static atomic_int Closed,Leave;
static void ByeFn(TlsGateEntry *this,TlsGate *Up) {
    (void)this;
    x_(Up,Shutdown);
    atomic_store(&Closed,1);
    while (!atomic_load(&Leave)) usleep(1000);
}
static struct TlsGateEntry ByeStatic = {ByeFn};
static void TestF(void) {
    TlsGateEntry bye = {&ByeStatic};
    TlsGate *g;
    Msg req;
    atomic_store(&Closed,0); atomic_store(&Leave,0);
    g = TlsGateSet(32768,&bye);
    while (!atomic_load(&Closed)) usleep(1000);
    CHECK(x_(g,Knock,&req.G)==&TlsGateEndShutdown.Guest);
    atomic_store(&Leave,1);
    x_(g,Shutdown);
    printf("F: a child that shuts its end is not served any more: ok\n");
}
 
/*---- main ----*/
static MPPool *MP;
static TlsMemPool *Shared;
static void MainProc(void *arg) {
    TlsStackNod *l;
    (void)arg;
    CHECK(TlsGateUp()==0);                  /* the main thread is the root of the tree */
    l = TlsHeapLayer(); TestA(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestB(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestC(); TlsHeapLayerPop(l);
    TestD();
    l = TlsHeapLayer(); TestE(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestF(); TlsHeapLayerPop(l);
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
 
