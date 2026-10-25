# Fondement Michtam: Developer Quick Reference

This concise reference manual outlines the core interfaces, memory rules, fiber semantics, inter-thread synchronization primitives, and application lifecycle for the **Fondement Michtam** framework.

-----

## 1\. Class Idioms & Struct Navigation (`Tools_Class.h`)

Data structures are built by embedding typed components inside outer structures. Navigation relies on offset arithmetic without vtable pointers or manual indirection.

### Offset & Container Retrieval

| Macro         | Signature              | Description                                                                     |
| :------------ | :--------------------- | :------------------------------------------------------------------------------ |
| `s_(t)`       | `s_(Type)`             | Returns `sizeof(Type)` via pointer arithmetic.                                  |
| `o_(t, d)`    | `o_(Type, field)`      | Byte offset of `field` within `Type`.                                           |
| `v_(t, d, p)` | `v_(Type, field, ptr)` | Recovers the outer `Type*` containing embedded `field* ptr`.                    |
| `of_(spec)`   | `of_(Outer.Field)`     | Contextual annotation macro indicating role/connection of embedded lists/nodes. |

### OOP & Method Dispatch

``` c
/* Class Definition */
TlsClassDef(MyClass) {
    int (*DoWork)(MyClass *this, int arg);
};

/* Recover specialized outer struct inside a method */
#define GetMyStruct(this) MyClass *that = v_(MyStruct, ClassField, this)

/* Method Invocation Macro */
x_(obj, DoWork, 42); // Expands to: ((obj)->Static->DoWork)(obj, 42)

```

> **Critical Discipline:** The `x_(v, m, ...)` macro evaluates `v` twice. Never pass function calls or mutating expressions as `v`. Store the instance pointer in a local variable prior to invocation.

-----

## 2\. Memory Model, Stacks & Heaps (`Tools_Core.h`, `Tools_MainPool`)

The memory model combines pointer-bump allocations on local thread heaps with chunked, tiered pool management.

### Allocation & Stack Scopes

  * **Pointer-Bump Allocation:**
      * `rPush(ptr)`: Allocates `sizeof(*ptr)` on the current thread heap (`TlsHeap_.Tgt`).
      * `rnPush(ptr, count)`: Allocates an array of `count` elements of `sizeof(*ptr)`.
  * **Layer Unwinding:**
      * `TlsStackNod *layer = TlsStackLayer(stack);` ¿ Marks a scope checkpoint.
      * `TlsStackLayerPop(stack, layer);` ¿ Reclaims all allocations made since `layer` in $O(1)$ time and triggers attached hooks.
      * `TlsStackHook(stack, fn, arg);` ¿ Registers a cleanup callback invoked when its containing layer is popped.

### Memory Pools (`TlsMemPool`)

`TlsMemPool` handles represent specific bucket size classes:

``` c
TlsMemPool *bucket = x_(pool, SizeSelect, size);   // Obtain handle for 'size'
void *mem          = x_(bucket, Allocate);         // Allocate BucketSize block
x_(bucket, Free, mem, size);                       // Free block back to pool
unsigned int bs    = x_(bucket, BucketSize);       // Actual served size (>= requested)

```

  * **Alignment Rule:**
      * Power-of-two sizes are aligned on themselves up to 4 MiB.
      * Other bucket sizes are aligned on their lowest set bit (multiple of 8, 4, or 2).

-----

## 3\. Fiber Primitives (`fiber.h`)

Fibers represent execution contexts parked in rendezvous slots (`fiber`). Context switches perform 1-to-1 swaps between two slots.

``` c
typedef struct fiber fiber;
struct fiber { fiber *caller; };

```

### Core Operations

  * **`fiber_launch(stack, entry, org)`**:
    Runs `entry(org)` on `stack` (`struct fiber_stack { void *start, *end; }`). The launching context is parked in `org` (received as `parent` by the child).
  * **`fiber_switch(next)`**:
    Swaps the active execution context with the context parked in slot `next`. Returns the slot through which control returned.
  * **`fiber_move(dst, src)`**:
    Transfers the context parked in `src` to `dst`. Leaves `src` empty (`caller == NULL`).
  * **`fiber_exit(parent)`**:
    Terminates the current fiber, resuming the context parked in `parent`. `parent` is left empty.

