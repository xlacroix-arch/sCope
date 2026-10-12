#include <Tools_Class.h>
#include "fiber.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STK 32768

/* 1. specialized slot (fiber NOT the first member) carries parameters ----- */
struct gen { int n; int value; fiber f; };

static void count(fiber *parent)
{
    struct gen *g = v_(struct gen, f, parent);
    for (int i = 0; i < g->n; i++) {
        g->value = i;
        fiber *r = fiber_switch(parent);
        assert(r == parent);                 /* resumed through the same slot */
        (void)r;
    }
}

static void test_generator(void)
{
    static char stk[STK];
    struct fiber_stack st = { stk, stk + STK };
    struct gen g = { .n = 3 };               /* a plain local is a fine slot */
    int sum = 0, steps = 0;

    fiber *r = fiber_launch(&st, count, &g.f);
    assert(r == &g.f && g.f.caller);         /* parked: the slot holds the child */
    (void)r;
    while (g.f.caller) { sum += g.value; steps++; fiber_switch(&g.f); }
    assert(sum == 0 + 1 + 2 && steps == 3);
    assert(g.f.caller == NULL);              /* ended: the slot is empty */

    /* 2. a dead fiber's stack can be reused for a new fiber ------------- */
    g.n = 2; g.value = -1;
    fiber_launch(&st, count, &g.f);
    sum = 0;
    while (g.f.caller) { sum += g.value; fiber_switch(&g.f); }
    assert(sum == 0 + 1);
    puts("generator, v_(), end = empty slot, stack reuse: ok");
}

/* 3. stack isolation: child locals live in the child stack --------------- */
static char *iso_lo, *iso_hi;
static int iso_ok;
static void iso_entry(fiber *p)
{
    char local[64];
    iso_ok = ((char *)local >= iso_lo && (char *)local < iso_hi);
    fiber_switch(p);
}
static void test_isolation(void)
{
    static char stk[STK];
    struct fiber_stack st = { stk, stk + STK };
    fiber slot;                              /* uninitialized on purpose */
    iso_lo = stk; iso_hi = stk + STK;
    fiber *r = fiber_launch(&st, iso_entry, &slot);
    assert(r == &slot && iso_ok);
    (void)r;
    puts("stack isolation: ok");
}

/* 4. nesting: the child owns a local slot for its own child -------------- */
static int trace[16], nt;
static void grandchild(fiber *p)
{
    trace[nt++] = 3;
    fiber_switch(p);          /* back to child */
    trace[nt++] = 5;
    fiber_switch(p);
}
static void child(fiber *p)
{
    static char gstk[STK];
    struct fiber_stack gst = { gstk, gstk + STK };
    fiber gslot;                             /* a local variable of the child */
    trace[nt++] = 1;
    fiber_launch(&gst, grandchild, &gslot);
    trace[nt++] = 4;
    fiber_switch(p);          /* yield to root while grandchild is parked */
    trace[nt++] = 6;
    fiber_switch(&gslot);     /* resume grandchild */
    trace[nt++] = 7;
    fiber_switch(p);
}
static void test_nesting(void)
{
    static char stk[STK];
    struct fiber_stack st = { stk, stk + STK };
    fiber slot;
    fiber_launch(&st, child, &slot);
    trace[nt++] = 2;
    fiber_switch(&slot);
    trace[nt++] = 8;
    int expect[] = {1,3,4,2,6,5,7,8};
    assert(nt == 8);
    for (int i = 0; i < nt; i++) assert(trace[i] == expect[i]);
    (void)expect;
    puts("nested launch with local slots: ok");
}

