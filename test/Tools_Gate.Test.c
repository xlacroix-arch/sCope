/* Fiber-only test of the gates: "threads" are cooperative contexts of one OS
 * thread, run by a round robin scheduler. The implementation is included to
 * replace the blocking primitive by a yield. */
#include "Tools_Gate.c"
#include <stdio.h>
#include <stdlib.h>
 
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
 
/*---- scheduler ----*/
typedef struct SimT SimT;
struct SimT {
    fiber slot;
    TlsGateNod N;
    int done;
    void (*fn)(void *);
    void *arg;
};
static SimT *SimCur;
 
static void SimYield(void) {
    TlsHeap *mine = TlsHeap_.Tgt;
    SimT *me = SimCur;
    x_(mine,Sync);
    fiber_switch(&me->slot);
    x_(mine,Set);
}
static void SimBlock(atomic_uint *w,unsigned e) {
    while (atomic_load_explicit(w,memory_order_acquire)==e) SimYield();
}
static void SimWake(atomic_uint *w) { (void)w; }
static const GateSync SimSync = {SimBlock,SimWake};
 
static void SimBoot(fiber *parent) {
    SimT *t = v_(SimT,slot,parent);
    TlsGateHomeRun(&t->N,32768,t->fn,t->arg);
    t->done = 1;
}
static void SimRun(SimT *ts,int n) {
    struct fiber_stack st;
    TlsHeap *mainheap;
    int i,round,alive;
    x_(TlsHeap_.Tgt,Sync);
    mainheap = TlsHeap_.Tgt;
    for (i=0;i<n;i++) {
        char *stk = malloc(65536);
        st.start = stk; st.end = stk+65536;
        ts[i].done = 0;
        x_(mainheap,Set);
        SimCur = &ts[i];
        Home_ = &ts[i].N;
        fiber_launch(&st,SimBoot,&ts[i].slot);
    }
    for (round=0;round<200000;round++) {
        alive = 0;
        for (i=0;i<n;i++) {
            if (ts[i].done) continue;
            alive = 1;
            x_(mainheap,Set);
            SimCur = &ts[i];
            Home_ = &ts[i].N;
            fiber_switch(&ts[i].slot);
        }
        if (!alive) break;
    }
    x_(mainheap,Set);
    CHECK(round<200000 && "deadlock");
}
 
typedef struct { TlsGateGuest G; int Arg,Res,Id; } Msg;
static TlsGate *G;
static int Count[8];
static int Order[8],NOrder;
 
/*---- 1. plain requests: no merge ----*/
#define K 50
static void ServerPlain(void *a) {
    int n = *(int *)a;
    while (n--) {
        TlsGateGuest *r;
        CHECK(x_(G,Wait)==1);
        r = x_(G,Invite);
        CHECK(r);
        Count[7]++;
        TlsGateGuestSendOff(r);
    }
}
static void ClientPlain(void *a) {
    int id = *(int *)a,i;
    Msg req;
    for (i=0;i<K;i++) {
        TlsGateGuest *r = x_(G,Knock,&req.G);
        CHECK(r==0);
        Count[id]++;
    }
}
static void Test1(void) {
    SimT t[4]; int ids[3] = {0,1,2},ns = 3*K;
    memset(Count,0,sizeof(Count));
    t[0].fn = ServerPlain; t[0].arg = &ns;
    t[1].fn = ClientPlain; t[1].arg = &ids[0];
    t[2].fn = ClientPlain; t[2].arg = &ids[1];
    t[3].fn = ClientPlain; t[3].arg = &ids[2];
    SimRun(t,4);
    CHECK(Count[0]==K && Count[1]==K && Count[2]==K && Count[7]==3*K);
    printf("plain requests: ok\n");
}
 