### Fiber Local Slot Adoption Pattern

``` c
static void child_entry(fiber *parent) {
    struct {
        int result;
        fiber f;
    } local_slot;

    // Adopt parent context into local specialized slot
    fiber_move(&local_slot.f, parent);

    while (working) {
        local_slot.result = compute();
        fiber_switch(&local_slot.f); // Hand control back; parent sees local_slot
    }

    // Hand parent context back before entry returns
    fiber_move(parent, &local_slot.f);
}

```

-----

## 4\. Thread-Tree Synchronization & Gates (`Tools_Gate.h`)

Threads form a strict hierarchy. Communication between adjacent parent-child threads takes place through bidirectional channels called **Gates**.

### Gate Primitives

  * **`Knock(gate, request)`**: Client posts a request to the peer inbox and suspends until served.
  * **`Check(gate)` / `Wait(gate)`**: Server checks (non-blocking) or waits (blocking) for pending requests.
  * **`Invite(gate)`**: Server extracts the oldest pending request from its inbox, returning a `TlsGateGuest*`.
  * **`TlsGateGuestSwitch(target, ret)`**: Swaps execution between server and client across thread boundaries (*Merge*). The client process temporarily executes on the server thread.
  * **`TlsGateGuestSendOff(target)`**: Concludes a request. Resumes the client process on its home thread.
  * **`TlsGateSet(stack_size, entry)`**: Spawns a child thread and returns the parent end of the gate edge.

### Workflow Sequence

``` 
Client Thread                                   Server Thread
-------------                                   -------------
Knock(Up, req) ---------[Inbox Post]----------> Wait(Up) / Invite(Up)
(Suspended)                                     TlsGateGuestSwitch(Guest, ret)
                                                (Client merged onto Server thread)
                                                ... Execute Work ...
                                                TlsGateGuestSendOff(Guest)
Resumed on Client Thread <---------------------- (Server continues)

```

-----

## 5\. Application Entry & Lifecycle (`Tools_Main.h`)

Applications do not define a standard `main()`. Instead, they define `TlsMain` and optional configuration overrides.

### Application Entry Point

``` c
#include <Tools_Main.h>

/* Optional configuration override (weak symbol) */
const TlsMainConfig TlsMainConfigApp = {
    .Source     = 0,            // Default: TlsChunkSourceArena()
    .Stack      = 1024 * 1024,  // Execution stack of main process (1 MiB)
    .HeapGrowth = 64 * 1024,    // Chunk size of root heap (64 KiB)
    .PoolBuffer = 64 * 1024     // Main thread pool buffer capacity (64 KiB)
};

int TlsMain(int argc, char **argv) {
    // Current heap is initialized and ready for rPush / rnPush
    printf("Framework initialized successfully.\n");
    return 0; // Return value is the exit code
}

```

### Automatic Teardown Sequence

1.  `TlsMain` returns exit code to `TlsMainRun`.
2.  Root process heap layer is popped, triggering hooks that shut down all active child threads, newest first.
3.  Memory pools and chunk sources are closed.
4.  Debug builds audit `Tools_MainPool` and output un-freed allocations to `stderr`.

-----

## 6\. Development Guidelines Cheat-Sheet

  * **Macro Evaluation:** Never invoke `x_(func_call(), Method)` directly. Assign `func_call()` to a temporary variable first.
  * **Container Recovery:** Always use `v_(OuterType, FieldName, ptr)` to obtain the parent structure from an embedded node or list.
  * **Fiber Slots:** Never copy `slot.caller` manually; always use `fiber_move(dst, src)`.
  * **Fiber Exit:** Ensure a child fiber moves the parent context back to its original `parent` slot prior to entry function return.
  * **Thread Migration:** When client code runs merged on a server thread between `Knock` and `GuestSendOff`, refresh any thread-local heap references (`TlsHeap_`) rather than caching addresses across calls.
  * **Clean Returns:** Avoid calling `exit()` from `TlsMain` so that automated thread joins, layer unwinding, and memory leak checks execute properly.

