/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */
 
#ifndef _Tools_Gate_h_
#define _Tools_Gate_h_
#include <Tools_Class.h>
#include <Tools_Core.h>
 
/*_____________________________________________________________________________
 |
 | Gates: how threads talk to each other.
 |
 | A gate is the edge between two threads of the tree of threads (a parent and
 | one of its children): N threads, N-1 gates, and the main thread has none
 | above it. A TlsGate is one END of that edge; each thread holds its own, and
 | the two ends know each other (TlsGatePeer). Each end has an inbox: the
 | requests addressed to it.
 |   * Knock(end,request) posts to the inbox of the PEER of that end, so the
 |     same five calls serve both directions: a parent knocks on the end of
 |     its child, a child knocks on its Up. The calling thread is suspended
 |     until the request is over. Only one Knock per thread is outstanding.
 |   * Wait (blocking), Check (polling) and Invite work on the inbox of the end
 |     itself: the requests the peer sent. Check/Wait tell that one is
 |     pending, Invite takes the oldest: it returns the request given to
 |     Knock, now tied to the client by its Nod. A thread that serves several
 |     ends (its children, its Up) waits on all of them at once with
 |     TlsGateWaitAny.
 |   * The two sides then merge. The server holds the client process parked
 |     in a rendezvous slot (TlsGateNod); TlsGateGuestSwitch(Target,ret) swaps
 |     the running context with the one parked in Target->Nod, on the thread
 |     of the caller. Target is consumed, and ret (which becomes the slot the
 |     other side must use to come back) is tied to the same Nod. The value
 |     returned is the ret given by the counterpart's own switch. The first
 |     counterpart of a server GuestSwitch is the Knock of the client, which
 |     returns that ret.
 |   * TlsGateGuestSendOff(Target) ends the request, from either side.
 |     Server side (the client process is parked in Target->Nod, wherever it
 |     went since Knock): the client is resumed on its own thread, by the
 |     parking fiber of that thread, and the call returns at once. Client
 |     side (merged, running on the server thread): it is a switch to the
 |     server with a NULL; the server's GuestSwitch returns NULL, releases the
 |     client, and the client's SendOff returns on its own thread. A NULL
 |     returned by a server GuestSwitch therefore means "ended by the client".
 |     What the resumed Knock, GuestSwitch or SendOff of the client sees is
 |     nothing, or the TlsGateEnd of the gate if the request was refused.
 |   * Shutdown on an end closes ITS inbox: the requests that are not yet
 |     invited, and the next ones, are refused with &TlsGateEndShutdown.Guest.
 |     The server keeps serving what it has already pulled with Check/Wait.
 |     Shutdown on the parent end of a thread edge is the removal of the
 |     thread, see Threads below.
 |
 | Rules:
 |   * Check, Wait and Invite of an end are for one server thread at a time,
 |     the one that holds it. Knock and Shutdown are for any thread.
 |   * The threads form a strict tree for now: a thread only talks to its
 |     parent and its children. Siblings go through their parent.
 |   * GuestSwitch requires a ret; only SendOff passes NULL.
 |   * A client process must not end while it is parked or merged.
 |   * A merged client process runs on the server thread: the code between
 |     Knock and its GuestSwitch back must reach the thread local heap head
 |     (TlsHeap_) through a fresh access, i.e. not cache its address across
 |     the call. Compile with -ftls-model=initial-exec or local-exec.
 |   * A thread must have a home (see TlsGateHomeRun) to Knock.
 |_____________________________________________________________________________
 */
 
typedef struct TlsGateNod TlsGateNod;
typedef struct {TlsGateNod *Nod;} TlsGateGuest;
typedef struct {
    TlsGateGuest Guest;
    int Status;
} TlsGateEnd;
extern TlsGateEnd TlsGateEndShutdown;       /* Status 1 */
 
TlsGateGuest *TlsGateGuestSwitch(TlsGateGuest *Target,TlsGateGuest *ret);
void TlsGateGuestSendOff(TlsGateGuest *Target);
 
