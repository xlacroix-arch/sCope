/* fiber.c - see fiber.h.
 *
 * Switching is plain _setjmp/_longjmp in every configuration. Only the
 * *bootstrap* (getting execution onto a fresh stack the first time) is
 * platform-specific, and is isolated in boot_prepare()/boot_enter():
 *
 *   default                 sigaltstack + signal trick (POSIX, no ucontext)
 *   -DFIBER_USE_UCONTEXT    getcontext/makecontext/setcontext
 *
 * Build notes: use -U_FORTIFY_SOURCE (fortified longjmp refuses cross-stack
 * jumps). Sanitizers and shadow-stack (CET) setups need care.
 */
#define _GNU_SOURCE
#include <Tools_Class.h>
#include "fiber.h"

#include <assert.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Private saved-context record: a local of the suspended fiber_switch /
 * launch call. A slot's `caller` points to its `fiber` member. Slots can be
 * moved (fiber_move), so there is deliberately no link back from the record
 * to "its" slot. */
struct fiber_jmp {
    fiber   fiber;                          /* unused; a record is-a fiber */
    jmp_buf jmp;
    fiber  *volatile via;                   /* written by the RESUMER: the slot it switched through */
    volatile int live;                      /* debug: 1 while parked; volatile so the "resumed" store survives the frame */
};

/* Bootstrap handoff only: written by launch, read once by the trampoline. */
static _Thread_local void (*boot_entry)(fiber *);
static _Thread_local fiber *boot_parent;

static inline void park_init(struct fiber_jmp *j, fiber *slot)
{
    j->fiber.caller = NULL;
    j->via  = slot;
    j->live = 1;
}

/* ---- switch -------------------------------------------------------- */

fiber *fiber_switch(fiber *next)
{
    struct fiber_jmp here;                  /* our context: a local of this call */
    assert(next->caller);                   /* empty or moved-from slot? */
    struct fiber_jmp *to = v_(struct fiber_jmp, fiber, next->caller);
    assert(to->live);                       /* a parked, not yet resumed context */

    park_init(&here, next);
    next->caller = &here.fiber;             /* slot -> our context: swapped in */
    to->via = next;                         /* tell the target which slot we used */

    if (_setjmp(here.jmp) == 0)
        _longjmp(to->jmp, 1);
    here.live = 0;                          /* running again */
    return here.via;                        /* the slot the resumer switched through */
}

/* ---- move ---------------------------------------------------------- */

void fiber_move(fiber *dst, fiber *src)
{
    assert(src->caller);                    /* src must hold a parked context */
    if (dst == src) return;
    dst->caller = src->caller;              /* the context now lives in dst... */
    src->caller = NULL;                     /* ...and src is empty: stale use asserts */
}

/* ---- exit ---------------------------------------------------------- */

/* One-way jump: nothing is parked, the slot is emptied, and the context
 * that was parked in it runs. Reached when an entry returns, or called
 * directly to end the fiber early (no unwinding of its frames). */
void fiber_exit(fiber *parent)
{
    assert(parent->caller);                 /* the parent's context must be in the slot */
    struct fiber_jmp *to = v_(struct fiber_jmp, fiber, parent->caller);
    assert(to->live);                       /* a parked, not yet resumed context */

    parent->caller = NULL;                  /* the slot is empty: it holds no context */
    to->via = parent;                       /* the parent sees the slot it was parked in */
    _longjmp(to->jmp, 1);
}

static void trampoline(void)
{
    fiber *parent = boot_parent;
    boot_entry(parent);
    fiber_exit(parent);                     /* finished: never returns */
}

/* ---- bootstrap backends --------------------------------------------
 * boot_prepare(lo, size): arrange for boot_enter() to start trampoline()
 *                         on the stack [lo, lo+size). 0 on success.
 * boot_enter():           does not return.
 */
#ifdef FIBER_USE_UCONTEXT

#include <ucontext.h>
static _Thread_local ucontext_t boot_uc;

static int boot_prepare(void *lo, size_t size)
{
    if (getcontext(&boot_uc)) return -1;
    boot_uc.uc_stack.ss_sp   = lo;
    boot_uc.uc_stack.ss_size = size;
    boot_uc.uc_link          = NULL;        /* trampoline never returns */
    makecontext(&boot_uc, trampoline, 0);
    return 0;
}
static void boot_enter(void) { setcontext(&boot_uc); }

#else /* sigaltstack backend */

#include <pthread.h>
#include <signal.h>

#ifndef FIBER_SIGNAL
#define FIBER_SIGNAL SIGUSR2                /* any signal the app does not use */
#endif

/* The handler/signal disposition is process-wide, the rest is per thread:
 * serialize the short install/raise/restore window across threads. */
static pthread_mutex_t boot_lock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local jmp_buf          boot_ctx;
static _Thread_local volatile int     booted;

/* Runs on the new stack (SA_ONSTACK). Capture a context *inside this frame*
 * and return normally, so the kernel restores mask and thread state. The
 * frame stays intact on the child stack; a later _longjmp(boot_ctx) resumes
 * here as ordinary code, no longer "in a signal", and calls trampoline().
 * (Jumping back into a frame whose function returned is outside ISO C, but
 * it is the classic, widely used technique and works on POSIX systems.) */
static void boot_handler(int sig)
{
    (void)sig;
    booted = 1;
    if (_setjmp(boot_ctx) == 0)
        return;
    trampoline();                           /* never returns */
}

static int boot_prepare(void *lo, size_t size)
{
    stack_t ss, old_ss;
    struct sigaction sa, old_sa;
    sigset_t unblock, old_mask;
    int rc = -1;

    ss.ss_sp = lo; ss.ss_size = size; ss.ss_flags = 0;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = boot_handler;
    sa.sa_flags   = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    if (sigaltstack(&ss, &old_ss) != 0) return -1;
    pthread_mutex_lock(&boot_lock);
    if (sigaction(FIBER_SIGNAL, &sa, &old_sa) == 0) {
        sigemptyset(&unblock);
        sigaddset(&unblock, FIBER_SIGNAL);
        pthread_sigmask(SIG_UNBLOCK, &unblock, &old_mask);
        booted = 0;
        raise(FIBER_SIGNAL);                /* handler runs now, on `lo` stack */
        pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
        sigaction(FIBER_SIGNAL, &old_sa, NULL);
        rc = booted ? 0 : -1;
    }
    pthread_mutex_unlock(&boot_lock);
    sigaltstack(&old_ss, NULL);             /* the stack is ours, not the signal's */
    return rc;
}
static void boot_enter(void) { _longjmp(boot_ctx, 1); }

#endif

/* ---- launch -------------------------------------------------------- */

/* Kept separate so no local of fiber_launch lives across the setjmp. */
static fiber *enter_child(fiber *org)
{
    struct fiber_jmp here;
    park_init(&here, org);
    org->caller = &here.fiber;              /* parent is parked in org */
    if (_setjmp(here.jmp) == 0)
        boot_enter();
    here.live = 0;
    return here.via;                        /* the slot control came back through */
}

fiber *fiber_launch(struct fiber_stack *st, void (*entry)(fiber *), fiber *org)
{
    assert(org);

    uintptr_t hi = (uintptr_t)st->end & ~(uintptr_t)(alignof(max_align_t) - 1);
    assert(hi > (uintptr_t)st->start);

    if (boot_prepare(st->start, hi - (uintptr_t)st->start) != 0)
        return NULL;

    boot_entry  = entry;
    boot_parent = org;
    return enter_child(org);
}