/* 5. sideways transfer: siblings switch to each other, root parked ------- */
static char order[32]; static int no;
static fiber *sx, *sy;                       /* slots of X and Y */
static void entry_x(fiber *p)
{
    order[no++] = 'X';  fiber_switch(p);     /* X1, yield to root */
    order[no++] = 'x';  fiber_switch(sy);    /* X2, jump sideways into Y */
    order[no++] = 'z';                       /* X3, Y swapped back; finish */
}
static void entry_y(fiber *p)
{
    order[no++] = 'Y';  fiber_switch(p);     /* Y1, yield to root */
    order[no++] = 'y';  fiber_switch(sy);    /* Y2, swap back to X through sy */
    order[no++] = 'w';                       /* Y3, resumed later by root; finish */
}
static void test_sideways(void)
{
    static char xs[STK], ys[STK];
    struct fiber_stack xst = { xs, xs + STK }, yst = { ys, ys + STK };
    fiber x, y;
    sx = &x; sy = &y;
    fiber_launch(&xst, entry_x, &x);         /* X1 */
    fiber_launch(&yst, entry_y, &y);         /* Y1 */
    fiber_switch(&x);                        /* X2, Y2, X3; X ends, exits to root */
    assert(x.caller == NULL && y.caller);    /* x ended; y still holds Y */
    order[no++] = 'R';
    fiber_switch(&y);                        /* Y3; Y ends, exits to root */
    assert(y.caller == NULL);
    order[no++] = 'R';
    order[no] = 0;
    assert(strcmp(order, "XYxyzRwR") == 0);
    puts("sideways transfer between siblings: ok");
}

/* 6. many fibers, round robin from the root ------------------------------ */
#define NF 500
static void worker_entry(fiber *p)
{
    int acc = 0;                             /* locals persist across switches */
    for (int i = 0; i < 100; i++) { acc += i; fiber_switch(p); }
}
static void test_many(void)
{
    struct fiber_stack st[NF];
    fiber slot[NF];
    for (int i = 0; i < NF; i++) {
        char *m = malloc(STK);
        assert(m);
        st[i].start = m; st[i].end = m + STK;
        fiber *r = fiber_launch(&st[i], worker_entry, &slot[i]);
        assert(r == &slot[i]);
        (void)r;
    }
    for (int round = 0; round < 99; round++)
        for (int i = 0; i < NF; i++) fiber_switch(&slot[i]);
    for (int i = 0; i < NF; i++) free(st[i].start);
    puts("500 fibers x 100 rounds: ok");
}

/* 7. specialized slot as a mailbox: who/what/how-many through one slot --- */
struct my_fiber { fiber fiber; int Id, Instance; };

static void mail_x(fiber *p)                 /* Id 10, parent slot p */
{
    struct my_fiber *m = v_(struct my_fiber, fiber, p);
    m->Id = 10; m->Instance++;               /* stamp, then hand over */
    fiber *r = fiber_switch(p);
    struct my_fiber *b = v_(struct my_fiber, fiber, r);
    assert(r == p && b->Id == 1 && b->Instance == 2);   /* root's stamp */
    (void)b;
    m->Id = 10; m->Instance++;
    fiber_switch(p);
}
static void test_mailbox(void)
{
    static char stk[STK];
    struct fiber_stack st = { stk, stk + STK };
    struct my_fiber slot = { .Id = 0, .Instance = 0 };

    fiber *r = fiber_launch(&st, mail_x, &slot.fiber);
    struct my_fiber *m = v_(struct my_fiber, fiber, r);
    assert(m == &slot && m->Id == 10 && m->Instance == 1);   /* child's stamp */
    m->Id = 1; m->Instance++;                                /* root's stamp  */
    r = fiber_switch(&slot.fiber);
    m = v_(struct my_fiber, fiber, r);
    assert(m->Id == 10 && m->Instance == 3);
    (void)m;
    puts("specialized slot as mailbox (Id, Instance): ok");
}

