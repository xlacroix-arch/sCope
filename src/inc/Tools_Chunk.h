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
 
#ifndef _Tools_Chunk_h_
#define _Tools_Chunk_h_
 
#include <Tools_Class.h>
 
/*_____________________________________________________________________________
 |
 | A chunk source hands out the aligned blocks pools are built on: the
 | malloc/mmap abstraction under a TlsMemPool (to go in Tools_Core.h).
 |
 |    - Get(this,sz,align) returns a block of sz bytes aligned on align (a
 |      power of 2). sz is a multiple of 1024; align is at most 4 MiB, and at
 |      most the lowest bit of sz (a power of 2 can be aligned on itself).
 |      The contents are not specified. It never returns NULL: failure is a
 |      critical error (TlsCritErrorShutdown).
 |    - Put(this,mem,sz) gives a block back. sz is the one given to Get.
 |    - Close releases the source and every block it still holds, in use or
 |      not (blocks still allocated are the caller's affair).
 |    A source is used by one thread at a time.
 |
 | TlsChunkSourceLibc: aligned_alloc and free. The reference behaviour: simple,
 |    but the memory of small chunks is not given back to the system, and big
 |    ones cost a system call each and twice their size of address space.
 | TlsChunkSourceArena: Linux (mmap, madvise). A buddy allocator over regions
 |    aligned on 4 MiB, placing blocks at the lowest address and giving free
 |    memory back to the system. See Tools_ChunkArena.c.
 | Each call to TlsChunkSourceArena makes a new arena; the libc source is shared.
 |_____________________________________________________________________________
*/
TlsClassDef(TlsChunkSource) {
    void *(*Get)(TlsChunkSource *this,unsigned int sz,unsigned int align);
    void (*Put)(TlsChunkSource *this,void *mem,unsigned int sz);
    void (*Close)(TlsChunkSource *this);
};
 
TlsChunkSource *TlsChunkSourceLibc(void);
TlsChunkSource *TlsChunkSourceArena(void);
 
#endif
 
