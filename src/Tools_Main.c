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
 
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <Tools_Main.h>
 
/* See Tools_Main.h. Prototypes of the pool constructors (to go in Tools_Core.h). */
TlsMemPool *TlsMainPoolOpenWith(TlsIntCounter *Count,TlsChunkSource *Src);
TlsMemPool *TlsMemPoolShared(TlsMemPool *child);
TlsMemPool *TlsMemPoolBuffered(TlsMemPool *child,unsigned int buffersize);
 
#define DefStack (1u<<20)
#define DefHeapGrowth 65536u
#define DefPoolBuffer 65536u
 
/* The accounting of the main pool, before any heap exists: a static atomic
 * counter (the pool is shared, but the counter is also read at exit). */
static atomic_int Bytes;
static int BytesAdd(TlsIntCounter *this,int v) {
    (void)this;
    return atomic_fetch_add_explicit(&Bytes,v,memory_order_relaxed)+v;
}
static int BytesSet(TlsIntCounter *this,int v) {
    (void)this;
    atomic_store_explicit(&Bytes,v,memory_order_relaxed);
    return v;
}
static struct TlsIntCounter BytesStatic = {BytesAdd,BytesSet};
static TlsIntCounter BytesCounter = {&BytesStatic};
 
typedef struct {
    int (*app)(int,char **);
    int argc;
    char **argv;
    int Result;
} Run;
 
/* The first process of the main thread. It has no gate: it is the root of the
 * tree of threads (TlsGateUp() is NULL). */
static void MainProc(void *arg) {
    Run *r = arg;
    r->Result = r->app(r->argc,r->argv);
}
 
int TlsMainRun(const TlsMainConfig *cfg,int argc,char **argv,int (*app)(int,char **)) {
    TlsMainConfig c = {0,DefStack,DefHeapGrowth,DefPoolBuffer};
    TlsChunkSource *Src;
    TlsMemPool *Main,*Shared,*B;
    TlsStack *s;
    TlsHeap *h;
    TlsGateNod *home;
    Run run;
    int Left;
    if (cfg) {
        if (cfg->Source) c.Source = cfg->Source;
        if (cfg->Stack) c.Stack = cfg->Stack;
        if (cfg->HeapGrowth) c.HeapGrowth = cfg->HeapGrowth;
        if (cfg->PoolBuffer) c.PoolBuffer = cfg->PoolBuffer;
    }
    Src = c.Source ? c.Source : TlsChunkSourceArena();
    atomic_store(&Bytes,0);
    Main = TlsMainPoolOpenWith(&BytesCounter,Src);
    Shared = TlsMemPoolShared(Main);
    TlsGateSharedSet(Shared);
    B = TlsMemPoolBuffered(Shared,c.PoolBuffer);
    s = TlsStackOpen(B,c.HeapGrowth,0);
    h = TlsStackHeap(s);
    x_(h,Set);
 
    home = malloc(TlsGateHomeSize());       /* the home of the main thread: before the heap is */
    if (!home) { x_(&TlsCritErrorShutdown,Signal,"Out of memory."); abort(); }
    run.app = app; run.argc = argc; run.argv = argv; run.Result = 1;
    TlsGateHomeRun(home,c.Stack,MainProc,&run);
    free(home);
 
    x_(h,Set);                              /* the heap of this thread, whatever the process left */
    TlsHeapClose(h);                        /* hooks: the threads left are shut down */
    x_(B,Close);
    Left = x_(&BytesCounter,Add,0);
    x_(Shared,Close);                       /* the main pool, and its source, with it */
    if (Left) {
#ifndef NDEBUG
        fprintf(stderr,"TlsMain: %d bytes still allocated in the main pool at exit.\n",Left);
#endif
    }
    return run.Result;
}
 
#ifndef TLS_MAIN_NO_MAIN
extern int TlsMain(int argc,char **argv);
extern const TlsMainConfig TlsMainConfigApp __attribute__((weak));
int main(int argc,char **argv) {
    return TlsMainRun(&TlsMainConfigApp ? &TlsMainConfigApp : 0,argc,argv,TlsMain);
}
#endif
 
