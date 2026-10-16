/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */
 
#define _GNU_SOURCE
#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <Tools_Core.h>
#include <Tools_Gate.h>
#include <pthread.h>
#include "fiber.h"
 
/*_____________________________________________________________________________
 |
 | Blocking: a thread blocks on an atomic word until it is not `expected`
 | any more. Futex on Linux; the test replaces it by a cooperative scheduler.
 |_____________________________________________________________________________
 */
typedef struct {
    void (*Block)(atomic_uint *w,unsigned expected);
    void (*Wake)(atomic_uint *w);
} GateSync;
static void FutexBlock(atomic_uint *w,unsigned e) {
    while (atomic_load_explicit(w,memory_order_acquire)==e)
        syscall(SYS_futex,w,FUTEX_WAIT_PRIVATE,e,0,0,0);
}
static void FutexWake(atomic_uint *w) {
    syscall(SYS_futex,w,FUTEX_WAKE_PRIVATE,1,0,0,0);
}
static const GateSync FutexSync = {FutexBlock,FutexWake};
static const GateSync *SyncOps = &FutexSync;
 
/*_____________________________________________________________________________
 |
 | Structures.
 |
 | A TlsGateNod is the home of one thread: the rendezvous slot f, in which the
 | other context of the thread is parked, and the word the thread sleeps on
 | when its process is out. At rest f holds the parking fiber P, the native
 | context of the thread. Knock swaps the client process in, P runs and
 | hands the request over; GuestSwitch swaps the two sides of a merge; P is
 | woken by SendOff (or a refusal), and swaps the client process back out.
 | Side tells what f holds: 0 P, 1 the client process, 2 the server context.
 |_____________________________________________________________________________
 */
typedef struct End End;
struct TlsGateNod {
    fiber f;
    TlsGateGuest *Guest;                    /* the mailbox: what the resumer passes */
    TlsGateNod *Next;                       /* link in the inbox of a gate */
    End *Out;                               /* request to hand over, for P */
    atomic_uint Word;                       /* 1: P must resume the client process */
    int Side;
    void (*BootFn)(void *);
    void *BootArg;
};
/* What a thread sleeps on when it serves gates: one per thread, shared by all
 * the inboxes it serves, so that it can wait on several ends at once. */
typedef struct {
    atomic_uint Seq;                        /* changes at each arrival */
    atomic_uint Sleeping;                   /* the server is about to sleep on Seq, or does */
} Waker;
/* A gate is an edge between two threads, and a TlsGate is one end of it. Each
 * end has an inbox: the requests addressed to it. Knock on an end posts to the
 * inbox of its Peer; Check, Wait and Invite serve the inbox of the end itself.
 * The loopback gate of TlsGateOpen is an end that is its own Peer. */
struct End {
    TlsGate TlsGate;
    End *Peer;
    _Atomic(TlsGateNod *) Head;             /* inbox, newest first; SHUT once shut */
    Waker *W;                               /* who sleeps for this inbox */
    TlsGateNod *Ready;                      /* server only: pulled, oldest first */
};
static void EndInit(End *e,End *peer,Waker *w) {
    e->Peer = peer;
    atomic_init(&e->Head,0);
    e->W = w;
    e->Ready = 0;
}
#define SHUT ((TlsGateNod *)1)
 
TlsGateEnd TlsGateEndShutdown = {{0},1};
#define EndShut (&TlsGateEndShutdown.Guest)
 
static __thread TlsGateNod *Home_;
 
unsigned TlsGateHomeSize(void) { return sizeof(TlsGateNod); }
 
/*_____________________________________________________________________________
 |
 | Guests
 |_____________________________________________________________________________
 */
 
/* Wakes the parking fiber of the client, which resumes its process with res. */
static void Release(TlsGateNod *n,TlsGateGuest *res) {
    n->Guest = res;
    atomic_store_explicit(&n->Word,1,memory_order_release);
    SyncOps->Wake(&n->Word);
}
 
/* Swaps the running context with the one parked in n->f, passing ret (NULL
 * when the caller ends the request). Why nothing here is volatile: fiber_switch
 * is an opaque call, so after it the compiler must reload anything that is
 * reachable through a pointer (n->Guest, n->Side ...), and the locals of this
 * frame are restored as they were at the call by the longjmp. Only the locals
 * of the function that contains the setjmp need volatile (fiber.c has them).
 * Between threads, the order comes from the release/acquire pairs of the
 * handoff (Head, Word), which volatile would not give. */