/*---- 2. merged: the client calls services of the server ----*/
#define R 20
static void ServerMerged(void *a) {
    int n = *(int *)a;
    while (n--) {
        TlsGateGuest *r,*c;
        Msg s,*q;
        int *mem;
        CHECK(x_(G,Wait)==1);
        r = x_(G,Invite);
        CHECK(r);
        c = TlsGateGuestSwitch(r,&s.G);     /* the client runs on this thread now */
        for (;;) {
            q = (Msg *)c;
            rPush(mem); *mem = q->Arg;      /* the server heap is back in use each time */
            if (q->Arg<0) { TlsGateGuestSendOff(&q->G); break; }
            q->Res = 2*q->Arg;
            CHECK(*mem==q->Arg);
            c = TlsGateGuestSwitch(&q->G,&s.G);
        }
        Count[7]++;
    }
}
static void ClientMerged(void *a) {
    int id = *(int *)a,i,j;
    Msg req,q;
    for (j=0;j<3;j++) {
        TlsGateGuest *sv = x_(G,Knock,&req.G);
        int *mine;
        CHECK(sv);
        rnPush(mine,16);                    /* allocated while merged */
        for (i=0;i<16;i++) mine[i] = id*1000+i;
        for (i=0;i<R;i++) {
            q.Arg = i;
            sv = TlsGateGuestSwitch(sv,&q.G);
            CHECK(sv && q.Res==2*i);
        }
        q.Arg = -1;
        CHECK(TlsGateGuestSwitch(sv,&q.G)==0);
        for (i=0;i<16;i++) CHECK(mine[i]==id*1000+i);   /* heap intact on our own thread */
        Count[id]++;
    }
}
static void Test2(void) {
    SimT t[3]; int ids[2] = {0,1},ns = 6;
    memset(Count,0,sizeof(Count));
    t[0].fn = ServerMerged; t[0].arg = &ns;
    t[1].fn = ClientMerged; t[1].arg = &ids[0];
    t[2].fn = ClientMerged; t[2].arg = &ids[1];
    SimRun(t,3);
    CHECK(Count[0]==3 && Count[1]==3 && Count[7]==6);
    printf("merged requests: ok\n");
}
 
/*---- 3. the client process moves while merged: SendOff resumes where it is ----*/
static int MoveStage;
static TlsGateGuest *SvPass;
static void MoveChild(TlsFiber *parent) {
    Msg req;
    SvPass = x_(G,Knock,&req.G);
    CHECK(SvPass);
    MoveStage = 1;
    TlsFiberSwitch(parent);                 /* the thread goes on in the parent fiber */
    MoveStage = 4;
}
static void ClientMove(void *a) {
    TlsFiber slot;
    Msg q;
    (void)a;
    TlsFiberLaunch(16384,MoveChild,&slot);
    CHECK(MoveStage==1);
    q.Arg = -1;
    CHECK(TlsGateGuestSwitch(SvPass,&q.G)==0);   /* resumed here, not in the child */
    MoveStage = 3;
    TlsFiberSwitch(&slot);
    CHECK(MoveStage==4);
}
static void Test3(void) {
    SimT t[2]; int ns = 1;
    MoveStage = 0;
    t[0].fn = ServerMerged; t[0].arg = &ns;
    t[1].fn = ClientMove; t[1].arg = 0;
    SimRun(t,2);
    CHECK(MoveStage==4);
    printf("process moved while merged: ok\n");
}
 
/*---- 4. arrival order ----*/
static void ClientOrder(void *a) {
    Msg req;
    req.Id = *(int *)a;
    CHECK(x_(G,Knock,&req.G)==0);
}
static void ServerOrder(void *a) {
    int n = *(int *)a;
    CHECK(x_(G,Check)==0);
    CHECK(x_(G,Invite)==0);
    while (x_(G,Check)<1) SimYield();       /* let a few arrive */
    SimYield(); SimYield(); SimYield();
    while (n--) {
        TlsGateGuest *r;
        CHECK(x_(G,Wait)==1);
        r = x_(G,Invite);
        Order[NOrder++] = ((Msg *)r)->Id;
        TlsGateGuestSendOff(r);
    }
}
static void Test4(void) {
    SimT t[4]; int ids[3] = {0,1,2},ns = 3;
    NOrder = 0;
    t[0].fn = ServerOrder; t[0].arg = &ns;
    t[1].fn = ClientOrder; t[1].arg = &ids[0];
    t[2].fn = ClientOrder; t[2].arg = &ids[1];
    t[3].fn = ClientOrder; t[3].arg = &ids[2];
    SimRun(t,4);
    CHECK(NOrder==3 && Order[0]==0 && Order[1]==1 && Order[2]==2);
    printf("arrival order: ok\n");
}
 
