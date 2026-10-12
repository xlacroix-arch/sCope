/*  Fondement Michtam
 *  Copyright (C) 2026 <AI> Claude
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
 
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <Tools_Core.h>
#include <Tools_Chunk.h>
#include <Tools_Core_Debug.h>
 
/*_____________________________________________________________________________
 |
 | Chunk sources: the libc one, and the arena.
 |
 | THE ARENA (Linux). Memory comes from windows mapped with mmap, aligned on
 | 4 MiB and cut in supers of 4 MiB. Each super is a binary buddy allocator
 | over blocks of 2^10 (1 KiB) to 2^22 (4 MiB) bytes: a block of order k is
 | aligned on 2^k, so every chunk of a pool is aligned for free (no 2x address
 | space as with aligned_alloc), and free neighbours merge.
 |
 |  - The metadata is out of line (about 1.2 KiB per super: a bitmap of free
 |    blocks per order, a bitmap of pages that may be resident). Free memory is
 |    never touched by the arena, so what it gives back stays given back.
 |  - Placement: Get takes the lowest super that has a block big enough, and in
 |    it the smallest sufficient order, at the lowest address. Allocation is
 |    pushed to the low end, so the high end drains and can be released.
 |  - Sizes that are not a power of 2 (large buckets) take a block of the order
 |    above and put the tail back as free blocks: the arena is exact to 1 KiB.
 |  - Above 4 MiB, blocks are mapped and unmapped one by one.
 |  - Release, decided on Put only: the free blocks of 64 KiB or more that may
 |    be resident (dirty) are counted. When they exceed what the arena may
 |    cache, max(CacheMin, in use / CacheDiv), the highest ones are given back
 |    with madvise(DONTNEED) down to half of it (hysteresis: not at each Put).
 |    Smaller free blocks are never given back alone: they are only worth it
 |    once merged. A whole super that is free is unmapped if another one is
 |    free too, so at most one is kept as a cache.
 |  - Known limits: finding a super is a scan of the slots (from the lowest
 |    one that has room); fine for gigabytes, a hint structure would do for
 |    hundreds. Windows are mapped with MAP_NORESERVE; with strict
 |    overcommit (mode 2) the flag is ignored and a window counts as used.
 |_____________________________________________________________________________
*/
 
static void Crit(char *Msg) {
    x_(&TlsCritErrorShutdown,Signal,Msg);
    abort();
}
 
/*_____________________________________________________________________________
 |
 | The libc source
 |_____________________________________________________________________________
*/
 
static void *LibcGet(TlsChunkSource *this,unsigned int sz,unsigned int align) {
    void *r;
    (void)this;
    r = aligned_alloc(align,sz);
    if (!r) Crit("Out of Memory");
    return r;
}
static void LibcPut(TlsChunkSource *this,void *mem,unsigned int sz) {
    (void)this; (void)sz;
    free(mem);
}
static void LibcClose(TlsChunkSource *this) { (void)this; }
static struct TlsChunkSource LibcStatic = {LibcGet,LibcPut,LibcClose};
static TlsChunkSource Libc = {&LibcStatic};
TlsChunkSource *TlsChunkSourceLibc(void) { return &Libc; }
 
/*_____________________________________________________________________________
 |
 | Constants and structures
 |_____________________________________________________________________________
*/
 
#define SupOrder 22
#define SupSize (1u<<SupOrder)
#define MinOrder 10
#define NOrd (SupOrder-MinOrder+1)
#define PageShift 12
#define PageSize (1u<<PageShift)
#define ReleaseMinOrder 16              /* only free blocks of 64 KiB or more are given back */
#define CacheMin (8u<<20)               /* free memory kept even if little is in use */
#define CacheDiv 8                      /* and 1/8 of what is in use */
#define WinFirst 4                      /* supers of the first window ... */
#define WinMax 256                      /* ... doubling up to this */
#define NoSlot 0xFFFFFFFFu
 
/* words of the free bitmap of order k=10..22: 2^(22-k) bits */
static const unsigned short WOff[NOrd+1] = {0,64,96,112,120,124,126,127,128,129,130,131,132,133};
#define NWords 133
 
