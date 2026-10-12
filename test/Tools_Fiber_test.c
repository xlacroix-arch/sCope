#define _GNU_SOURCE
#include <Tools_Core.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>

extern __thread int TlsFiberLive_;          /* Tools_Stack.c, debug builds */

#define STK 32768

/* 1. generator, relaunch after a layer pop ----------------------------------- */
struct gen { int n; int value; TlsFiber f; };

static void count(TlsFiber *parent) {
    struct gen *g = v_(struct gen,f,parent);
    for (int i=0;i<g->n;i++) {
        g->value = i;
        TlsFiber *r = TlsFiberSwitch(parent);
        assert(r == parent);
        (void)r;
    }
}
static void test_generator(void) {
    struct gen g = { .n = 3 };
    int sum = 0, steps = 0;
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber *r = TlsFiberLaunch(STK,count,&g.f);
    assert(r == &g.f && g.f.SwitchNod && TlsFiberLive_ == 1);
    (void)r;
    while (g.f.SwitchNod) { sum += g.value; steps++; TlsFiberSwitch(&g.f); }
    assert(sum == 0+1+2 && steps == 3 && TlsFiberLive_ == 0);
    assert(TlsHeapLayerPop(L) == 1);                 /* stack, nod and heap are gone */

    L = TlsHeapLayer();                              /* the same space serves a new fiber */
    g.n = 2; g.value = -1;
    TlsFiberLaunch(STK,count,&g.f);
    sum = 0;
    while (g.f.SwitchNod) { sum += g.value; TlsFiberSwitch(&g.f); }
    assert(sum == 0+1 && TlsFiberLive_ == 0);
    TlsHeapLayerPop(L);
    puts("generator, end = empty slot, relaunch after a layer pop: ok");
}

/* 2. a fiber has its own heap; the parent's is untouched ----------------------- */
static TlsHeap *fiber_heap;
static int *fiber_ptr;
static void heap_entry(TlsFiber *p) {
    fiber_heap = TlsHeap_.Tgt;
    for (int i=0;i<3;i++) {
        int *v; rPush(v); *v = 100+i;                 /* in the fiber's heap */
        if (i == 0) fiber_ptr = v;
        TlsFiberSwitch(p);
        assert(TlsHeap_.Tgt == fiber_heap && *fiber_ptr == 100);
    }
}
static void test_heap_isolation(void) {
    TlsHeap *mine = TlsHeap_.Tgt;
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber slot;
    TlsFiberLaunch(STK,heap_entry,&slot);
    assert(fiber_heap != mine && TlsHeap_.Tgt == mine);   /* parent heap is back */
    unsigned char *a; rnPush(a,500); memset(a,0xAA,500);  /* parent allocates between switches */
    for (int i=0;i<3;i++) {
        void *pbefore = TlsHeap_.p;
        TlsFiberSwitch(&slot);
        assert(TlsHeap_.Tgt == mine && TlsHeap_.p == pbefore);
        int *c; rPush(c); *c = i;
    }
    assert(slot.SwitchNod == NULL);
    for (int i=0;i<500;i++) assert(a[i] == 0xAA);
    TlsHeapLayerPop(L);
    puts("each fiber on its own heap, parent heap restored on every switch: ok");
}

/* 3. TlsHeapIn/Out across a switch ------------------------------------------- */
static void inout_entry(TlsFiber *p) {
    TlsHeap *h2 = TlsHeapFork();
    TlsHeapIn(h2)
        int *a; rPush(a); *a = 1;
        TlsFiberSwitch(p);                           /* switch inside the block */
        assert(TlsHeap_.Tgt == h2 && *a == 1);       /* still on h2 afterwards */
        int *b; rPush(b); *b = 2;
        assert(*a == 1 && *b == 2 && (void *)b > (void *)a);
    TlsHeapOut
}
static void test_in_out(void) {
    TlsHeap *mine = TlsHeap_.Tgt;
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber slot;
    TlsFiberLaunch(STK,inout_entry,&slot);
    assert(TlsHeap_.Tgt == mine);                    /* the parent is not on h2 */
    TlsFiberSwitch(&slot);
    assert(slot.SwitchNod == NULL && TlsHeap_.Tgt == mine);
    TlsHeapLayerPop(L);
    puts("switch inside TlsHeapIn/Out keeps each side on its heap: ok");
}

/* 3b. another fiber allocates in a parked fiber's heap ------------------------- */
static TlsHeap *xh; static int *xa, *xz;
static void xalloc_entry(TlsFiber *p) {
    xh = TlsHeap_.Tgt;
    int *a; rPush(a); *a = 1; xa = a;
    TlsFiberSwitch(p);                               /* meanwhile the parent allocates here */
    int *b; rPush(b); *b = 3;
    assert(b != xz && b != xa && *xa == 1 && *xz == 2);   /* no overlap */
}
static void test_parked_heap(void) {
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber slot;
    TlsFiberLaunch(STK,xalloc_entry,&slot);
    TlsHeapIn(xh)
        rPush(xz); *xz = 2;
    TlsHeapOut
    assert(xz != xa);
    TlsFiberSwitch(&slot);
    assert(slot.SwitchNod == NULL && *xa == 1 && *xz == 2);
    TlsHeapLayerPop(L);
    puts("a parked fiber's heap can be allocated in by another fiber: ok");
}

