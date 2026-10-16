
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
 | A TlsGate is a mailbox with a server side and a client side.
 |   * A client calls Knock(gate,request): its whole thread is suspended until
 |     the request is over. Only one Knock per thread is outstanding at a time.
 |   * The server calls Wait (blocking) or Check (polling) to know if a request
 |     is pending, then Invite to take the oldest one: Invite returns the
 |     request given to Knock, now tied to the client by its Nod.
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
 |   * Shutdown refuses the requests that are not yet invited, and the next
 |     ones, with &TlsGateEndShutdown.Guest. The server keeps serving what it
 |     has already pulled with Check/Wait.
 |
 | Rules:
 |   * Check, Wait and Invite are for one server thread at a time.
 |     Knock, Shutdown are for any thread.
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
/* A gate, on the current heap: it must outlive its users. */
TlsGate *TlsGateOpen(void);
 
/* Gives the calling thread its home and runs entry(arg) as its first process on
 * a forked heap and a stack of stack_sz bytes (at least 16 KiB). It returns when
 * entry returns. While the process is parked in a Knock the thread blocks here. */
void TlsGateHomeRun(TlsGateNod *Home,unsigned stack_sz,void (*entry)(void *),void *arg);
unsigned TlsGateHomeSize(void);             /* room for a TlsGateNod */
 
/*_____________________________________________________________________________
 |
 | Threads.
 |
 | TlsGateSet(StackSize,Entry) spawns a thread and returns its gate: the thread
 | is the server of that gate. The thread
 |   * gets its own root heap, whose pool is a TlsMemPoolBuffered over the shared
 |     pool given to TlsGateSharedSet (which must be called first, once);
 |   * runs Entry->Entry(Entry,Parent) as its first process, on an execution stack
 |     of StackSize bytes; Parent is the gate of the thread that spawned it
 |     (NULL if that one has none); TlsGateSelf() is the gate this thread serves;
 |   * ends when Entry returns. Requests that are still waiting are then refused.
 | Shutdown on a thread gate refuses the requests as usual, then waits for the
 | thread to end (so Entry must return once Wait says 0), and releases it.
 | It is also done by itself when the heap layer that holds the gate is popped
 | (a hook in the stack of the parent), newest first: a thread is shut down
 | before the thread that spawned it, whose own pop shuts its children. The
 | hook runs while the stack is being released: whatever the thread uses from
 | the parent heap, the Entry object included, must have been pushed before
 | TlsGateSet and must outlive the thread: put it on the heap (rPush), not in
 | the frame of a function that may return before the join. Shutdown can still
 | be called earlier, by the parent only.
 |_____________________________________________________________________________
 */
TlsClassDef(TlsGateEntry) {
    void (*Entry)(TlsGateEntry *this,TlsGate *Parent);
};
void TlsGateSharedSet(TlsMemPool *Shared);
TlsGate *TlsGateSelf(void);
/* For a thread that is not made by TlsGateSet (the main thread, see Tools_Main.h):
 * declares the gate it serves, so that the threads it spawns get it as Parent. */
void TlsGateSelfSet(TlsGate *g);
TlsGate *TlsGateSet(int StackSize,TlsGateEntry *Entry);
 
#endif
 