TlsClassDef(TlsGate) {
    void (*Shutdown)(TlsGate *this);
    TlsGateGuest *(*Knock)(TlsGate *this,TlsGateGuest *request);
    int (*Check)(TlsGate *this);
    int (*Wait)(TlsGate *this);
    TlsGateGuest *(*Invite)(TlsGate *this);
};
/* A loopback gate, on the current heap (it must outlive its users): an end
 * that is its own peer, so what is knocked on it is served by the same thread's
 * own server loop (a process of that thread, or a fiber scheduler: the tests). */
TlsGate *TlsGateOpen(void);
/* The other end of the edge. */
TlsGate *TlsGatePeer(TlsGate *End);
/* Waits until one of the n ends (1 to 16, all held by the calling thread) has a
 * request: returns its index, the request is then served with Invite on that
 * end. Returns -1 when none has one and some end is shut (TlsGateIsShut): the
 * caller finds which, and drops it from its array, or ends. Pending requests
 * are always reported first. */
int TlsGateWaitAny(TlsGate **Ends,int n);
/* 1 if the inbox of the end is shut and has nothing left to serve: what Wait
 * reports as 0. */
int TlsGateIsShut(TlsGate *End);
 
/* Gives the calling thread its home and runs entry(arg) as its first process on
 * a forked heap and a stack of stack_sz bytes (at least 16 KiB). It returns when
 * entry returns. While the process is parked in a Knock the thread blocks here. */
void TlsGateHomeRun(TlsGateNod *Home,unsigned stack_sz,void (*entry)(void *),void *arg);
unsigned TlsGateHomeSize(void);             /* room for a TlsGateNod */
 
/*_____________________________________________________________________________
 |
 | Threads.
 |
 | TlsGateSet(StackSize,Entry) spawns a child thread and returns the PARENT end
 | of the edge to it (its Peer is the child's end). The child
 |   * gets its own root heap, whose pool is a TlsMemPoolBuffered over the shared
 |     pool given to TlsGateSharedSet (which must be called first, once);
 |   * runs Entry->Entry(Entry,Up) as its first process, on an execution stack
 |     of StackSize bytes; Up is its end of the edge, also given by TlsGateUp()
 |     (NULL on a thread that has no parent: the main thread, see Tools_Main.h);
 |   * ends when Entry returns. Both inboxes of the edge are then shut: what
 |     is still waiting is refused, and so is what is knocked later.
 | The edge (both ends, the child's waker) is pushed on the heap of the parent,
 | next to the hook that removes the thread: it lives exactly as long as the
 | thread does, and until then a late Knock finds a closed inbox, never freed
 | memory. The child can shut its own end (Shutdown on Up) to stop being
 | served: the parent's next Knock is refused.
 | Shutdown on the parent end is the removal of the thread: it shuts both
 | inboxes, then waits for the thread to end (so Entry must return once its
 | Waits say it is shut) and releases it. It is also done by itself when the
 | heap layer that holds the edge is popped (the hook), newest first: a thread
 | is removed before the thread that spawned it, whose own pop removes its
 | children. The hook runs while the stack is being released: whatever the
 | thread uses from the parent heap, the Entry object included, must have been
 | pushed before TlsGateSet and must outlive the thread: put it on the heap
 | (rPush), not in the frame of a function that may return before the join.
 | Shutdown of the parent end can still be called earlier, by the parent only.
 | The parent end is served by the thread that called TlsGateSet.
 |_____________________________________________________________________________
 */
TlsClassDef(TlsGateEntry) {
    void (*Entry)(TlsGateEntry *this,TlsGate *Up);
};
void TlsGateSharedSet(TlsMemPool *Shared);
TlsGate *TlsGateUp(void);              /* the end of the edge to the parent; NULL for the main thread */
TlsGate *TlsGateSet(int StackSize,TlsGateEntry *Entry);
 
#endif