typedef struct {
    char *base;                         /* NULL: slot unused */
    uint32_t mask;                      /* bit (k-MinOrder) set if cnt[k] is not 0 */
    uint16_t cnt[NOrd];                 /* free blocks of each order */
    uint64_t bits[NWords];              /* free blocks, by order: bit i is the block at i<<k */
    uint64_t dirty[16];                 /* pages that may be resident: 1024 bits */
} Super;
 
typedef struct {
    char *base;
    unsigned int n,used;                /* supers in the window, and taken so far */
    unsigned int *slot;                 /* slot of each super, NoSlot once unmapped */
} Window;
 
typedef struct Arena {
    TlsChunkSource TlsChunkSource;
    Super *S;
    unsigned int nS,capS;
    unsigned int low;                   /* no slot below has a free block */
    Window *W;
    unsigned int nW,capW,nextWin;
    uint64_t InUse,SupUsed,DirtyBig;    /* bytes; bytes from supers; dirty pages of free blocks >= 64 KiB */
    unsigned int Mapped,FreeSupers;
    unsigned long Madvise,Mmap,Munmap;
} Arena;
 
/*_____________________________________________________________________________
 |
 | Bitmaps
 |_____________________________________________________________________________
*/
 
static unsigned int Words(unsigned int k) { return WOff[k-MinOrder+1]-WOff[k-MinOrder]; }
static uint64_t *Bits(Super *s,unsigned int k) { return s->bits+WOff[k-MinOrder]; }
static int TestBit(Super *s,unsigned int k,uint32_t i) { return (Bits(s,k)[i>>6]>>(i&63))&1; }
static unsigned int PopRange(const uint64_t *d,unsigned int first,unsigned int n) {
    unsigned int c = 0;
    while (n) {
        unsigned int b = first&63,t = 64-b;
        uint64_t m;
        if (t>n) t = n;
        m = (t==64) ? ~0ull : (((1ull<<t)-1)<<b);
        c += (unsigned int)__builtin_popcountll(d[first>>6]&m);
        first += t; n -= t;
    }
    return c;
}
static void SetRange(uint64_t *d,unsigned int first,unsigned int n,int val) {
    while (n) {
        unsigned int b = first&63,t = 64-b;
        uint64_t m;
        if (t>n) t = n;
        m = (t==64) ? ~0ull : (((1ull<<t)-1)<<b);
        if (val) d[first>>6] |= m; else d[first>>6] &= ~m;
        first += t; n -= t;
    }
}
/* dirty pages of the free block (k,i) */
static unsigned int DirtyCount(Super *s,unsigned int k,uint32_t i) {
    return PopRange(s->dirty,i<<(k-PageShift),1u<<(k-PageShift));
}
 
/* Free blocks come and go thru these two: they keep the counts, the mask, the
 * hint of the lowest slot and the number of dirty pages of the big free blocks. */
static void BlockSet(Arena *A,unsigned int si,unsigned int k,uint32_t i) {
    Super *s = &A->S[si];
    Bits(s,k)[i>>6] |= 1ull<<(i&63);
    s->cnt[k-MinOrder]++;
    s->mask |= 1u<<(k-MinOrder);
    if (k==SupOrder) A->FreeSupers++;
    if (k>=ReleaseMinOrder) A->DirtyBig += DirtyCount(s,k,i);
    if (si<A->low) A->low = si;
}
static void BlockClear(Arena *A,unsigned int si,unsigned int k,uint32_t i) {
    Super *s = &A->S[si];
    Bits(s,k)[i>>6] &= ~(1ull<<(i&63));
    if (--s->cnt[k-MinOrder]==0) s->mask &= ~(1u<<(k-MinOrder));
    if (k==SupOrder) A->FreeSupers--;
    if (k>=ReleaseMinOrder) A->DirtyBig -= DirtyCount(s,k,i);
}
static uint32_t LowestBlock(Super *s,unsigned int k) {
    uint64_t *b = Bits(s,k);
    unsigned int w = 0;
    while (!b[w]) w++;
    return (w<<6)+(uint32_t)__builtin_ctzll(b[w]);
}
static void MarkDirty(Super *s,uint32_t off,uint32_t len) {
    unsigned int f = off>>PageShift,l = (off+len-1)>>PageShift;
    SetRange(s->dirty,f,l-f+1,1);
}
 
