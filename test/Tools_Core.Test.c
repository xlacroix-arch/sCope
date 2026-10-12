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
#include <stdint.h>
#include <stdio.h>

struct c_Core {
    struct {
        char First,Second,Third,Fourth,Fifth;
    } Numeral;
    struct {
        char TopLeft,BottomLeft,TopRight,BottomRight;
    } From;
    struct {
        struct { char x,y; } TopLeft,BottomLeft,TopRight,BottomRight;
    } View;
};

typedef struct { int x,y,z; TlsList List; } PointNod;

TlsClassDef(CoreMsg) {
    void (*log)(CoreMsg *this);
};

typedef struct {
    CoreMsg CoreMsg;
    char *Msg;
} CoreMsgString;
static void CoreMsgStringLog(CoreMsg *this) {
    TlsGetThat(CoreMsgString,CoreMsg);
    printf(that->Msg);
}
CoreMsg *CoreMsgStringNew(char *Msg) {
    CoreMsgString *r;
    static struct CoreMsg Static = {CoreMsgStringLog};
    rPush(r);
    r->CoreMsg.Static = &Static;
    r->Msg = Msg;
    return &r->CoreMsg;
}

typedef struct {
    CoreMsg CoreMsg;
    CoreMsg *c[2];
} CoreMsgPair;
static void CoreMsgPairLog(CoreMsg *this) {
    TlsGetThat(CoreMsgPair,CoreMsg);
    printf("[\"");
    x_(that->c[0],log);
    printf("\",\"");
    x_(that->c[1],log);
    printf("\"]");
}
CoreMsg *CoreMsgPairNew(CoreMsg *A,CoreMsg *B) {
    CoreMsgPair *r;
    static struct CoreMsg Static = {CoreMsgPairLog};
    rPush(r);
    r->CoreMsg.Static = &Static;
    r->c[0] = A;
    r->c[1] = B;
    return &r->CoreMsg;
}

TlsClassDef(IntFn) {
    int (*Val)(IntFn *this,int a);
};

typedef struct {
    IntFn IntFn;
    int (*fn)(int a);
} IntFnDirect;
static int IntFnDirectVal(IntFn *this,int a) {
    TlsGetThat(IntFnDirect,IntFn);
    return that->fn(a);
}
IntFn *IntFnDirectNew(int (*fn)(int a)) {
    IntFnDirect *r;
    static struct IntFn Static = {IntFnDirectVal};
    rPush(r);
    r->IntFn.Static = &Static;
    r->fn = fn;
    return &r->IntFn;
}

typedef struct {
    IntFn IntFn;
    IntFn *F,*G;
} IntFnCompose;
static int IntFnComposeVal(IntFn *this,int a) {
    int r;
    TlsGetThat(IntFnCompose,IntFn);
    r = x_(that->G,Val,a);
    r = x_(that->F,Val,r);
    return r;
}
IntFn *IntFnComposeNew(IntFn *F,IntFn *G) {
    IntFnCompose *r;
    static struct IntFn Static = {IntFnComposeVal};
    rPush(r);
    r->IntFn.Static = &Static;
    r->F = F; r->G = G;
    return &r->IntFn;
}

struct TlsBNod {
    union {
        uint32_t pL;
        struct {
           unsigned int pH : 14;
           unsigned int KeyL : 18;
        } d1;
        struct {
           unsigned int pH : 14;
           unsigned int KeyL: 13;
           unsigned int bit : 5;
        } d3;
    } d[4];
};

int main() {
    char Buffer[4096];
    TlsHeap_.p = Buffer; TlsHeap_.e=Buffer+4096;
    
    printf("TlsBNod size: %d\n",s_(TlsBNod));
    printf("First:%d Second:%d Third:%d\n",c_(Core,Numeral,First),c_(Core,Numeral,Second),c_(Core,Numeral,Third));
    printf("BottomRight.Idx:%d TopLeft.x.Idx:%d TopRight.y.Idx:%d\n",c_(Core,View,BottomRight),c_(Core,View,TopLeft.x),c_(Core,View,TopRight.y));
    printf("sizeof(int)=%d sizeof(long)=%d sizeof(long long)=%d sizeof(short)=%d\n",s_(int),s_(long),s_(long long),s_(short));
    {  PointNod PointPool[10];
       TlsList pl;
       pl.n = &TlsListEnd;
       { PointNod *ppn=PointPool; 
          int i=0;
          while (i<10) { ppn->x=ppn->y=ppn->z=i; ppn->List.n = pl.n; pl.n=&ppn->List; i++; ppn++; }
       }
       { TlsList *p;
         p = pl.n; 
         while (p!=&TlsListEnd) { 
             PointNod *cp = v_(PointNod,List,p); 
             printf("[%d,%d,%d] ",cp->x,cp->y,cp->z);  
             p=p->n;
         }
         printf("\n");
       }
    }
    { 
        CoreMsg *Msg = CoreMsgPairNew(CoreMsgStringNew("Hello World!"),CoreMsgStringNew("How do you do fellow programmers?"));
        x_(Msg,log);
        printf("\n");
    }
    printf("Log2(1)=%d, Log2(2)=%d, Log2(9)=%d, Log2(129823)=%d\n",TlsLog2(1),TlsLog2(2),TlsLog2(9),TlsLog2(129823));
}

