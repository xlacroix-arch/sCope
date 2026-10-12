Tools_MainPool: policies, best usage, pros and cons

Aside file for Tools_MainPool.c. It explains why the pool behaves as it does, and what to do (or avoid) as a user. Numbers are those of the current code; the constants live at the top of the file.

Vocabulary: a bucket is what a TlsMemPool handle designates (one size served). A slab bucket serves blocks cut from chunks (power-of-two sized, aligned on their size). A large bucket serves one block per allocation.

0. Using the pool
Call SizeSelect once per size you use, keep the handle, then Allocate / Free through that handle. This is the fast path.
Free(h, mem, sz): sz is the size you gave to SizeSelect (or the bucket size). Through another handle it still works, with one more dispatch.
A wrong sz is not detected in general (the block is looked up in the chunk of the class of sz). A block freed twice, or a pointer that is not an object boundary, is detected by slab buckets and ends in a critical error.
Blocks are not zeroed, and no more than BucketSize is yours. Their alignment is a property of the bucket (see 3).
A pool is used by one thread at a time.
1. Size classes

The table of classes is built once from the chunk geometry (270 classes up to 4 MiB); SizeSelect is a binary search in it. Above 4 MiB, sizes are rounded to 64 KiB.

Tiny: every size 1..64 is a class. No rounding where it hurts most.
Powers of 2: every one is a class, and stays exact. See 3.
Harmonic: for each chunk size C = 1 KiB .. 4 MiB and each count N of objects in [Nmin(C), 2*Nmin(previous C)), the class is (C-head)/N rounded down to a multiple of its alignment. The class sizes are what fills the chunks, so there is no tail to lose. Nmin is 12 up to 256 KiB chunks, then 11, 10, 8, 8: big objects cannot be 12 per chunk.
Large: above the biggest harmonic class (520 KB), 16 steps per octave, page multiples.

Measured on requests of 65 B..60 KB (log-uniform), counting the rounding to the class and the loss in the chunk: 3.8% on average, 9.9% at worst. The grid it replaced (16 steps per octave, tail lost in the chunk): 5.1% and 12.5%.

Pro: the waste moves from the chunk (1.1% on average) to the class rounding (2.8%): a request is never the unlucky size just above a threshold.
Pro: SizeSelect(h, BucketSize(h)) == h by construction, any class sequence is possible later: the table is the only place that knows the grid.
Pro: from 64 KiB up, every non-power-of-2 size is a page multiple.
Con: BucketSize is not a round number (4064, 4224 ...). A caller that wants all it pays for rounds its size up to a class (BucketSize) and uses all of it, but the popular sizes are exact anyway.
Con: the widest step is 11% (around 260 KB); many classes means many partly used chunks if the sizes of a program are spread out.
2. Slab chunks

A chunk is [objects from the base][padding][bitmap][32 bytes header]. Chunk sizes are every power of 2 from 1 KiB to 4 MiB. The bitmap is one bit per object.

Pro: no per-object header; objects are packed and neighbours share cache lines.
Pro: the chunk of a block is found by masking its address, because a chunk is aligned on its size. No lookup table, no tree. A bitmap makes a double free detectable at no cost.
Pro: with the header at the tail, objects start at the base of the chunk: that is what makes the alignment of 3 possible.
Con: the header page and the first object are not on the same page for chunks above 4 KiB: one more resident page per sparse class.
Con: a chunk must be aligned on its size. With the libc source (aligned_alloc) that wastes address space and, up to 16 KB, the memory is never returned (measured: about 2x RSS). The default source is the arena (9).
Con: mid-size classes (say 20 KB) live in big chunks (256 KB, 12 objects). Untouched pages are not resident, but the address space is.
3. Alignment and powers of 2

Blocks are aligned on the lowest set bit of their size, because object i is at i*size from the aligned base of the chunk.

A power of 2 is aligned on itself (64 -> 64, 4096 -> 4096, 2 MiB -> 2 MiB). Users who choose these sizes for that reason get it.
Harmonic classes are rounded down to a multiple of a, the largest power of 2 not above size/64: so 4 from 256, 8 from 512, 16 from 1024 ..., at a cost below 1/64 of the size. TlsMainPoolAlign(h) gives the alignment of a bucket.
Pro: it costs nothing more than the header slot (below).
Con: a power of 2 loses a slot to the header: 31 objects of 256 in a 8 KiB chunk, 3.1%. The chunk could hold 63 (1.6%) at twice the size: PowObjs. Above 64 KiB (1 MiB chunks, 15 objects, 6.25%) powers of 2 are large buckets: one block each, aligned on its size.
Con: asking for the alignment of a power of 2 with a size just above is not possible: 257 bytes are served as 268, aligned on 4.
4. Fullest chunk first

