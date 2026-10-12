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

#include <Tools_Core.h>
#include <stdio.h>
#include <stdint.h>

struct TlsBNod { unsigned int d[4];};

typedef struct {
    TlsBNodProcess TlsBNodProcess;
    char *Sep;
} NodPrintClos;
static int NodPrintFound(TlsBNodProcess *this,TlsBNod *nod,unsigned int Key) {
    TlsGetThat(NodPrintClos,TlsBNodProcess);
    printf("%s%d",that->Sep,Key);
    that->Sep=",";
    return (0!=0);
}
static int NodPrint(TlsBTree *Tree){
    static struct TlsBNodProcess Static = {&NodPrintFound};
    NodPrintClos clos = {{&Static},""};
    printf("[");
    x_(Tree,Fwd.ForEach,&clos.TlsBNodProcess); clos.Sep="";
    printf("]");
}
static int NodBackPrint(TlsBTree *Tree,int from){
    static struct TlsBNodProcess Static = {&NodPrintFound};
    NodPrintClos clos = {{&Static},""};
    x_(Tree,Bck.ForEachFrom,from,&clos.TlsBNodProcess); clos.Sep="";
    printf("]");
}

typedef struct {
    TlsBNod TlsBNod;
    int Radius;
} HanoiPlate;
typedef struct {
    TlsBTreeRoot rt;
    TlsBTree *p;
} HanStack;
typedef struct {
    HanoiPlate Plates[16];
    unsigned int *Ids;
    HanStack FreePlates,Towers[3];
    int BottomSz;
} HanState;
static void HanoiPrint(HanState *s) {
    printf("[");
    NodPrint(s->Towers[0].p);
    printf("-");
    NodPrint(s->Towers[1].p);
    printf("-");
    NodPrint(s->Towers[2].p);
    printf("]");
}

static void HanPlateMove(HanState *State,int tgt,int Plate);
static void HanTowerMove(HanState *State,int Tgt,int base) {
    int Top[3],top;
    int i;
    for (i=0;i<3;i++) {Top[i]=0; TlsBNodGetLowest(State->Towers[i].p,Top+i);}
    top = Top[0]; if (top>Top[1]) top=Top[1]; if (top>Top[2]) top=Top[2];
    while (base>=top) {
        for (i=0;i<3;i++) {
             Top[i]=base; 
             if (TlsBNodGetHighest(State->Towers[i].p,Top+i)==TlsBNodEnd) Top[i]=-1;
        }
        base = Top[0]; if (Top[1]>base) base=Top[1]; if (Top[2]>base) base=Top[2];
        HanPlateMove(State,Tgt,base); 
        base=base-1;
    }
}
static void HanPlateMove(HanState *State,int tgt,int Plate) {
    TlsBNod *f;
    f = x_(State->Towers[tgt].p,Seek,Plate);
    if (f==TlsBNodEnd) {
        int Bottom[3];
        int src,aux;
        src = (tgt+1)%3; aux = (tgt+2)%3;
        if (x_(State->Towers[src].p,Seek,Plate)==TlsBNodEnd) { src=aux; aux= (tgt+1)%3; }
        HanTowerMove(State,aux,Plate-1);
        printf("(%d->%d)",Plate,tgt);
        f = x_(State->Towers[src].p,Remove,Plate);
        if (f!=TlsBNodEnd) {
            x_(State->Towers[tgt].p,Insert,f,Plate);
        }
        HanoiPrint(State);
        printf("\n");
    }
}
static void InitHanoi(HanState *State,int nbPlates,int seed) {
    int lowest = 0;
    printf("Towers inited: %d plates with %d to:",nbPlates,seed);
    nbPlates--;
    while (nbPlates>=0) {
        TlsBNod *f;
        unsigned int k = State->Ids[nbPlates];
        if (k>lowest) lowest = k;
        f = x_(State->FreePlates.p,Remove,k);
        if (f!=TlsBNodEnd) {
            f = x_(State->Towers[seed%3].p,Insert,f,k);
        }
        seed = seed/3; nbPlates--;
    }
    State->BottomSz = 1+lowest;
    HanoiPrint(State);
    printf(", plates left:");
    NodPrint(State->FreePlates.p);
    printf(".\n");
}
typedef struct {
    TlsBNodProcess TlsBNodProcess;
    TlsBTree *free;
} cleanHanoiClos;
static int cleanHanoiProcess(TlsBNodProcess *this,TlsBNod *f,unsigned int K) {
    TlsGetThat(cleanHanoiClos,TlsBNodProcess);
    printf("free(%d) ",K);
    x_(that->free,Insert,f,K);
}
static void CleanHanoi(HanState *State) {
    static struct TlsBNodProcess Static = {&cleanHanoiProcess};
    cleanHanoiClos clos = {{&Static},State->FreePlates.p};
    x_(State->Towers[0].p,Clear,&clos.TlsBNodProcess);
    x_(State->Towers[1].p,Clear,&clos.TlsBNodProcess);
    x_(State->Towers[2].p,Clear,&clos.TlsBNodProcess);
    printf("Hanoi cleaned:");
    HanoiPrint(State);
    printf("\nFree:"); NodPrint(State->FreePlates.p);
    printf("\n");
}
int main() {
    unsigned int PlatesId[16] = {0,1,2,3, 4,9,13,25, 26,27,29,30, 31,134,242,562};
    HanState Hanoi;
    int i;
    printf("Testing lists, binary trees, rad trees...\n");
    printf("TlsBNod size: %d (int:%d) (int *:%d)\n",s_(TlsBNod),s_(int),s_(int *));
    printf("Log2(1)=%d, Log2(2)=%d, Log2(9)=%d, Log2(129823)=%d\n",TlsLog2(1),TlsLog2(2),TlsLog2(9),TlsLog2(129823));
    Hanoi.Ids = PlatesId;
    Hanoi.FreePlates.p = Tls31RadixInit(&Hanoi.FreePlates.rt,s_(TlsBNod));
    for (i=0;i<3;i++) Hanoi.Towers[i].p = Tls31RadixInit(&Hanoi.Towers[i].rt,s_(TlsBNod));
    {
        printf("Populating AllPlates:\n");
        for (i=0;i<16;i++) {
             x_(Hanoi.FreePlates.p,Insert,&Hanoi.Plates[i^6].TlsBNod,PlatesId[i^6]);
             NodPrint(Hanoi.FreePlates.p);
             printf("   Bck from 26:[");
             NodBackPrint(Hanoi.FreePlates.p,26);
             printf("\n");
        }
        printf("\n");
        InitHanoi(&Hanoi,6,0x543);
        CleanHanoi(&Hanoi);
        InitHanoi(&Hanoi,6,0x543);
        printf("Moving to Tower 0:\n");
        HanTowerMove(&Hanoi,0,9);
        printf("Moving to Tower 1:\n");
        HanTowerMove(&Hanoi,1,9);
        CleanHanoi(&Hanoi);
    }
}