static TlsGateGuest *Switch(TlsGateNod *n,TlsGateGuest *ret) {
    TlsHeap *mine;
    TlsGateGuest *r;
    n->Side = 3-n->Side;
    n->Guest = ret;
    if (ret) ret->Nod = n;
    mine = TlsHeap_.Tgt;                    /* the heap of this process: it goes with it */
    x_(mine,Sync);
    fiber_switch(&n->f);
    x_(mine,Set);                           /* possibly on another thread */
    r = n->Guest;
    if (!r && n->Side==1) Release(n,0);     /* the client ended the request while merged:
                                               the server side finishes the job */
    return r;
}
 
TlsGateGuest *TlsGateGuestSwitch(TlsGateGuest *Target,TlsGateGuest *ret) {
    TlsGateNod *n = Target->Nod;
    assert(n && (n->Side==1 || n->Side==2) && "GuestSwitch: target is not a parked guest");
    assert(ret && "GuestSwitch: ret is required (SendOff ends a request)");
    Target->Nod = 0;
    return Switch(n,ret);
}
 
void TlsGateGuestSendOff(TlsGateGuest *Target) {
    TlsGateNod *n = Target->Nod;
    assert(n && (n->Side==1 || n->Side==2) && "SendOff: target is not a parked guest");
    Target->Nod = 0;
    if (n->Side==1) {                       /* server side, client parked: release it */
        Release(n,0);
    } else {                                /* client side, merged: switch with a NULL,
                                               the other side does the release and this
                                               returns once resumed on the home thread */
        Switch(n,0);
    }
}
 
/*_____________________________________________________________________________
 |
 | The parking fiber
 |_____________________________________________________________________________
 */
 
/* Hands the request of the parked client to the gate. Runs on the parking
 * fiber, after the client process is safely parked in n->f. */
static void Publish(End *g,TlsGateNod *n) {
    TlsGateNod *h = atomic_load_explicit(&g->Head,memory_order_relaxed);
    do {
        if (h==SHUT) { Release(n,EndShut); return; }
        n->Next = h;
    } while (!atomic_compare_exchange_weak_explicit(&g->Head,&h,n,
                memory_order_release,memory_order_relaxed));
    atomic_fetch_add_explicit(&g->W->Seq,1,memory_order_release);
    atomic_thread_fence(memory_order_seq_cst);          /* pairs with the one in Wait */
    if (atomic_load_explicit(&g->W->Sleeping,memory_order_relaxed))
        SyncOps->Wake(&g->W->Seq);          /* a busy server is not woken */
}
 
static void HomeBoot(fiber *parent) {
    TlsGateNod *n = v_(TlsGateNod,f,parent);
    n->BootFn(n->BootArg);
    assert(n->Side==0 && "a client process must not end while parked or merged");
}
 
void TlsGateHomeRun(TlsGateNod *n,unsigned stack_sz,void (*fn)(void *),void *arg) {
    struct fiber_stack st;
    char *stk;
    TlsHeap *child;
    TlsGateNod *prev = Home_;
    fiber *r;
    memset(n,0,sizeof(*n));
    n->BootFn = fn;
    n->BootArg = arg;
    child = TlsHeapFork();
    x_(child,Set);
    TlsHeapAlign(16);
    rnPush(stk,stack_sz);
    st.start = stk;
    st.end = stk+stack_sz;
    Home_ = n;
    r = fiber_launch(&st,HomeBoot,&n->f);
    assert(r && "the process could not be bootstrapped");
    while (n->f.caller) {                   /* else the process is over */
        End *g = n->Out;
        assert(g && "parking fiber resumed without a request");
        n->Out = 0;
        Publish(g,n);
        SyncOps->Block(&n->Word,0);
        atomic_store_explicit(&n->Word,0,memory_order_relaxed);
        n->Side = 0;
        fiber_switch(&n->f);                /* back to the client process, here */
    }
    Home_ = prev;
}
 
/*_____________________________________________________________________________
 |
 | Gate ends
 |_____________________________________________________________________________
 */
 
static TlsGateGuest *GateKnock(TlsGate *this,TlsGateGuest *request) {
    TlsGetThat(End,TlsGate);
    End *to = that->Peer;                   /* the inbox the request goes to */
    TlsGateNod *n = Home_;
    TlsHeap *mine;
    assert(n && "Knock: this thread has no home");
    if (atomic_load_explicit(&to->Head,memory_order_acquire)==SHUT) return EndShut;
    request->Nod = 0;
    n->Guest = request;
    n->Out = to;
    n->Side = 1;
    mine = TlsHeap_.Tgt;
    x_(mine,Sync);
    fiber_switch(&n->f);                    /* park here, P runs */
    x_(mine,Set);                           /* resumed, on the server thread if merged */
    return n->Guest;
}
 
