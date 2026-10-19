/*  Fondement Michtam
 *  Copyright (C) 2024 Xavier Lacroix
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#ifndef _Tools_Core_h_
#define _Tools_Core_h_
 
#include <Tools_Class.h>
 
int TlsLog2(unsigned int v);
 
/*_____________________________________________________________________________
 |
 | Signals are used to allow software components to react to certain
 | inputs.
 |
 | the behaviour of TlsCritErrorShutdown is to write a error message before
 | exiting the program.
 |_____________________________________________________________________________
*/
 
TlsClassDef(TlsSignal) {
    int (*Signal)(TlsSignal *this);
};
TlsClassDef(TlsSignalInt){
    int (*Signal)(TlsSignalInt *this,int Val);
};
TlsClassDef(TlsSignalString){
    int (*Signal)(TlsSignalString *this,char *Msg);
};
 
TlsSignal *TlsSignalMessage(TlsSignalString *Tgt,char *Msg);
 
extern TlsSignalString TlsCritErrorShutdown;
extern TlsSignal *TlsCritErrorOutOfMemory;
 
/*_____________________________________________________________________________
 |
 | Signals are used to allow software components to react to certain
 | inputs.
 |
 | the behaviour of TlsCritErrorShutdown is to write a error message before
 | exiting the program.
 |_____________________________________________________________________________
*/
 
TlsClassDef(TlsIntCounter) {
    int (*Add)(TlsIntCounter *this,int val);
    int (*Set)(TlsIntCounter *this,int val);
};
 
extern TlsIntCounter TlsIntCounterNull;
TlsIntCounter *TlsIntCounterDirect(int val);
TlsIntCounter *TlsIntCounterAccumulate(TlsIntCounter *Acc,int val);
TlsIntCounter *TlsIntCounterWatch(TlsIntCounter *Tgt,int threshold,TlsSignal *signal);
 
/*_____________________________________________________________________________
 |
 | Memory management: we have stacks and pools.
 |_____________________________________________________________________________
*/
 
/*_____________________________________________________________________________
 |
 | Mempool: a pool hands out memory in buckets (sizes classes). Several
 | implementations share this interface, each with its own policy to allocate,
 | keep or release its buckets (malloc/mmap abstraction, size range specialists,
 | selectors over other pools, local pools flushed with a heap layer, ...).
 |
 | A TlsMemPool pointer is a HANDLE on a bucket:
 |    - handles are stable: a handle stays valid, and keeps designating the
 |      same bucket, until the root pool is closed.
 |    - SizeSelect(this,sz) returns the handle of the bucket serving requests
 |      of sz bytes. It can be called on any handle (the root, or another
 |      bucket), it never changes `this`, and the same sz always returns the
 |      same handle. It is typically costly: call it seldom and keep the handle.
 |    - BucketSize(this) is the size really served by the bucket (>= the size
 |      given to SizeSelect), and SizeSelect(h,BucketSize(h)) == h.
 |      A root with not bucket of its own cannot Allocate; the 0-size bucket can.
 |    - Allocate(this) returns a block of BucketSize(this) bytes, aligned as
 |      the ALIGNMENT rule below says. It never returns NULL: failure
 |      is a critical error (TlsCritErrorOutOfMemory).
 |    - Free(this,mem,sz) gives mem back to the bucket it came from: sz is the
 |      size that was given to SizeSelect to get the allocating handle.
 |      `this` may be any handle of the pool; going thru the bucket handle
 |      itself is the fast path.
 |    - Close is called on the root only. It releases every bucket and the
 |      management structures. Blocks still allocated are the caller's affair.
 |    - CountSwitch(this,Swp) makes the pool account its allocated bytes in Swp
 |      from now on, moving what was accumulated so far, and returns the
 |      previous counter. With Swp == NULL it only returns the current one.
 |    A pool is used by one thread at a time unless its constructor says so.
 |
 | ALIGNMENT. Every pool follows the same rule, so that a user (or another
 | pool: the Mini pool finds the header of a page by masking an address) can
 | count on it. A block of BucketSize s, in bytes, is aligned on
 |    - 8 if s is a multiple of 8, else 4 if it is a multiple of 4, else 2 if
 |      it is even (an odd size has no alignment);
 |    - itself if s is a power of 2: a 64 bytes bucket gives blocks aligned on
 |      64, a 4096 one gives pages. The main pool holds it up to 4 MiB; above,
 |      its blocks are aligned on the page (4096) only.
 |    A pool may give more (the main pool aligns on the lowest bit of the size:
 |    48 gives 16), never less. The rule is on the size of the bucket, not on
 |    the size asked to SizeSelect.
 |_____________________________________________________________________________
*/
TlsClassDef(TlsMemPool) {
    void *(*Allocate)(TlsMemPool *this);
    void (*Free)(TlsMemPool *this,void *mem,unsigned int sz);
    TlsMemPool *(*SizeSelect)(TlsMemPool *this,unsigned int sz);
    unsigned int (*BucketSize)(TlsMemPool *this);
    void (*Close)(TlsMemPool *this);
    TlsIntCounter *(*CountSwitch)(TlsMemPool *this,TlsIntCounter *Swp);
};
 
