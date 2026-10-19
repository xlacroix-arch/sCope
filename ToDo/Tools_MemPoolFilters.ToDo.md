MemPool filters: ideas for later

Status: TlsMemPoolShared and TlsMemPoolBuffered work (claude/Tools_MemPoolFilters.c), and the Gate uses them. These are strategies for the buffer, and for the filters around it. None is started; the first section of each item says what we do today.

1. Sizing the buffer

Today: a fixed budget per buffer (GateBuffer, 64 KiB, in Tools_Gate.c).

Adaptive budget: grow when refills and flushes come at a high rate, shrink when the working set is smaller than the budget.
Trim when idle: a thread that stops working keeps its blocks, which pin chunks of the Shared pool. Give them back after a quiet period (same family as item 7 of Tools_Gate.ToDo.md).
A budget per role (a worker thread, a thread that mostly serves gates).
2. Batch and flush policy

Today: a batch is a quarter of the budget (2 to 128 blocks), a flush goes down to half, the bucket being freed first, then the others from the newest bucket.

Adaptive batch per bucket from its recent demand (small after a quiet time, large when a bucket is hammered), like the thread caches of other allocators.
What a flush gives back first: the largest blocks, the coldest bucket (LRU), round robin. Flush only the excess instead of going down to half.
Keep the quarter/half hysteresis whatever we choose: it is what stops the storm at a boundary (there is a test for it).
3. Fairness between buckets

One hot bucket can take the whole budget and starve the others into refills. A cap per bucket (half of the budget, say), or a small reserve per bucket.

4. Blocks that change threads

With Gates a process migrates, and frees what another thread's buffer allocated. Today the block simply lands in the buffer of the thread that frees it, which is bounded and correct. A producer/consumer pair would make one buffer fill with what it only frees, and flush it all the time. Measure before changing anything; a possible answer is to flush more of the buckets that are only ever freed into.

5. The size cache

Today: 64 entries, direct mapped, by size.

2 ways, or a cache by range: if a bucket knew its lowest size, any size of the range would hit without an entry. That needs the contract to tell the range of a bucket (the main pool has it: Lo).
It relies on SizeSelect giving the same answer from any handle, forever. Revisit if a pool ever merges or splits buckets.
6. Tiny blocks

The link of a free list needs 8 bytes, so the buffer serves at least 8 bytes. For 1..7 byte data, sit the buffer over a Mini pool (Tools_MemPoolMini.c) or keep a side stack of pointers.

7. Locality

LIFO (today: the hottest block comes first), FIFO, or by address to keep a page together; NUMA, if it ever matters.

8. The Shared pool

One lock for the whole pool today. Spin count tuning; striping by group of buckets, or a lock per bucket, which needs a child that does not share state between its buckets (the main pool shares its class table and its chunk source, so it would need its own lock-free parts first). The raw futex calls are duplicated in Tools_Gate.c: one small Tools_Futex for both.

9. Statistics, to choose from data

Hit rate, refills, flushes, high water mark of a buffer, in Tools_Core_Debug.h. Budgets and batch sizes should come from these, not from guesses.

10. Memory pressure

When the Shared pool is close to a limit (TlsIntCounterWatch), ask the buffers to give their blocks back. Needs a registry of buffers and a way to ask a thread to flush its own (a buffer is not thread safe: the owner must do it).

11. The Close protocol

"Close the buffer before its child" is left to the caller. A registry (item 10) could check it.

Mini pool (claude/Tools_MemPoolMini.c)
Pages are taken from the Partial list most recently touched first. The main pool bins its chunks by free count and allocates from the fullest, so the others can drain and be released: same idea here.
One spare page per bucket for pages up to 16 KiB, like the main pool; a bigger parent policy (a spare shared by the whole pool) is possible.
The class table is a cache-less arithmetic grid (exact up to 32, then 16 steps per octave): a table tuned to the application's sizes could waste less.
Not thread safe: one Mini per thread, over a Shared or a per-thread parent.
Xavier's ideas

(to fill)
