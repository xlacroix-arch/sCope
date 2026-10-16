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
 
/* Tls fibers: see Tools_Core.h for the concepts, fiber.h for the primitives.
 *
 * A TlsFiberNod is the raw rendezvous slot of fiber.h plus a back pointer to
 * the TlsFiber currently designating it. It lives in the heap of the fiber it
 * was created for, together with that fiber's stack:
 *
 *      child heap:  [ ... TlsFiberNod ... stack ... ]
 *
 * The heap of a fiber is the one that is current when it switches; it is Synced
 * on the way out of TlsFiberSwitch and Set again on the way back in, so that
 * a switch inside a TlsHeapIn()/TlsHeapOut() block is safe and any fiber can
 * allocate in any heap it can reach: the heap object, not the thread head, is
 * the source of truth while a fiber is parked.
 */
 
#include <assert.h>
#include <Tools_Core.h>
#include "fiber.h"
 
#define TlsFiberMinStack 16384
 
/* The live count is that of the stack the fiber was launched on (TlsStackLive),
 * and the nod remembers it: a fiber ends or is abandoned on whatever thread it
 * runs, but always against the stack it was born on. */
static inline void TlsLiveInc(int *c) { (*c)++; }
static inline void TlsLiveDec(int *c) { assert(*c>0); (*c)--; }
 
struct TlsFiberNod {
    fiber fiber;            /* raw slot: the parked context lives here */
    TlsFiber *Fiber;        /* the TlsFiber designating this nod, if any */
    int *Live;              /* live count of the stack it was launched on */
};
 
static inline TlsFiber *SlotOf(fiber *r) { return v_(TlsFiberNod,fiber,r)->Fiber; }
 
/*_____________________________________________________________________________
 |
 | Switch, move, abandon
 |_____________________________________________________________________________
*/
 
TlsFiber *TlsFiberSwitch(TlsFiber *next) {
    TlsHeap *mine = TlsHeap_.Tgt;           /* the heap this context is running on */
    assert(next->SwitchNod && next->SwitchNod->Fiber==next);
    x_(mine,Sync);
    fiber *r = fiber_switch(&next->SwitchNod->fiber);
    x_(mine,Set);                           /* back: p and e from the heap itself */
    return SlotOf(r);
}
 
void TlsFiberNodMove(TlsFiber *dst,TlsFiber *src) {
    assert(src->SwitchNod && src->SwitchNod->Fiber==src);
    if (dst==src) return;
    dst->SwitchNod = src->SwitchNod;
    dst->SwitchNod->Fiber = dst;            /* the back pointer follows the slot */
    src->SwitchNod = 0;                     /* stale use trips the assert above */
}
 
void TlsFiberAbandon(TlsFiber *slot) {
    assert(slot->SwitchNod && slot->SwitchNod->Fiber==slot);
    TlsLiveDec(slot->SwitchNod->Live);      /* it will never end by itself */
    slot->SwitchNod = 0;
}
 
/*_____________________________________________________________________________
 |
 | Launch
 |_____________________________________________________________________________
*/
 
/* Lives in the frame of TlsFiberLaunch, which stays parked until tls_boot has
 * read it; it is the raw "org" of the launch, as everything else must come
 * from it. */
struct fiber_boot {
    fiber fiber;
    TlsFiberNod *Nod;
    void (*entry)(TlsFiber *parent);
};
 
static void tls_boot(fiber *p) {
    struct fiber_boot *b = v_(struct fiber_boot,fiber,p);
    TlsFiberNod *nod = b->Nod;                  /* all read before the first switch: */
    void (*entry)(TlsFiber *) = b->entry;       /* b is gone when the parent returns */
    TlsFiber *org = nod->Fiber;
 
    fiber_move(&nod->fiber,p);                  /* the parent is now parked in the nod */
    entry(org);
    assert(org->SwitchNod==nod && "a fiber must hand its parent back before returning");
    org->SwitchNod = 0;                         /* ended: the slot holds nothing */
    TlsLiveDec(nod->Live);
    fiber_exit(&nod->fiber);                    /* resume the parent through the nod */
}
 
TlsFiber *TlsFiberLaunch(unsigned stack_sz,void (*entry)(TlsFiber *parent),TlsFiber *org) {
    struct fiber_boot boot;
    struct fiber_stack st;
    TlsFiberNod *nod;
    char *stk;
    fiber *r;
    TlsHeap *child,*mine;
    int *live = TlsStackLive(x_(TlsHeap_.Tgt,GetStack));
 
    assert(stack_sz>=TlsFiberMinStack);
    child = TlsHeapFork();                      /* a heap inside the current one ... */
    mine = TlsHeap_.Tgt;                        /* ... which stays the current heap */
    x_(child,Set);
 
    TlsHeapAlign(16);
    rPush(nod);                                 /* nod and stack: the first objects of */
    rnPush(stk,stack_sz);                       /* the child heap, so they live and die with it */
    nod->Fiber = org;
    nod->Live = live;
    org->SwitchNod = nod;
 
    boot.Nod = nod;
    boot.entry = entry;
    st.start = stk;
    st.end = stk+stack_sz;
    TlsLiveInc(live);
    r = fiber_launch(&st,tls_boot,&boot.fiber);
    x_(mine,Set);                               /* the parent is back on its own heap */
    if (!r) {                                   /* bootstrap failed: nothing ran. The child */
        TlsLiveDec(live);                       /* heap and stack stay until the layer pops */
        org->SwitchNod = 0;
        return 0;
    }
    return SlotOf(r);
}