/*_____________________________________________________________________________
 |
 | Stack:
 | Use rPush() to push and object on the current stack; rnPush() to push
 | an array of object on the stack.
 | TlsPush() to push a data on the stack
 | TlsStackFork() to push a stack on the stack.
 | TlsStackLayer() and TlsStackReground() to mark a point in the stack and go back to that
 | point.
 | TlsStackOpen() and TlsStackClose() create and release a stack.
 |_____________________________________________________________________________
*/
 
typedef struct TlsStack TlsStack;
typedef struct TlsStackNod TlsStackNod;
void *TlsPush(TlsStack *Stack,unsigned int sz);
void TlsPushAlign(TlsStack *Stack,unsigned int Align);
TlsStack *TlsStackFork(TlsStack *Root);
TlsStackNod *TlsStackLayer(TlsStack *Stack);
int TlsStackLayerPop(TlsStack *s,TlsStackNod *End);
TlsStack *TlsStackOpen(TlsMemPool *Pool,unsigned int growth,TlsIntCounter *Watch);
TlsMemPool *TlsStackPool(TlsStack *s); /* the root pool the stack was opened with */
/* The stack's live fiber counter (see Fiber management): fibers launched on
 * a heap of this stack and not yet ended or abandoned. It follows the stack,
 * not the thread, so fibers may cross threads (see Tools_Gate.h). */
int *TlsStackLive(TlsStack *s);
/* Hook: fn(arg) is called when the layer holding it is popped (or the stack
 * is closed), in the reverse order of the pushes, like blocks are freed and
 * forked stacks closed. A hook must not push on the stack it is released from. */
void TlsStackHook(TlsStack *s,void (*fn)(void *),void *arg);
void TlsStackClose(TlsStack *Stck);
 
/*_____________________________________________________________________________
 |
 | Creation of object is typically done on pushing them on the Heap via
 | rPush() for an object and rnPush() for an array of object.
 | For performance purpose, we only use two pointers to locate the new object
 | and detect overflow; so a function call is only done when an overflow is
 | detected via, thru the Tls_HeapOverflow_() function.
 | The Heap is associated with a TlsStack.
 | It is possible to change the associated stack via TlsHeapTgtSet(); in that
 | case, the current associated stack; if any is first Synced. And the Heap is
 | made to point to the top of the new stack.
 | When doing extra stuff with the associated
 | stack (like forking, marking a layer, poping layer ...) you need to first
 | sync the heap and the Stack via a call to TlsHeapTgtSync(). Do the operation,
 | and then reupdate the heap pointer via a call to TlsHeapResync();
 |_____________________________________________________________________________
*/
 
TlsClassDef(TlsHeap) {
    void (*Set)(TlsHeap *this);
    void (*Sync)(TlsHeap *this);
    void *(*Ovf)(TlsHeap *this,void *Data);
    TlsStack *(*GetStack)(TlsHeap *this);
};
TlsHeap *TlsStackHeap(TlsStack *s);
TlsHeap *TlsBufferHeap(void *b,void *e);
extern __thread struct TlsHeapHead {
    TlsHeap *Tgt;
    void *p,*e;
} TlsHeap_;
void TlsHeapAlign(unsigned int Align);
TlsStackNod *TlsHeapLayer(void);
int TlsHeapLayerPop(TlsStackNod *l);
TlsHeap *TlsHeapFork(void);
void TlsHeapClose(TlsHeap *Heap);
void TlsHeapHook(void (*fn)(void *),void *arg);   /* TlsStackHook on the current heap */
#define rPush(v) { v=TlsHeap_.p; TlsHeap_.p=v+1;\
    if((TlsHeap_.e-TlsHeap_.p)<0){v = x_(TlsHeap_.Tgt,Ovf,v);}\
}
#define rnPush(v,n) { v=TlsHeap_.p; TlsHeap_.p=v+n;\
    if((TlsHeap_.e-TlsHeap_.p)<0){v = x_(TlsHeap_.Tgt,Ovf,v);}\
}
 
#define TlsHeapIn(_nHeap_) { \
    TlsHeap *_oHeap_; \
    x_(TlsHeap_.Tgt,Sync); _oHeap_ = TlsHeap_.Tgt; x_(_nHeap_,Set);
#define TlsHeapOut \
    x_(TlsHeap_.Tgt,Sync); x_(_oHeap_,Set);\
}
void TlsMainHeapOpen(int growth);
void TlsMainHeapClose(void);
 
/*________________________________________________________________________
 |
 | Extra components
 |________________________________________________________________________
*/
 
