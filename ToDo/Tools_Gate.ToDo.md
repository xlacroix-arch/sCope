Gates and threads: ideas for later

Status: Gates are the edges of the tree of threads (see claude/Tools_Gate.h), TlsMain builds the world around the application (claude/Tools_Main.h). These are the mechanisms that could be added. None is started.

Stance

Threads are process accelerators for a process made of many fibers: the fibers are the program, the threads are where they happen to run faster. This view may change as the framework progresses; the items below are written to hold either way, and the first one is the one that matters most to it.

1. Several processes per thread (structural, do this first)

A thread has one home and one parking fiber, so Knock suspends the whole thread. A small per-thread scheduler (a run queue of processes) would let Knock park only the calling process while the others run, and gives the "wait on several ends" loop (TlsGateWaitAny) a natural place. It touches the home and the parking fiber, so it should come before much else is built on gates. It is also what makes the "threads accelerate a multi-fiber process" view concrete: a process (fiber) could be handed to a thread by a gate, run there, and come back.

2. Posting without waiting

Every request is a rendezvous today. A one-way Post (send, carry on, memory owned by the Shared pool) would cover notifications and work queues, where a merge is overkill. The counterpart of Knock.

3. Timeouts and cancellation

A client parked on a server that never invites is released only by a shutdown. Wait and Knock with a deadline, and a way to withdraw a request still in the inbox. Easy before the request is invited, much harder once it is merged.

4. Deadlock detection

With a strict tree it is cheap to see a parent knocking on a child that is knocking on the parent (both threads suspended). A debug-build assertion on the edge turns a silent hang into an immediate error. Item 1 changes what "suspended" means (a process, not a thread), so check it again then.

5. Hazards of a migrated process

A merged process runs on another thread: anything thread-local computed before the Knock can be stale (met with pthread_self() and the heap head TlsHeap_; errno and the C library per-thread state are the same). Non-inlined accessors for the thread-local accesses, or a debug mode that checks the thread id across a merge, would be safer than the compile-flag note in the header.

6. Outside events in TlsMain
Signals (SIGINT ...) triggering an orderly shutdown of the tree of threads.
A thread that serves a network or timer source needs a wake source other than the futex word (an eventfd that the waker also signals). The blocking operations are behind one small interface (GateSync in Tools_Gate.c), so this should stay contained.
7. Observability

Thread names, a debug dump of the tree and its edges (in Tools_Core_Debug.h), counters (merges, refusals, lock round trips). A buffered pool that hands its memory back when its thread has been idle for a while.

Smaller items
The strict tree (a thread only talks to its parent and its children) is a convention documented in the header, not enforced by the code. Decide whether to enforce it once its limits are understood; siblings go through the parent.
The thread tests use a stand-in malloc pool (claude/MallocPool.h): run them over the real main pool too (TlsMain was tested over it).
The parent end of an edge is served by the thread that called TlsGateSet; if a merged client process spawns a thread, the server thread it runs on becomes the parent. Fine, but to keep in mind with item 1.
