/* fiber.h - minimal fibers / multi-stack programming in C.
 *
 * Four primitives (launch, switch, move, exit), one public structure.
 *
 * A `fiber` is a RENDEZVOUS SLOT: a place where a suspended execution
 * context is parked. Its `caller` member points to an implementation-
 * dependent structure, set by fiber_launch() and fiber_switch(); treat it
 * as opaque. A slot is typically a local variable (or a member of a
 * specialized struct living in a local variable) of the fiber that owns it.
 *
 * Specialization: embed `fiber` anywhere in your struct and recover your
 * struct with v_():   struct gen { int n; fiber f; };
 *                     struct gen *g = v_(struct gen, f, parent);
 *
 * fiber_switch(slot) is a SWAP: the running context is parked in `slot`
 * and the context that was parked there runs. Whoever later switches to
 * the same slot swaps back. So a parent and a child ping-pong through one
 * shared slot (the `org` of the launch, which the child gets as `parent`);
 * a parent with several children keeps one slot per child; control may
 * also go sideways (switch to a slot holding a sibling's context).
 *
 * MOVING a slot. A slot only designates a parked context, and
 * fiber_move(dst, src) transfers that context from one slot to another;
 * `src` is left empty. This is how a fiber gets its own specialized local
 * slot while using the channel it was launched with:
 *
 *     static void child(fiber *parent)
 *     {
 *         struct gen me;                    // child-local, specialized slot
 *         fiber_move(&me.f, parent);        // adopt the parked parent context
 *         ... fiber_switch(&me.f) ...       // parent now sees `me` as the slot
 *         fiber_move(parent, &me.f);        // hand it back before entry returns
 *     }
 *
 * The slot through which control comes back is what fiber_switch() and
 * fiber_launch() return, so the parent learns about `me` (and can read the
 * child's locals through it) without any other channel.
 *
 * ENDING. When an entry returns, the fiber ends: the context parked in
 * `parent` runs (fiber_exit), `parent` is left empty, and the fiber is
 * gone. The parent can tell: the slot it gets back is empty (`caller` is
 * NULL). Resuming an ended fiber is an error (empty slot), not a no-op.
 * A fiber that wants zombie semantics simply never returns from its entry
 * and loops on fiber_switch(parent) itself.
 *
 * Discipline (required at project level, only partly checked by asserts):
 *   - only switch to a slot that currently holds a parked context (a slot
 *     emptied by fiber_move() or by the end of a fiber is caught by an
 *     assert);
 *   - a slot must stay alive while it holds a context (for a slot that is
 *     a local of some fiber, its stack too, and its entry must not return
 *     while it holds one: hand the context back first, see above, since
 *     the exit goes through `parent`);
 *   - never copy a slot's `caller` by hand: use fiber_move().
 *
 * There is no global "current fiber": fibers are anonymous, only slots
 * exist. Apart from a short-lived bootstrap handoff, no state is shared.
 */
#ifndef FIBER_H
#define FIBER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fiber fiber;
struct fiber { fiber *caller; };

struct fiber_stack { void *start, *end; };

/* Run entry(org) immediately on the given stack. `org` is the slot in
 * which the launching context is parked while the child runs; the child
 * receives it as `parent` and switches to it to hand control back.
 * `org` need not be initialized.
 *
 * The whole range [start,end) is usable. Child state is meant to live in
 * its locals.
 *
 * Returns the slot control first comes back through (same as
 * fiber_switch(): `org`, or the child's own slot if it moved the parent's
 * context into one), or NULL if the stack could not be bootstrapped
 * (nothing was run).
 *
 * When entry returns, the fiber ends through fiber_exit(parent): control
 * comes back through `org` (or whatever slot holds the parent's context),
 * which is left empty. If entry returns without ever switching, that is
 * what fiber_launch itself returns.
 *
 * Stack size: a few KiB plus a signal frame for the sigaltstack backend
 * (see fiber.c); 16 KiB is a sane floor, more for real work. */
fiber *fiber_launch(struct fiber_stack *stack,
                    void (*entry)(fiber *parent),
                    fiber *org);

/* Swap the running context with the one parked in `next`.
 * Returns, when the parked context is resumed, the slot the resumer
 * switched through. Usually that is `next` again; it differs when the slot
 * was moved meanwhile (see above). The slot is also a mailbox: whatever the
 * resumer stored in the specialized struct around it before switching can
 * be read through v_() on the returned slot (and vice versa). */
fiber *fiber_switch(fiber *next);

/* Move the parked context of slot `src` into slot `dst` (whose previous
 * content, possibly uninitialized, is discarded); `src` becomes empty and
 * must not be switched to until it holds a context again. src == dst is a
 * no-op. */
void fiber_move(fiber *dst, fiber *src);

/* End the running fiber: the context parked in `parent` runs, and `parent`
 * is left empty (the resumed context gets `parent` as the slot it came
 * back through). Nothing is parked, so this never returns. It is what
 * happens when an entry returns; calling it directly ends the fiber early
 * (its frames are not unwound). `parent` must hold the parent's context. */
#ifdef __cplusplus
[[noreturn]]
#else
_Noreturn
#endif
void fiber_exit(fiber *parent);

#ifdef __cplusplus
}
#endif
#endif /* FIBER_H */