/* Server only: moves the arrivals to the ready list, oldest first. */
static int Pull(End *g) {
    TlsGateNod *h;
    if (g->Ready) return 1;
    h = atomic_load_explicit(&g->Head,memory_order_acquire);
    while (h && h!=SHUT) {
        if (atomic_compare_exchange_weak_explicit(&g->Head,&h,0,
                memory_order_acquire,memory_order_relaxed)) {
            TlsGateNod *r = 0;
            while (h) { TlsGateNod *n = h->Next; h->Next = r; r = h; h = n; }
            g->Ready = r;
            return 1;
        }
    }
    return 0;
}
static int IsShut(End *g) { return atomic_load_explicit(&g->Head,memory_order_acquire)==SHUT; }
 
static int GateCheck(TlsGate *this) {
    TlsGetThat(End,TlsGate);
    return Pull(that);
}
 
/* Waits on the n ends (all served by the calling thread: one Waker) until one
 * has a request: returns its index. Returns -1 when none has one and some end
 * is shut: the caller finds which with IsShut, and drops it (or ends). */
static int WaitAnyEnds(End **e,int n) {
    Waker *w = e[0]->W;
    for (;;) {
        unsigned s;
        int i;
        for (i=0;i<n;i++) {
            assert(e[i]->W==w && "WaitAny: the ends must be served by the same thread");
            if (Pull(e[i])) return i;
        }
        for (i=0;i<n;i++) if (IsShut(e[i])) return -1;
        s = atomic_load_explicit(&w->Seq,memory_order_acquire);
        atomic_store_explicit(&w->Sleeping,1,memory_order_relaxed);
        atomic_thread_fence(memory_order_seq_cst);      /* either the publisher sees Sleeping, or we see its request */
        for (i=0;i<n;i++) if (Pull(e[i]) || IsShut(e[i])) break;
        if (i<n) {                          /* arrived since the first look, or shut */
            atomic_store_explicit(&w->Sleeping,0,memory_order_relaxed);
            continue;
        }
        SyncOps->Block(&w->Seq,s);
        atomic_store_explicit(&w->Sleeping,0,memory_order_relaxed);
    }
}
 
static int GateWait(TlsGate *this) {
    TlsGetThat(End,TlsGate);
    End *e = that;
    return WaitAnyEnds(&e,1)==0;
}
 
int TlsGateIsShut(TlsGate *this) {
    TlsGetThat(End,TlsGate);
    return IsShut(that) && !that->Ready;
}
 
int TlsGateWaitAny(TlsGate **Ends,int n) {
    End *e[16];
    int i;
    assert(n>=1 && n<=16 && "WaitAny: 1 to 16 ends");
    for (i=0;i<n;i++) e[i] = v_(End,TlsGate,Ends[i]);
    return WaitAnyEnds(e,n);
}
 
static TlsGateGuest *GateInvite(TlsGate *this) {
    TlsGetThat(End,TlsGate);
    TlsGateNod *n;
    if (!Pull(that)) return 0;
    n = that->Ready;
    that->Ready = n->Next;
    n->Next = 0;
    n->Guest->Nod = n;
    return n->Guest;
}
 
/* Closes the inbox: what nobody served is refused, what arrives later too. */
static void ShutInbox(End *g) {
    TlsGateNod *h = atomic_exchange_explicit(&g->Head,SHUT,memory_order_acq_rel);
    if (h==SHUT) return;
    while (h) {
        TlsGateNod *n = h->Next;            /* before the client can reuse it */
        Release(h,EndShut);
        h = n;
    }
    atomic_fetch_add_explicit(&g->W->Seq,1,memory_order_release);
    SyncOps->Wake(&g->W->Seq);
}
static void GateShutdown(TlsGate *this) {
    TlsGetThat(End,TlsGate);
    ShutInbox(that);
}
/* The server's own Ready list, once it will not serve it: refused too. */
static void DrainReady(End *g) {
    while (g->Ready) {
        TlsGateNod *n = g->Ready;
        g->Ready = n->Next;
        n->Next = 0;
        Release(n,EndShut);
    }
}
 
static Waker *CurWaker(void);
TlsGate *TlsGateOpen(void) {
    static struct TlsGate Static = {GateShutdown,GateKnock,GateCheck,GateWait,GateInvite};
    End *g;
    TlsHeapAlign(16);
    rPush(g);
    g->TlsGate.Static = &Static;
    EndInit(g,g,CurWaker());                /* its own peer: a loopback gate */
    return &g->TlsGate;
}
TlsGate *TlsGatePeer(TlsGate *this) {
    TlsGetThat(End,TlsGate);
    return &that->Peer->TlsGate;
}
 
/*_____________________________________________________________________________
 |
 | Threads
 |_____________________________________________________________________________
 */