/* 4. nesting: a child launches its own child ----------------------------------- */
static int trace[16], nt;
static void grandchild(TlsFiber *p) {
    trace[nt++] = 3;
    TlsFiberSwitch(p);
    trace[nt++] = 5;
    TlsFiberSwitch(p);
}
static void child(TlsFiber *p) {
    TlsFiber gslot;
    trace[nt++] = 1;
    TlsFiberLaunch(STK,grandchild,&gslot);
    trace[nt++] = 4;
    TlsFiberSwitch(p);
    trace[nt++] = 6;
    TlsFiberSwitch(&gslot);
    trace[nt++] = 7;
    TlsFiberSwitch(p);
    TlsFiberSwitch(&gslot);                          /* let the grandchild end */
    assert(gslot.SwitchNod == NULL);
}
static void test_nesting(void) {
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber slot;
    TlsFiberLaunch(STK,child,&slot);
    trace[nt++] = 2;
    TlsFiberSwitch(&slot);
    trace[nt++] = 8;
    TlsFiberSwitch(&slot);                           /* the child finishes */
    assert(slot.SwitchNod == NULL && TlsFiberLive_ == 0);
    int expect[] = {1,3,4,2,6,5,7,8};
    assert(nt == 8);
    for (int i=0;i<nt;i++) assert(trace[i] == expect[i]);
    (void)expect;
    TlsHeapLayerPop(L);
    puts("nested launch (heap of heap, stack in heap): ok");
}

/* 5. sideways transfer between siblings ---------------------------------------- */
static char order[32]; static int no;
static TlsFiber *sy;
static void entry_x(TlsFiber *p) {
    order[no++] = 'X';  TlsFiberSwitch(p);
    order[no++] = 'x';  TlsFiberSwitch(sy);
    order[no++] = 'z';
}
static void entry_y(TlsFiber *p) {
    order[no++] = 'Y';  TlsFiberSwitch(p);
    order[no++] = 'y';  TlsFiberSwitch(sy);
    order[no++] = 'w';
}
static void test_sideways(void) {
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber x, y;
    sy = &y;
    TlsFiberLaunch(STK,entry_x,&x);
    TlsFiberLaunch(STK,entry_y,&y);
    TlsFiberSwitch(&x);
    assert(x.SwitchNod == NULL && y.SwitchNod);
    order[no++] = 'R';
    TlsFiberSwitch(&y);
    assert(y.SwitchNod == NULL);
    order[no++] = 'R';
    order[no] = 0;
    assert(strcmp(order,"XYxyzRwR") == 0 && TlsFiberLive_ == 0);
    TlsHeapLayerPop(L);
    puts("sideways transfer between siblings: ok");
}

/* 6. many fibers, each allocating in its own heap ------------------------------ */
#define NF 300
static void worker(TlsFiber *p) {
    int acc = 0;
    int *mine; rPush(mine); *mine = 0;
    for (int i=0;i<50;i++) {
        acc += i; (*mine)++;
        unsigned char *junk; rnPush(junk,64+i); memset(junk,i,64+i);
        TlsFiberSwitch(p);
    }
    assert(*mine == 50 && acc == 49*50/2);
}
static void test_many(void) {
    static TlsFiber slot[NF];
    TlsStackNod *L = TlsHeapLayer();
    for (int i=0;i<NF;i++) {
        TlsFiber *r = TlsFiberLaunch(STK,worker,&slot[i]);
        assert(r == &slot[i]);
        (void)r;
    }
    assert(TlsFiberLive_ == NF);
    for (int round=0;round<49;round++)
        for (int i=0;i<NF;i++) TlsFiberSwitch(&slot[i]);
    for (int i=0;i<NF;i++) { TlsFiberSwitch(&slot[i]); assert(slot[i].SwitchNod == NULL); }
    assert(TlsFiberLive_ == 0);
    TlsHeapLayerPop(L);
    puts("300 fibers x 50 rounds, one heap each: ok");
}

