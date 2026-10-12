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
 
#ifndef _Tools_Core_Debug_h_
#define _Tools_Core_Debug_h_
 
#include <Tools_Core.h>
#include <Tools_Chunk.h>
 
/*_____________________________________________________________________________
 |
 | Troubleshooting and test hooks. Nothing here is part of the contract of the
 | components: it looks inside them, and may change with their implementation.
 |_____________________________________________________________________________
*/
 
/* Main pool (Tools_MainPool.c).
 *
 * ChunkStats: chunks taken from and given back to the chunk source since the
 *   pool was opened; got-put is what the pool currently holds (in use + spares).
 * SlabInfo: for the handle of a slab bucket, returns 1 and the chunk size and
 *   the number of objects per chunk; returns 0 for the other kinds of bucket.
 */
void TlsMainPoolChunkStats(TlsMemPool *Handle,unsigned long *got,unsigned long *put);
int TlsMainPoolSlabInfo(TlsMemPool *Handle,unsigned int *Chunk,unsigned int *N);
/* Align: the alignment every block of the bucket has (a power of 2). */
unsigned int TlsMainPoolAlign(TlsMemPool *Handle);
 
/* Chunk arena (Tools_ChunkArena.c), for a source made by TlsChunkSourceArena.
 *
 * ArenaStats: InUse is the bytes handed out; Supers the 4 MiB regions mapped and
 *   FreeSupers those that are wholly free; FreeCached the free bytes in the
 *   supers; DirtyFreeBig the bytes of free blocks of 64 KiB or more that may be
 *   resident (what a release would give back); and the system calls made.
 */
typedef struct {
    unsigned long InUse,FreeCached,DirtyFreeBig;
    unsigned long Madvise,Mmap,Munmap;
    unsigned int Supers,FreeSupers;
} TlsChunkArenaInfo;
void TlsChunkArenaStats(TlsChunkSource *Src,TlsChunkArenaInfo *Info);
 
#endif
 