/* 8. sideways with stamps: the receiver learns who sent it --------------- */
static struct my_fiber sl_y;                 /* slot parking Y's context */
static int seen_from_x, seen_from_y;
static void side_x(fiber *p)
{
    fiber_switch(p);                          /* yield to root */
    sl_y.Id = 10;                             /* "from X" */
    fiber *r = fiber_switch(&sl_y.fiber);     /* jump into Y */
    seen_from_y = v_(struct my_fiber, fiber, r)->Id;
}
static void side_y(fiber *p)
{
    fiber_switch(p);
    seen_from_x = sl_y.Id;                    /* read after resume */
    sl_y.Id = 20;                             /* "from Y" */
    fiber_switch(&sl_y.fiber);                /* swap back to X */
}
static void test_stamps(void)
{
    static char xs[STK], ys[STK];
    struct fiber_stack xst = { xs, xs + STK }, yst = { ys, ys + STK };
    fiber xp;
    fiber_launch(&xst, side_x, &xp);
    fiber_launch(&yst, side_y, &sl_y.fiber);
    fiber_switch(&xp);                        /* X -> Y -> X, X ends -> root */
    assert(seen_from_x == 10 && seen_from_y == 20);
    assert(xp.caller == NULL);                /* X ended */
    puts("sideways stamps (receiver learns sender): ok");
}

/* 9. the child owns a specialized LOCAL slot (adopts the parent's context) */
static void count_own(fiber *parent)
{
    struct gen *g = v_(struct gen, f, parent);
    struct gen me;                           /* child state + identity: locals */
    me.n = g->n;
    fiber_move(&me.f, parent);               /* adopt the parked parent context */
    for (int i = 0; i < me.n; i++) {
        me.value = i;
        fiber *r = fiber_switch(&me.f);
        assert(r == &me.f);
        (void)r;
    }
    fiber_move(parent, &me.f);               /* hand it back: entry is returning */
}

static void test_own_slot(void)
{
    static char stk[STK];
    struct fiber_stack st = { stk, stk + STK };
    struct gen g = { .n = 3 };
    int sum = 0, steps = 0;

    fiber *r = fiber_launch(&st, count_own, &g.f);
    assert(r != &g.f);                       /* the child's own slot came back */
    while (r != &g.f) {                      /* &g.f again == the fiber ended  */
        struct gen *c = v_(struct gen, f, r);        /* the child's local `me` */
        assert((char *)c >= stk && (char *)c < stk + STK);
        sum += c->value; steps++;
        r = fiber_switch(r);
    }
    assert(sum == 0 + 1 + 2 && steps == 3);
    assert(g.f.caller == NULL);              /* ended: the parent slot is empty */
    puts("child-local specialized slot (adopt / hand back): ok");
}

/* 10. fiber_move between sibling slots ----------------------------------- */
static int mv_runs;
static void mv_entry(fiber *p)
{
    mv_runs++;
    fiber *r = fiber_switch(p);              /* parked; resumed through the NEW slot */
    mv_runs += 10;
    assert(r != p);                          /* not through the slot we parked in */
    fiber_move(p, r);                        /* general hand-back: parent's context -> p */
}
static void test_move(void)
{
    static char stk[STK];
    struct fiber_stack st = { stk, stk + STK };
    fiber a, b;
    fiber_launch(&st, mv_entry, &a);         /* fiber parked in `a`, runs once */
    assert(mv_runs == 1);
    fiber_move(&b, &a);                      /* now parked in `b`, `a` is empty */
    fiber_move(&b, &b);                      /* self move: no-op */
    fiber *r = fiber_switch(&b);             /* resume through b */
    assert(r == &a && mv_runs == 11);        /* it ended and came back through `a` */
    assert(a.caller == NULL && b.caller == NULL);   /* both slots empty after the end */
    (void)r;
    puts("fiber_move between slots: ok");
}

int main(void)
{
    test_generator();
    test_isolation();
    test_nesting();
    test_sideways();
    test_many();
    test_mailbox();
    test_stamps();
    test_own_slot();
    test_move();
    puts("all ok");
    return 0;
}