/* 7. mailbox: a specialized slot carries stamps -------------------------------- */
struct my_fiber { TlsFiber fiber; int Id, Instance; };
static void mail_x(TlsFiber *p) {
    struct my_fiber *m = v_(struct my_fiber,fiber,p);
    m->Id = 10; m->Instance++;
    TlsFiber *r = TlsFiberSwitch(p);
    struct my_fiber *b = v_(struct my_fiber,fiber,r);
    assert(r == p && b->Id == 1 && b->Instance == 2);
    (void)b;
    m->Id = 10; m->Instance++;
    TlsFiberSwitch(p);
}
static void test_mailbox(void) {
    TlsStackNod *L = TlsHeapLayer();
    struct my_fiber slot = { .Id = 0, .Instance = 0 };
    TlsFiber *r = TlsFiberLaunch(STK,mail_x,&slot.fiber);
    struct my_fiber *m = v_(struct my_fiber,fiber,r);
    assert(m == &slot && m->Id == 10 && m->Instance == 1);
    m->Id = 1; m->Instance++;
    r = TlsFiberSwitch(&slot.fiber);
    m = v_(struct my_fiber,fiber,r);
    assert(m->Id == 10 && m->Instance == 3);
    (void)m;
    TlsFiberSwitch(&slot.fiber);                     /* let it end */
    assert(slot.fiber.SwitchNod == NULL);
    TlsHeapLayerPop(L);
    puts("specialized slot as mailbox: ok");
}

/* 8. the child adopts the parent's nod in a local slot -------------------------- */
static void count_own(TlsFiber *parent) {
    struct gen *g = v_(struct gen,f,parent);
    struct gen me;
    me.n = g->n;
    TlsFiberNodMove(&me.f,parent);
    for (int i=0;i<me.n;i++) {
        me.value = i;
        TlsFiber *r = TlsFiberSwitch(&me.f);
        assert(r == &me.f);
        (void)r;
    }
    TlsFiberNodMove(parent,&me.f);                   /* hand it back before returning */
}
static void test_own_slot(void) {
    TlsStackNod *L = TlsHeapLayer();
    struct gen g = { .n = 3 };
    int sum = 0, steps = 0;
    TlsFiber *r = TlsFiberLaunch(STK,count_own,&g.f);
    assert(r != &g.f);
    while (r != &g.f) {
        struct gen *c = v_(struct gen,f,r);
        sum += c->value; steps++;
        r = TlsFiberSwitch(r);
    }
    assert(sum == 0+1+2 && steps == 3 && g.f.SwitchNod == NULL);
    TlsHeapLayerPop(L);
    puts("child-local slot (NodMove / hand back): ok");
}

/* 9. TlsFiberNodMove between sibling slots -------------------------------------- */
static int mv_runs;
static void mv_entry(TlsFiber *p) {
    mv_runs++;
    TlsFiber *r = TlsFiberSwitch(p);
    mv_runs += 10;
    assert(r != p);
    TlsFiberNodMove(p,r);
}
static void test_move(void) {
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber a, b;
    TlsFiberLaunch(STK,mv_entry,&a);
    assert(mv_runs == 1);
    TlsFiberNodMove(&b,&a);
    TlsFiberNodMove(&b,&b);
    TlsFiber *r = TlsFiberSwitch(&b);
    assert(r == &a && mv_runs == 11);
    assert(a.SwitchNod == NULL && b.SwitchNod == NULL);
    (void)r;
    TlsHeapLayerPop(L);
    puts("TlsFiberNodMove between slots: ok");
}

/* 10. a fiber that never returns, abandoned by its parent ------------------------- */
static void zombie(TlsFiber *p) { for (;;) TlsFiberSwitch(p); }
static void test_abandon(void) {
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber z;
    TlsFiberLaunch(STK,zombie,&z);
    for (int i=0;i<3;i++) { TlsFiberSwitch(&z); assert(z.SwitchNod); }
    assert(TlsFiberLive_ == 1);
    TlsFiberAbandon(&z);
    assert(z.SwitchNod == NULL && TlsFiberLive_ == 0);
    assert(TlsHeapLayerPop(L) == 1);
    puts("zombie by choice, abandoned by the parent: ok");
}

/* 11. popping a layer with a live fiber is caught --------------------------------- */
static void test_live_check(void) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        TlsFiber z;
        TlsStackNod *L = TlsHeapLayer();
        TlsFiberLaunch(STK,zombie,&z);
        freopen("/dev/null","w",stderr);
        TlsHeapLayerPop(L);                          /* z is still parked: must abort */
        _exit(0);
    }
    int st; waitpid(pid,&st,0);
    assert(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
    puts("layer pop with a live fiber trips the debug check: ok");
}

/* 12. an entry that returns without ever switching -------------------------------- */
static void quick(TlsFiber *p) { (void)p; }
static void test_end_without_switch(void) {
    TlsStackNod *L = TlsHeapLayer();
    TlsFiber s;
    TlsFiber *r = TlsFiberLaunch(STK,quick,&s);
    assert(r == &s && s.SwitchNod == NULL && TlsFiberLive_ == 0);
    (void)r;
    TlsHeapLayerPop(L);
    puts("entry returning without ever switching: ok");
}

int main(void) {
    TlsMainHeapOpen(4096);
    test_generator();
    test_heap_isolation();
    test_in_out();
    test_parked_heap();
    test_nesting();
    test_sideways();
    test_many();
    test_mailbox();
    test_own_slot();
    test_move();
    test_abandon();
    test_live_check();
    test_end_without_switch();
    assert(TlsFiberLive_ == 0);
    TlsMainHeapClose();
    puts("all ok");
    return 0;
}