/*_____________________________________________________________________________
 |
 | Windows and supers
 |_____________________________________________________________________________
*/
 
static void *MapAligned(Arena *A,size_t len,int noreserve) {
    char *m = mmap(0,len+SupSize,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|(noreserve?MAP_NORESERVE:0),-1,0),*b;
    size_t head,tail;
    if (m==MAP_FAILED) return 0;
    A->Mmap++;
    b = (char *)(((uintptr_t)m+SupSize-1)&~(uintptr_t)(SupSize-1));
    head = (size_t)(b-m);
    tail = len+SupSize-head-len;
    if (head) { munmap(m,head); A->Munmap++; }
    if (tail) { munmap(b+len,tail); A->Munmap++; }
    return b;
}
static void WindowNew(Arena *A) {
    unsigned int n = A->nextWin;
    char *b = 0;
    Window *w;
    while (n>=1 && !(b = MapAligned(A,(size_t)n*SupSize,1))) n >>= 1;
    if (!b) Crit("Out of Memory");
    if (A->nextWin<WinMax) A->nextWin <<= 1;
    if (A->nW==A->capW) {
        A->capW = A->capW ? 2*A->capW : 8;
        A->W = realloc(A->W,A->capW*sizeof(Window));
        if (!A->W) Crit("Out of Memory");
    }
    w = &A->W[A->nW++];
    w->base = b; w->n = n; w->used = 0;
    w->slot = malloc(n*sizeof(unsigned int));
    if (!w->slot) Crit("Out of Memory");
    memset(w->slot,0xFF,n*sizeof(unsigned int));
}
/* A new super, all free; returns its slot. */
static unsigned int SuperNew(Arena *A) {
    unsigned int si,wi;
    Window *w = 0;
    Super *s;
    for (wi=0;wi<A->nW;wi++) if (A->W[wi].used<A->W[wi].n) { w = &A->W[wi]; break; }
    if (!w) { WindowNew(A); w = &A->W[A->nW-1]; }
    for (si=0;si<A->nS && A->S[si].base;si++) ;
    if (si==A->nS) {
        if (A->nS==A->capS) {
            A->capS = A->capS ? 2*A->capS : 16;
            A->S = realloc(A->S,A->capS*sizeof(Super));
            if (!A->S) Crit("Out of Memory");
        }
        A->nS++;
    }
    s = &A->S[si];
    memset(s,0,sizeof(*s));
    s->base = w->base+(size_t)w->used*SupSize;
    w->slot[w->used++] = si;
    A->Mapped++;
    BlockSet(A,si,SupOrder,0);
    return si;
}
/* A window keeps its whole original extent, but a super released from it is
 * unmapped, and the hole can be reused by the next mmap, for another window. So
 * several windows can claim an address: only the one whose slot there is live
 * holds the super. */
static unsigned int SuperOf(Arena *A,void *p) {
    unsigned int i;
    int inside = 0;
    for (i=0;i<A->nW;i++) {
        Window *w = &A->W[i];
        if ((char *)p>=w->base && (char *)p<w->base+(size_t)w->n*SupSize) {
            unsigned int si = w->slot[((char *)p-w->base)>>SupOrder];
            if (si!=NoSlot) return si;
            inside = 1;
        }
    }
    Crit(inside ? "Chunk Put: block in a released region." : "Chunk Put: not a block of this source.");
    return 0;
}
/* The window that holds the live super in slot si. */
static Window *WindowOfSuper(Arena *A,unsigned int si) {
    char *b = A->S[si].base;
    unsigned int i;
    for (i=0;i<A->nW;i++) {
        Window *w = &A->W[i];
        if (b>=w->base && b<w->base+(size_t)w->n*SupSize && w->slot[(b-w->base)>>SupOrder]==si) return w;
    }
    Crit("Super without a window.");
    return 0;
}
static void SuperRelease(Arena *A,unsigned int si) {
    Super *s = &A->S[si];
    Window *w = WindowOfSuper(A,si);
    BlockClear(A,si,SupOrder,0);
    munmap(s->base,SupSize);
    A->Munmap++;
    w->slot[(s->base-w->base)>>SupOrder] = NoSlot;
    s->base = 0;
    s->mask = 0;
    A->Mapped--;
}
 
