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
typedef struct Gate Gate;
struct TlsGateNod {
    fiber f;
    TlsGateGuest *Guest;                    /* the mailbox: what the resumer passes */
    TlsGateNod *Next;                       /* link in the inbox of a gate */
    Gate *Out;                              /* request to hand over, for P */
    atomic_uint Word;                       /* 1: P must resume the client process */
    int Side;
    void (*BootFn)(void *);
    void *BootArg;
};
struct Gate {
    TlsGate TlsGate;
    _Atomic(TlsGateNod *) Head;             /* inbox, newest first; SHUT once shut */
    atomic_uint Seq;                        /* changes at each arrival */
    atomic_uint Sleeping;                   /* the server is about to sleep on Seq, or does */
    TlsGateNod *Ready;                      /* server only: pulled, oldest first */
};
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
static void Publish(Gate *g,TlsGateNod *n) {
    TlsGateNod *h = atomic_load_explicit(&g->Head,memory_order_relaxed);
    do {
        if (h==SHUT) { Release(n,EndShut); return; }
        n->Next = h;
    } while (!atomic_compare_exchange_weak_explicit(&g->Head,&h,n,
                memory_order_release,memory_order_relaxed));
    atomic_fetch_add_explicit(&g->Seq,1,memory_order_release);
    atomic_thread_fence(memory_order_seq_cst);          /* pairs with the one in Wait */
    if (atomic_load_explicit(&g->Sleeping,memory_order_relaxed))
        SyncOps->Wake(&g->Seq);             /* a busy server is not woken */
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
        Gate *g = n->Out;
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
 | Gate
 |_____________________________________________________________________________
 */
 
static TlsGateGuest *GateKnock(TlsGate *this,TlsGateGuest *request) {
    TlsGetThat(Gate,TlsGate);
    TlsGateNod *n = Home_;
    TlsHeap *mine;
    assert(n && "Knock: this thread has no home");
    if (atomic_load_explicit(&that->Head,memory_order_acquire)==SHUT) return EndShut;
    request->Nod = 0;
    n->Guest = request;
    n->Out = that;
    n->Side = 1;
    mine = TlsHeap_.Tgt;
    x_(mine,Sync);
    fiber_switch(&n->f);                    /* park here, P runs */
    x_(mine,Set);                           /* resumed, on the server thread if merged */
    return n->Guest;
}
 
/* Server only: moves the arrivals to the ready list, oldest first. */
static int Pull(Gate *g) {
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
 
static int GateCheck(TlsGate *this) {
    TlsGetThat(Gate,TlsGate);
    return Pull(that);
}
 
static int GateWait(TlsGate *this) {
    TlsGetThat(Gate,TlsGate);
    for (;;) {
        unsigned s;
        if (Pull(that)) return 1;
        if (atomic_load_explicit(&that->Head,memory_order_acquire)==SHUT) return 0;
        s = atomic_load_explicit(&that->Seq,memory_order_acquire);
        atomic_store_explicit(&that->Sleeping,1,memory_order_relaxed);
        atomic_thread_fence(memory_order_seq_cst);      /* either the publisher sees Sleeping, or we see its request */
        if (Pull(that) ||
            atomic_load_explicit(&that->Head,memory_order_acquire)==SHUT) {
            atomic_store_explicit(&that->Sleeping,0,memory_order_relaxed);
            return Pull(that);              /* arrived since the first look, or shut */
        }
        SyncOps->Block(&that->Seq,s);
        atomic_store_explicit(&that->Sleeping,0,memory_order_relaxed);
    }
}
 
static TlsGateGuest *GateInvite(TlsGate *this) {
    TlsGetThat(Gate,TlsGate);
    TlsGateNod *n;
    if (!Pull(that)) return 0;
    n = that->Ready;
    that->Ready = n->Next;
    n->Next = 0;
    n->Guest->Nod = n;
    return n->Guest;
}
 
static void GateShutdown(TlsGate *this) {
    TlsGetThat(Gate,TlsGate);
    TlsGateNod *h = atomic_exchange_explicit(&that->Head,SHUT,memory_order_acq_rel);
    if (h==SHUT) return;
    while (h) {
        TlsGateNod *n = h->Next;            /* before the client can reuse it */
        Release(h,EndShut);
        h = n;
    }
    atomic_fetch_add_explicit(&that->Seq,1,memory_order_release);
    SyncOps->Wake(&that->Seq);
}
 
TlsGate *TlsGateOpen(void) {
    static struct TlsGate Static = {GateShutdown,GateKnock,GateCheck,GateWait,GateInvite};
    Gate *g;
    TlsHeapAlign(16);
    rPush(g);
    g->TlsGate.Static = &Static;
    atomic_init(&g->Head,0);
    atomic_init(&g->Seq,0);
    atomic_init(&g->Sleeping,0);
    g->Ready = 0;
    return &g->TlsGate;
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
static __thread TlsGate *Self_;
 
void TlsGateSharedSet(TlsMemPool *Shared) { SharedPool_ = Shared; }
TlsGate *TlsGateSelf(void) { return Self_; }
void TlsGateSelfSet(TlsGate *g) { Self_ = g; }
 
typedef struct {
    Gate G;
    pthread_t Tid;
    int Joined;                             /* the parent only */
    unsigned StackSize;
    TlsGateEntry *Entry;
    TlsGate *Parent;
} ThreadGate;
 
static void ThreadBoot(void *arg) {
    ThreadGate *t = arg;
    x_(t->Entry,Entry,t->Parent);
}
 
/* The thread is over: what nobody served is refused, what arrives later too. */
static void Drain(Gate *g) {
    GateShutdown(&g->TlsGate);
    while (g->Ready) {
        TlsGateNod *n = g->Ready;
        g->Ready = n->Next;
        n->Next = 0;
        Release(n,EndShut);
    }
}
 
static void *ThreadMain(void *arg) {
    ThreadGate *t = arg;
    TlsMemPool *B = TlsMemPoolBuffered(SharedPool_,GateBuffer);
    TlsStack *s = TlsStackOpen(B,GateHeapGrowth,0);
    TlsHeap *h = TlsStackHeap(s);
    TlsGateNod home;
    x_(h,Set);
    Self_ = &t->G.TlsGate;
    TlsGateHomeRun(&home,t->StackSize,ThreadBoot,t);
    Drain(&t->G);
    Self_ = 0;
    x_(h,Set);                              /* the heap head is stale: make it the root's */
    TlsHeapClose(h);                        /* all the blocks go back to the buffer ... */
    x_(B,Close);                            /* ... and the buffer gives them back to the shared pool */
    return 0;
}
 
static void ThreadShutdown(TlsGate *this);
/* Registered in the stack of the parent: popping the layer that holds the gate
 * shuts the thread down, newest first, so children go before their parents. */
static void ThreadHook(void *arg) {
    ThreadGate *t = arg;
    ThreadShutdown(&t->G.TlsGate);
}
static void ThreadShutdown(TlsGate *this) {
    TlsGetThat(ThreadGate,G.TlsGate);
    assert(!pthread_equal(pthread_self(),that->Tid) && "a thread cannot shut its own gate down");
    GateShutdown(this);
    if (!that->Joined) {
        pthread_join(that->Tid,0);
        that->Joined = 1;
    }
}
 
TlsGate *TlsGateSet(int StackSize,TlsGateEntry *Entry) {
    static struct TlsGate Static = {ThreadShutdown,GateKnock,GateCheck,GateWait,GateInvite};
    ThreadGate *t;
    assert(SharedPool_ && "TlsGateSharedSet must be called before TlsGateSet");
    TlsHeapAlign(16);
    rPush(t);
    t->G.TlsGate.Static = &Static;
    atomic_init(&t->G.Head,0);
    atomic_init(&t->G.Seq,0);
    atomic_init(&t->G.Sleeping,0);
    t->G.Ready = 0;
    t->Joined = 0;
    t->StackSize = StackSize;
    t->Entry = Entry;
    t->Parent = Self_;
    if (pthread_create(&t->Tid,0,ThreadMain,t)) {
        x_(&TlsCritErrorShutdown,Signal,"Cannot create a thread.");
        abort();
    }
    TlsHeapHook(ThreadHook,t);
    return &t->G.TlsGate;
}
 
