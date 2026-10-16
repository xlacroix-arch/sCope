/*  Fondement Michtam
 *  Copyright (C) 2024 Xavier Lacroix
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
 
#ifndef _Tools_Main_h_
#define _Tools_Main_h_
 
#include <Tools_Core.h>
#include <Tools_Chunk.h>
#include <Tools_Gate.h>
 
/*_____________________________________________________________________________
 |
 | The entry point. An application does not have a main(): it is a library or
 | an object file that defines
 |
 |      int TlsMain(int argc,char **argv);
 |
 | and Tools_Main.c, which defines main(), builds the world around it:
 |
 |   1. a chunk source (the arena, unless the configuration gives another);
 |   2. the main pool on it, wrapped by TlsMemPoolShared: every thread of the
 |      program allocates from it (it is given to TlsGateSharedSet);
 |   3. the heap of the main thread, on a TlsMemPoolBuffered over the shared
 |      pool, exactly like the heap of a thread made by TlsGateSet;
 |   4. nothing for gates: the main thread is the root of the tree of threads and
 |      has no gate above it (TlsGateUp() is NULL); the threads TlsMain spawns
 |      with TlsGateSet are its children, and it holds the parent ends;
 |   5. TlsMain itself, as the first process of the main thread: on its own
 |      execution stack (Stack bytes) and on a heap forked from the root one
 |      (see TlsGateHomeRun). It is the current heap: rPush works at once.
 |
 | When TlsMain returns, its value is the exit code of the program, and
 |   - the heap of the process is released, which (stack hooks) shuts down the
 |     gate threads it spawned and did not shut down itself, children first;
 |   - the pools are closed, from the heap to the chunk source;
 |   - in debug builds (NDEBUG not defined) bytes still allocated in the main
 |     pool are reported on stderr: they were leaked by someone.
 | Calling exit() from TlsMain skips all this: return instead.
 |
 | TLS_MAIN_NO_MAIN: compile Tools_Main.c with it to provide TlsMainRun only,
 | for a program that wants its own main() (or several entry points, tests ...).
 |_____________________________________________________________________________
*/
 
/* Zero means the default, for every field. */
typedef struct {
    TlsChunkSource *Source;     /* chunks of the main pool; it is closed with it. Default: TlsChunkSourceArena() */
    unsigned int Stack;         /* execution stack of the main process. Default: 1 MiB */
    unsigned int HeapGrowth;    /* chunk size of the root heap. Default: 64 KiB */
    unsigned int PoolBuffer;    /* bytes of free blocks the main thread pool may hold. Default: 64 KiB */
} TlsMainConfig;
 
/* To be defined by the application. */
int TlsMain(int argc,char **argv);
 
/* An application that wants other settings defines this too (it is a weak
 * symbol: when absent, the defaults are used by main()). */
extern const TlsMainConfig TlsMainConfigApp;
 
/* What main() does: runs app as above. cfg may be NULL. Returns the exit code. */
int TlsMainRun(const TlsMainConfig *cfg,int argc,char **argv,int (*app)(int argc,char **argv));
 
#endif
 