/*_____________________________________________________________________________
 |
 | Buddy
 |_____________________________________________________________________________
*/
 
/* Take a block of order j: returns the slot and the offset in it. */
static unsigned int Take(Arena *A,unsigned int j,uint32_t *off) {
    unsigned int si,first = A->nS,k;
    uint32_t i;
    Super *s;
    for (si=A->low;si<A->nS;si++) {
        s = &A->S[si];
        if (!s->base || !s->mask) continue;
        if (first==A->nS) first = si;
        if (s->mask>>(j-MinOrder)) break;
    }
    A->low = first;
    if (si==A->nS) si = SuperNew(A);
    s = &A->S[si];
    k = j+(unsigned int)__builtin_ctz(s->mask>>(j-MinOrder));
    i = LowestBlock(s,k);
    BlockClear(A,si,k,i);
    while (k>j) {                       /* split: keep the low half, free the high half */
        k--; i <<= 1;
        BlockSet(A,si,k,i+1);
    }
    *off = i<<j;
    return si;
}
/* Free the block (k,i), merging with its free buddies. */
static void FreeBlock(Arena *A,unsigned int si,unsigned int k,uint32_t i) {
    while (k<SupOrder && TestBit(&A->S[si],k,i^1)) {
        BlockClear(A,si,k,i^1);
        i >>= 1; k++;
    }
    BlockSet(A,si,k,i);
    if (k==SupOrder && A->FreeSupers>1) SuperRelease(A,si);
}
/* Free [off,off+len) of a super as the biggest aligned blocks. */
static void FreeRange(Arena *A,unsigned int si,uint32_t off,uint32_t len) {
    while (len) {
        unsigned int k = off ? (unsigned int)__builtin_ctz(off) : SupOrder;
        unsigned int m = 31-(unsigned int)__builtin_clz(len);
        if (m<k) k = m;
        if (k>SupOrder) k = SupOrder;
        FreeBlock(A,si,k,off>>k);
        off += 1u<<k; len -= 1u<<k;
    }
}
 
/* Give back the highest big free blocks that may be resident, down to target pages. */
static void Trim(Arena *A,uint64_t target) {
    unsigned int si,k;
    for (si=A->nS;si-->0;) {
        Super *s = &A->S[si];
        if (!s->base) continue;
        for (k=SupOrder;k>=ReleaseMinOrder;k--) {
            unsigned int w;
            if (!(s->mask&(1u<<(k-MinOrder)))) continue;
            for (w=Words(k);w-->0;) {
                uint64_t v = Bits(s,k)[w];
                while (v) {
                    unsigned int b = 63-(unsigned int)__builtin_clzll(v);
                    uint32_t i = (w<<6)+b;
                    unsigned int c = DirtyCount(s,k,i);
                    v &= ~(1ull<<b);
                    if (!c) continue;
                    madvise(s->base+((size_t)i<<k),(size_t)1<<k,MADV_DONTNEED);
                    A->Madvise++;
                    SetRange(s->dirty,i<<(k-PageShift),1u<<(k-PageShift),0);
                    A->DirtyBig -= c;
                    if (A->DirtyBig<=target) return;
                }
            }
        }
    }
}
 
/*_____________________________________________________________________________
 |
 | The source
 |_____________________________________________________________________________
*/
 