TlsMemPool *TlsMemPoolBuffered(TlsMemPool *child,unsigned int buffersize);
 
#define GateHeapGrowth 65536                /* chunk of the root heap of a thread */
#define GateBuffer 65536                    /* bytes of free blocks its pool may hold */
 
static TlsMemPool *SharedPool_;
static __thread TlsGate *Up_;               /* the end of the edge to the parent; NULL on the main thread */
static __thread Waker *Waker_;              /* set for the threads made by TlsGateSet */
static __thread Waker ThreadWaker_;         /* the others */
 
void TlsGateSharedSet(TlsMemPool *Shared) { SharedPool_ = Shared; }
TlsGate *TlsGateUp(void) { return Up_; }
__attribute__((noinline)) static Waker *CurWaker(void) {
    return Waker_ ? Waker_ : &ThreadWaker_;
}
 
/* The edge to a thread. It lives on the heap of the parent, in the layer that
 * holds the hook which shuts the thread down: it is released with the thread,
 * and until then a late Knock of the parent finds a closed inbox. */
typedef struct {
    End P,C;                                /* the parent's end, the child's end */
    Waker CW;                               /* the child thread's */
    pthread_t Tid;
    int Joined;                             /* the parent only */
    unsigned StackSize;
    TlsGateEntry *Entry;
} ThreadEdge;
 
static void ThreadBoot(void *arg) {
    ThreadEdge *t = arg;
    x_(t->Entry,Entry,&t->C.TlsGate);
}
 
static void *ThreadMain(void *arg) {
    ThreadEdge *t = arg;
    TlsMemPool *B = TlsMemPoolBuffered(SharedPool_,GateBuffer);
    TlsStack *s = TlsStackOpen(B,GateHeapGrowth,0);
    TlsHeap *h = TlsStackHeap(s);
    TlsGateNod home;
    x_(h,Set);
    Up_ = &t->C.TlsGate;
    Waker_ = &t->CW;
    TlsGateHomeRun(&home,t->StackSize,ThreadBoot,t);
    ShutInbox(&t->C);                       /* the thread is over: nothing more is served ... */
    DrainReady(&t->C);
    ShutInbox(&t->P);                       /* ... and the parent sees the end of its edge */
    Up_ = 0;
    Waker_ = 0;
    x_(h,Set);                              /* the heap head is stale: make it the root's */
    TlsHeapClose(h);                        /* all the blocks go back to the buffer ... */
    x_(B,Close);                            /* ... and the buffer gives them back to the shared pool */
    return 0;
}
 
static void ThreadShutdown(TlsGate *this);
/* Registered in the stack of the parent: popping the layer that holds the edge
 * shuts the thread down, newest first, so children go before their parents. */
static void ThreadHook(void *arg) {
    ThreadEdge *t = arg;
    ThreadShutdown(&t->P.TlsGate);
}
/* The parent's end: closes the edge, both ways, and waits for the thread. */
static void ThreadShutdown(TlsGate *this) {
    TlsGetThat(ThreadEdge,P.TlsGate);
    assert(!pthread_equal(pthread_self(),that->Tid) && "a thread cannot shut its own gate down");
    ShutInbox(&that->C);
    ShutInbox(&that->P);
    DrainReady(&that->P);                   /* pulled by the parent, never invited: refused */
    if (!that->Joined) {
        pthread_join(that->Tid,0);
        that->Joined = 1;
    }
}
 
TlsGate *TlsGateSet(int StackSize,TlsGateEntry *Entry) {
    static struct TlsGate ParentStatic = {ThreadShutdown,GateKnock,GateCheck,GateWait,GateInvite};
    static struct TlsGate ChildStatic = {GateShutdown,GateKnock,GateCheck,GateWait,GateInvite};
    ThreadEdge *t;
    assert(SharedPool_ && "TlsGateSharedSet must be called before TlsGateSet");
    TlsHeapAlign(16);
    rPush(t);
    t->P.TlsGate.Static = &ParentStatic;
    t->C.TlsGate.Static = &ChildStatic;
    atomic_init(&t->CW.Seq,0);
    atomic_init(&t->CW.Sleeping,0);
    EndInit(&t->P,&t->C,CurWaker());
    EndInit(&t->C,&t->P,&t->CW);
    t->Joined = 0;
    t->StackSize = StackSize;
    t->Entry = Entry;
    if (pthread_create(&t->Tid,0,ThreadMain,t)) {
        x_(&TlsCritErrorShutdown,Signal,"Cannot create a thread.");
        abort();
    }
    TlsHeapHook(ThreadHook,t);
    return &t->P.TlsGate;
}
 