typedef struct TlsBNod TlsBNod;
#define TlsBNodEnd ((TlsBNod *)(0))
TlsClassDef(TlsBNodProcess){
    int (*found)(TlsBNodProcess *this,TlsBNod *nod,unsigned int Key);
};
TlsClassDef(TlsBTree){
    int (*NodNumbers)(TlsBTree *this);
    unsigned int (*GetKey)(TlsBTree *this,TlsBNod *iKey);
    TlsBNod *(*Seek)(TlsBTree *this,unsigned int Key);
    TlsBNod *(*Insert)(TlsBTree *this,TlsBNod *n,unsigned int Key);
    TlsBNod *(*Remove)(TlsBTree *this,unsigned int Key);
    struct {
        TlsBNod *(*ForEach)(TlsBTree *this,TlsBNodProcess *Found);
        TlsBNod *(*ForEachFrom)(TlsBTree *this,unsigned int StartKey,TlsBNodProcess *Found);
    } Fwd,Bck;
    void (*Clear)(TlsBTree *this,TlsBNodProcess *Remove);
};
/* 31 radix trees use a key of 31 bits, the 5 remaining bits are used internally
Use Tls31RadixInit(&root,s_(TlsBNod));
31 Radix needs s_(TlsBNod) of at least 16 bytes or a fatal error will occur.
*/
typedef struct {
    TlsBTree TlsBTree;
    TlsBNod *Root;
    unsigned int foo;
} TlsBTreeRoot;
TlsBTree *Tls31RadixInit(TlsBTreeRoot *r,int NodSz);
/* Lowest: get the smallest value greater or egal to "from" and return it in from.
as well as the associated Nod. returns &TlsBNodEnd if nothing is found. */
TlsBNod *TlsBNodGetLowest(TlsBTree *rt,unsigned int *from);
TlsBNod *TlsBNodGetHighest(TlsBTree *rt,unsigned int *from);
 
/*________________________________________________________________________
 |
 |  Fiber management:
 |
 | Concept:
 |  * Fibers are tied to their private execute stack and private execute Heap.
 | They are spawned with the TlsFiberLaunch primitive that push the local heap
 | on the current heap, and then push the local stack on the local heap, before
 | calling the entry point. A FiberNod is also spawned in the process as a mean
 | to return to the creator fiber.
 |  * FiberNod are switch points between fibers, and are in limited
 | numbers (one per fiber, created by TlsFiberLaunch). They are also tied to a target
 | fiber, that is the last fiber they left. A call to FiberSwitch will
 | move to that target fiber, AND, change the target of the FiberNod to the
 | calling fiber.
 |  * TlsFiber can be tied to a FiberNod and act as handle to them. You
 | can move one FiberNod from one TlsFiber to another using
 | TlsFiberNodMove.
 |  * TlsFiberSwitch switch to the fiber targeted by the attached FiberNod.
 | They return a TlsFiber chosen by the returning fiber; which can be embedded
 | within a structure giving extra protocol data.
 |
 | Launching:
 |  * The stack size must be at least 16 KiB. TlsFiberLaunch returns the slot
 | control first comes back through (like TlsFiberSwitch), or NULL if the
 | fiber could not be bootstrapped; in that case nothing ran and org is left
 | empty (its heap and stack stay on the current heap until its layer pops).
 |
 | Ending:
 |  * When the entry returns, the fiber ends: the context of its parent is
 | resumed through the slot it was parked in, and that slot is left empty
 | (SwitchNod == NULL). That is how the parent knows the fiber is over.
 |  * Switching to an empty slot is an error (checked by assert). A fiber that
 | wants zombie semantics never returns from its entry and loops on
 | TlsFiberSwitch(parent) by itself; it is then the application's business.
 |
 | Abandoning:
 |  * TlsFiberAbandon discards a parked fiber that will never be resumed and
 | empties its slot. It is only valid on a fiber that has not ended: an ended
 | fiber is already accounted for. A fiber that deliberately never ends (see
 | above) must be abandoned by its parent before the layer that contains it
 | is popped.
 |
 | Lifetime:
 |  * The slot of the fiber, its nod, its stack and its heap all live in the
 | block of the launcher: popping a layer (TlsHeapLayerPop) while a fiber
 | launched inside it is still parked is an error. In debug builds, a live
 | fiber counter, kept per stack (TlsStackLive), is compared at each layer pop
 | to catch most of these cases.
 |  * Each fiber owns its Heap: The running Heap is synced before
 | TlsFiberSwitch call and is Set after it returns.
 | Only the fiber itself may layer, fork or pop its own heap.
 |________________________________________________________________________
*/
 
typedef struct TlsFiberNod TlsFiberNod;
typedef struct { TlsFiberNod *SwitchNod; } TlsFiber;
 
TlsFiber *TlsFiberLaunch(unsigned stack_sz, void (*entry)(TlsFiber *parent), TlsFiber *org);
TlsFiber *TlsFiberSwitch(TlsFiber *next);
void      TlsFiberNodMove(TlsFiber *dst, TlsFiber *src);
void      TlsFiberAbandon(TlsFiber *slot);   /* discard a parked, unfinished fiber */
 
#endif