The chunks of a bucket that are neither full nor empty are binned by the log2 of their number of free objects. Allocate takes from the bin with the fewest free objects, so from the fullest chunk; Free moves a chunk to another bin only when its free count crosses a power of two.

Pro: nearly empty chunks stop receiving allocations, drain, and are released. It is the practical form of defragmentation: raw pointers cannot be moved.
Pro: O(1) both ways (a bitmask of non-empty bins, a find-first-bit).
Con: allocation is not address ordered, so locality across chunks is not looked after. Within a chunk, the lowest free object goes first.
Con: a long-lived object can pin a nearly empty chunk. Nothing can be done about that without moving objects.
5. Spare chunk, small tiers only

When a chunk becomes empty, a bucket whose chunks are 16 KB or less keeps the first one as a spare and gives back any further empty chunk. Allocate uses the spare only when no partly used chunk is left.

Pro (small tiers): a program that allocates and frees around a chunk boundary does not call the chunk source each time: the performance jitter that is hard to track down is avoided where sizes are most common.
Pro: the cost of a spare is bounded: at most 16 KB per class in use.
Con (big tiers): no spare, so the same pattern gets a chunk and gives it back every round trip. The tests show it: 10 rounds of allocate and free on a 20000 byte class took 10 chunks from the source.
Con: a spare is memory the application cannot use elsewhere.
Use: in the big tiers, keep allocation counts stable, or hold a few blocks yourself. Also expect a cheaper ChunkGet/ChunkPut once the arena caches free chunks: then the spare could go, in every tier.
Tunable: SpareMaxChunk.
6. Large buckets

Used for the classes above 520 KB, for the powers of 2 above 64 KiB, for the 0 size (the handle of the root), and above 4 MiB.

Pro: nothing to track, one block per allocation from the chunk source (on the page, or on the size for a power of 2).
Con: no reuse by the bucket; with the arena the blocks up to 4 MiB are cut from its supers and cached by it, above 4 MiB each one is a direct mmap/munmap and goes back at once.
Use: keep big blocks long-lived, grow them by class steps.
7. Accounting

CountSwitch follows the contract of TlsMemPool: the counter gets the bytes handed out (bucket size per block), not the bytes held in chunks or spares.

Pro: the number means what the application did.
Con: it under-reports what the process really holds. ChunkStats in Tools_Core_Debug.h gives the chunk side.
Con: counters are int; a size above 0x7FFF0000 is refused.
8. What is not covered yet
Linux only for now; portability is studied once the system is validated.
The arena releases memory on Put only. Releasing from a watch on the counter (TlsIntCounterWatch) waits for that feature to mature.
No thread safety: the multi-thread pool is a wrapper to be designed with the threading model of the framework.
The class table is global and built on first use (a once guard is enough); Nmin by size, the powers of 2 policy and the large grid are constants at the top of Tools_MainPool.c.
9. The chunk source and the arena

ChunkGet/ChunkPut go through a TlsChunkSource (Tools_Chunk.h): TlsMainPoolOpen uses the arena, TlsMainPoolOpenWith(Count,Src) takes any source (TlsChunkSourceLibc() for comparisons). Both pass the whole test suite.

Memory comes in windows mapped (MAP_NORESERVE) aligned on 4 MiB and cut in 4 MiB supers. Inside, a binary buddy from 1 KiB to 4 MiB with out-of-line bitmaps (about 1.2 KiB per super): chunks are aligned on their size for free.
Placement: lowest super first, smallest sufficient order, lowest address: used memory packs low, so high supers empty out and can go.
Release, on Put only: dirty free blocks of 64 KiB or more beyond max(8 MiB, in use / 8) are madvise(DONTNEED), highest first, down to half of the allowance; a wholly free super is unmapped if another one is free.
Measured: 128 MiB of 64 KiB chunks all freed leaves +4 MiB RSS; with 4 KiB chunks, 63 of 64 freed, RSS stays at 36% with the arena against 99% with libc.
Con: a chunk pinned by one object pins its super (4 MiB of address space, its other pages can still be madvised if free and 64 KiB or more).
Con: free blocks under 64 KiB are never returned until their super is free.