static void *ArenaGet(TlsChunkSource *this,unsigned int sz,unsigned int align) {
    TlsGetThat(Arena,TlsChunkSource);
    Arena *A = that;
    unsigned int j,si;
    uint32_t off;
    char *r;
    if (sz<(1u<<MinOrder) || (sz&((1u<<MinOrder)-1)) || !align || (align&(align-1)) || align>SupSize)
        Crit("Chunk Get: bad size or alignment.");
    if (sz>SupSize) {                           /* big: mapped alone, page aligned */
        if (align>PageSize) Crit("Chunk Get: bad size or alignment.");
        r = mmap(0,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if (r==MAP_FAILED) Crit("Out of Memory");
        A->Mmap++;
        A->InUse += sz;
        return r;
    }
    j = (sz&(sz-1)) ? 32-(unsigned int)__builtin_clz(sz) : 31-(unsigned int)__builtin_clz(sz);
    if (align>(1u<<j)) Crit("Chunk Get: bad size or alignment.");
    si = Take(A,j,&off);
    if ((1u<<j)>sz) FreeRange(A,si,off+sz,(1u<<j)-sz);      /* the tail goes back */
    MarkDirty(&A->S[si],off,sz);
    A->InUse += sz; A->SupUsed += sz;
    return A->S[si].base+off;
}
static void ArenaPut(TlsChunkSource *this,void *mem,unsigned int sz) {
    TlsGetThat(Arena,TlsChunkSource);
    Arena *A = that;
    unsigned int si;
    uint32_t off;
    uint64_t allow;
    if (sz>SupSize) {
        munmap(mem,sz);
        A->Munmap++;
        A->InUse -= sz;
        return;
    }
    si = SuperOf(A,mem);
    off = (uint32_t)((char *)mem-A->S[si].base);
    if (!(sz&(sz-1))) {
        unsigned int k = 31-(unsigned int)__builtin_clz(sz);
        if (off&(sz-1)) Crit("Chunk Put: not a block of this source.");
        if (TestBit(&A->S[si],k,off>>k)) Crit("Chunk Put: block already free.");
        FreeBlock(A,si,k,off>>k);
    } else FreeRange(A,si,off,sz);
    A->InUse -= sz; A->SupUsed -= sz;
    allow = A->InUse/CacheDiv;
    if (allow<CacheMin) allow = CacheMin;
    if ((A->DirtyBig<<PageShift)>allow) Trim(A,(allow/2)>>PageShift);
}
static void ArenaClose(TlsChunkSource *this) {
    TlsGetThat(Arena,TlsChunkSource);
    Arena *A = that;
    unsigned int i,j,k;
    for (i=0;i<A->nW;i++) {
        Window *w = &A->W[i];
        /* Only what is still mapped: the live supers and the ones not taken yet.
         * A released super is a hole that may now belong to someone else. */
        for (j=0;j<w->n;j=k) {
            int mapped = (j>=w->used || w->slot[j]!=NoSlot);
            for (k=j+1;k<w->n && (k>=w->used || w->slot[k]!=NoSlot)==mapped;k++) ;
            if (mapped) munmap(w->base+(size_t)j*SupSize,(size_t)(k-j)*SupSize);
        }
        free(w->slot);
    }
    free(A->W);
    free(A->S);
    free(A);
}
static struct TlsChunkSource ArenaStatic = {ArenaGet,ArenaPut,ArenaClose};
 
TlsChunkSource *TlsChunkSourceArena(void) {
    Arena *A = calloc(1,sizeof(*A));
    if (!A) Crit("Out of Memory");
    A->TlsChunkSource.Static = &ArenaStatic;
    A->nextWin = WinFirst;
    A->low = 0;
    return &A->TlsChunkSource;
}
 
/*_____________________________________________________________________________
 |
 | Introspection, for tests.
 |_____________________________________________________________________________
*/
 
void TlsChunkArenaStats(TlsChunkSource *this,TlsChunkArenaInfo *Info) {
    TlsGetThat(Arena,TlsChunkSource);
    Info->InUse = that->InUse;
    Info->Supers = that->Mapped;
    Info->FreeSupers = that->FreeSupers;
    Info->FreeCached = (unsigned long)that->Mapped*SupSize-that->SupUsed;
    Info->DirtyFreeBig = that->DirtyBig<<PageShift;
    Info->Madvise = that->Madvise;
    Info->Mmap = that->Mmap;
    Info->Munmap = that->Munmap;
}
 