/*---- 5. shutdown ----*/
static int ShutSeen;
static void ClientShut(void *a) {
    Msg req;
    TlsGateGuest *r;
    (void)a;
    r = x_(G,Knock,&req.G);                 /* nobody serves: refused at the shutdown */
    CHECK(r==&TlsGateEndShutdown.Guest);
    r = x_(G,Knock,&req.G);                 /* refused at once */
    CHECK(r==&TlsGateEndShutdown.Guest && TlsGateEndShutdown.Status==1);
    ShutSeen++;
}
static void Controller(void *a) {
    int i;
    (void)a;
    for (i=0;i<20;i++) SimYield();
    x_(G,Shutdown);
    x_(G,Shutdown);                         /* idempotent */
}
static void ServerShut(void *a) {
    (void)a;
    CHECK(x_(G,Wait)==0);                   /* woken by the shutdown */
    CHECK(x_(G,Invite)==0);
    ShutSeen++;
}
static void Test5(void) {
    SimT t[3];
    ShutSeen = 0;
    t[0].fn = ClientShut; t[0].arg = 0;
    t[1].fn = ClientShut; t[1].arg = 0;
    t[2].fn = Controller; t[2].arg = 0;
    SimRun(t,3);
    CHECK(ShutSeen==2);
    printf("shutdown refuses the requests nobody pulled: ok\n");
}
static void Test5b(void) {
    SimT t[2];
    ShutSeen = 0;
    t[0].fn = ServerShut; t[0].arg = 0;
    t[1].fn = Controller; t[1].arg = 0;
    SimRun(t,2);
    CHECK(ShutSeen==1);
    printf("shutdown wakes the server: ok\n");
}
 
/*---- 6. what the server pulled before the shutdown is served ----*/
static void ClientPulled(void *a) {
    Msg req;
    (void)a;
    CHECK(x_(G,Knock,&req.G)==0);           /* served, not refused */
    ShutSeen++;
}
static void ServerPulled(void *a) {
    TlsGateGuest *r;
    (void)a;
    while (x_(G,Check)<1) SimYield();       /* pulled */
    while (!(atomic_load(&((Gate *)G)->Head)==SHUT)) SimYield();
    r = x_(G,Invite);                       /* after the shutdown */
    CHECK(r);
    TlsGateGuestSendOff(r);
    CHECK(x_(G,Wait)==0);
    ShutSeen++;
}
static void ControllerLate(void *a) {
    int i;
    (void)a;
    for (i=0;i<20;i++) SimYield();
    x_(G,Shutdown);
}
static void Test6(void) {
    SimT t[3];
    ShutSeen = 0;
    t[0].fn = ClientPulled; t[0].arg = 0;
    t[1].fn = ServerPulled; t[1].arg = 0;
    t[2].fn = ControllerLate; t[2].arg = 0;
    SimRun(t,3);
    CHECK(ShutSeen==2);
    printf("requests pulled before the shutdown are served: ok\n");
}
 
/*---- 7. the client ends the request itself ----*/
static void ServerEarly(void *a) {
    int n = *(int *)a;
    while (n--) {
        TlsGateGuest *r,*c;
        Msg s;
        CHECK(x_(G,Wait)==1);
        r = x_(G,Invite);
        c = TlsGateGuestSwitch(r,&s.G);
        while (c) {                         /* NULL: ended by the client */
            Msg *q = (Msg *)c;
            q->Res = 2*q->Arg;
            c = TlsGateGuestSwitch(&q->G,&s.G);
        }
        Count[7]++;
    }
}
static void ClientEarly(void *a) {
    int id = *(int *)a,i,j;
    Msg req,q;
    for (j=0;j<4;j++) {
        TlsGateGuest *sv = x_(G,Knock,&req.G);
        int *mine;
        CHECK(sv);
        rnPush(mine,8); mine[0] = id*100+j;
        for (i=0;i<(j&1?0:5);i++) {         /* odd rounds: end before any call */
            q.Arg = i;
            sv = TlsGateGuestSwitch(sv,&q.G);
            CHECK(sv && q.Res==2*i);
        }
        TlsGateGuestSendOff(sv);            /* back on our own thread after this */
        CHECK(mine[0]==id*100+j);
        Count[id]++;
    }
}
static void Test7(void) {
    SimT t[3]; int ids[2] = {0,1},ns = 8;
    memset(Count,0,sizeof(Count));
    t[0].fn = ServerEarly; t[0].arg = &ns;
    t[1].fn = ClientEarly; t[1].arg = &ids[0];
    t[2].fn = ClientEarly; t[2].arg = &ids[1];
    SimRun(t,3);
    CHECK(Count[0]==4 && Count[1]==4 && Count[7]==8);
    printf("request ended by the client: ok\n");
}
 
int main(void) {
    TlsStackNod *layer;
    SyncOps = &SimSync;
    TlsMainHeapOpen(65536);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test1(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test2(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test3(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test4(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test5(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test5b(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test6(); TlsHeapLayerPop(layer);
    layer = TlsHeapLayer();
    G = TlsGateOpen(); Test7(); TlsHeapLayerPop(layer);
    TlsMainHeapClose();
    printf("all passed\n");
    return 0;
}
 
