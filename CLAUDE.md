# Fondement Michtam: Tools framework

C framework (`Tools_*`). The rationale of each component lives in the header
comments and in aside files such as `claude/Tools_MainPool.Policies.md`. This
file only says what to do.

## Programming style: typed components

Data structures are built by embedding typed components, never by hand-rolling
links or vtable pointers.

- Embed components (TlsList, TlsDList, TlsBNod, TlsMemPool, TlsHeap, fiber, ...)
  as fields. Do not write your own next/prev pointers where a component exists.
- Class components (TlsMemPool, TlsSignal, TlsHeap, ...): name the field after
  the type, `TlsMemPool TlsMemPool;`. Then `TlsGetThat(Outer,TlsMemPool)` reads
  naturally and is greppable.
- List and node components (TlsList, TlsDList, TlsBNod): name the field after its
  role and mark it with `of_(Type.Field)`, naming the field the list is made of.
  Node: `TlsDList of_(Slab.List) List;`. Head: `TlsDList of_(Slab.List) Full;`.
- A TlsDList head is a sentinel, initialised circular (`h->n = h->p = h`).
  Empty means `h->n == h`. Iterate from `h->n` until you are back on `h`.
- Recover the outer struct with `v_(Outer,field,ptr)`, or `TlsGetThat` as the
  first line of a method. Never cast, and never assume the component is the
  first member: components are often embedded at an offset.
- A runtime array index inside `v_()`/`o_()`, as in
  `v_(AlignPool,MainBuck[cSz-8],that)`, is accepted: it is only offset arithmetic
  on a null base, and GCC compiles it cleanly. Keep it while the build stays
  warning-free; if a compiler starts to complain, compute the base explicitly.
- `x_(v,m,...)` evaluates `v` twice: the receiver must be a plain variable or
  field path, never a call. Store call results first.
- A new component states its contract in the header before the implementation;
  tests exercise that contract through the public interface only.

## Dialect and platform

- GNU C: void* arithmetic, `__VA_OPT__`, `__thread`. Linux only for now.
- Build with `-U_FORTIFY_SOURCE`: fibers jump across stacks.

## Do not "fix"

- `TlsBNod` is deliberately re-declared per translation unit (opaque, at least
  16 bytes). Do not share one definition.
