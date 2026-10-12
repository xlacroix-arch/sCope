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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <Tools_Core.h>


/*---------------------*/

int TlsLog2(unsigned int v) {
    return ((s_(unsigned int)<<3)-(1+__builtin_clz(v)));
}
int TlsLog2_(unsigned int v) {
    unsigned int msk=(((unsigned int)-1)<<(s_(unsigned int)<<2));
    struct log2loc {
        unsigned int v;
        int r;
    } c = {v,0},cn[2];
    int di = s_(unsigned int)<<2;
    while (di>1) {
        cn[0==0].r = c.r+di;
        cn[0!=0].r = c.r;
        di = di>>1;
        cn[0==0].v = c.v>>di;
        cn[0!=0].v = c.v<<di;
        c = cn[(c.v&msk)!=0];
    }
    cn[0!=0].r = c.r; cn[0==0].r = c.r+1;
    return cn[(c.v&msk)!=0].r;
}


/*---------------------*/

TlsList TlsListEnd = {&TlsListEnd};

/*---------------------*/

/*
static int CritErrorExit(TlsSignalString *this,char *Msg) {
    fprintf(stderr,"Critical Error: %s\n");
    exit(0);
}
static struct TlsSignalString CritErrorExitStatic = {CritErrorExit};
TlsSignalString TlsCritErrorShutdown = {&CritErrorExitStatic};
*/

     /*---------------------*/

typedef struct {
    TlsSignal TlsSignal;
    char *Msg;
    TlsSignalString *Tgt;
} MsgTrigger;
static int SignalMsgSignal(TlsSignal *this) {
    TlsGetThat(MsgTrigger,TlsSignal);
    return x_(that->Tgt,Signal,that->Msg);
}
TlsSignal *TlsSignalMessage(TlsSignalString *Tgt,char *Msg) {
    MsgTrigger *r;
    static struct TlsSignal Static = {&SignalMsgSignal};
    rPush(r);
    r->TlsSignal.Static = &Static;
    r->Tgt = Tgt;
    r->Msg = Msg;
    return &r->TlsSignal;
}

static struct TlsSignal OOMStatic = {&SignalMsgSignal};
static MsgTrigger OOMExit = {
    {&OOMStatic},"Out of Memory",&TlsCritErrorShutdown
};
TlsSignal *TlsCritErrorOutOfMemory=&OOMExit.TlsSignal;

/*______________________________________________________________________
 |
 | IntCounter
 |______________________________________________________________________
*/

static int IntCounterNullAdd(TlsIntCounter *this,int val) { return 0; }
static int IntCounterNullSet(TlsIntCounter *this,int val) { return 0; }
static struct TlsIntCounter IntCounterNullStatic = {IntCounterNullAdd,IntCounterNullSet};
TlsIntCounter TlsIntCounterNull = {&IntCounterNullStatic};

    /*---------------*/

typedef struct {
    TlsIntCounter TlsIntCounter;
    int Val;
} IntCounterDirect;
static int IntCounterDirectAdd(TlsIntCounter *this,int val) {
    TlsGetThat(IntCounterDirect,TlsIntCounter);
    that->Val+=val;
    return that->Val;
}
static int IntCounterDirectSet(TlsIntCounter *this,int val) {
     TlsGetThat(IntCounterDirect,TlsIntCounter);
     that->Val=val;
     return that->Val;
}
TlsIntCounter *TlsIntCounterDirect(int val) {
    static struct TlsIntCounter Static = {IntCounterDirectAdd,IntCounterDirectSet};
    IntCounterDirect *r;
    rPush(r);
    r->TlsIntCounter.Static = &Static;
    r->Val = val;
    return &r->TlsIntCounter;
}

   /*-------------------*/

typedef struct {
    TlsIntCounter TlsIntCounter;
    TlsIntCounter *Acc;
    int Val;
} IntCounterAccumulate;
static int IntCounterAccAdd(TlsIntCounter *this,int val) {
    TlsGetThat(IntCounterAccumulate,TlsIntCounter);
    x_(that->Acc,Add,val);
    that->Val += val;
    return that->Val;
}
static int IntCounterAccSet(TlsIntCounter *this,int val) {
    TlsGetThat(IntCounterAccumulate,TlsIntCounter);
    x_(that->Acc,Add,val-that->Val);
    that->Val = val;
    return val;
}
TlsIntCounter *TlsIntCounterAccumulate(TlsIntCounter *Acc,int val) {
    IntCounterAccumulate *r;
    static struct TlsIntCounter Static = {IntCounterAccAdd,IntCounterAccSet};
    rPush(r);
    r->TlsIntCounter.Static = &Static;
    r->Acc = Acc;
    r->Val = val;
    x_(Acc,Add,val);
    return &r->TlsIntCounter;
}

/*---------------------*/

typedef struct {
    TlsIntCounter TlsIntCounter;
    TlsIntCounter *Tgt;
    int Val,Threshold;
    TlsSignal *Signal;
} IntCounterWatch;
static int IntCountWatchAdd(TlsIntCounter *this,int val){
    int v0,v1,t;
    TlsGetThat(IntCounterWatch,TlsIntCounter);
    v0 = that->Val;
    t = that->Threshold;
    that->Val = v1 = x_(that->Tgt,Add,val);
    if ((v0>t)!=(v1>t))  { x_(that->Signal,Signal); }
    return v1;
}
static int IntCountWatchSet(TlsIntCounter *this,int val) {
    int v0,v1,t;
    TlsGetThat(IntCounterWatch,TlsIntCounter);
    v0 = that->Val;
    t = that->Threshold;
    that->Val = v1 = x_(that->Tgt,Set,val);
    if ((v0>t)!=(v1>t)) { x_(that->Signal,Signal); }
    return v1;
}
TlsIntCounter *TlsIntCounterWatch(TlsIntCounter *Tgt,int threshold,TlsSignal *signal) {
    IntCounterWatch *r;
    static struct TlsIntCounter Static = {IntCountWatchAdd,IntCountWatchSet};
    rPush(r);
    r->TlsIntCounter.Static = &Static;
    r->Tgt = Tgt;
    r->Val = x_(Tgt,Add,0);
    r->Threshold = threshold;
    r->Signal = signal;
    return &r->TlsIntCounter;
}


