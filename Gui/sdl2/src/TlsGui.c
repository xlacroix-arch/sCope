/*  MIT License
 *
 *  Copyright (c) 2020 mtxavier
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <Tools_Core.h>
#include <TlsGuiCtrl.h>
#include <stdarg.h>

/*______________________________________________________________
 |
 | Inputs
 |______________________________________________________________
*/

static int TriggerNullTrigger(TlsGuiInTrigger *this,unsigned int date) { 
    return (0!=0);
}
static struct TlsGuiInTrigger InTriggerNullStatic = {&TriggerNullTrigger};
TlsGuiInTrigger TlsGuiInTriggerNull = {&InTriggerNullStatic};

    /*------------*/

static int KeyNullTrigger(TlsGuiInKey *this,unsigned int date,int k,int k0) {
    return (0!=0);
}
static struct TlsGuiInKey  InKeyNullStatic = {&KeyNullTrigger};
TlsGuiInKey TlsGuiInKeyNull = {&InKeyNullStatic};

    /*------------*/

static int SliderNullTrigger(TlsGuiInSlider *this,unsigned int date,int Val) {
    return (0!=0);
}
static struct TlsGuiInSlider InSliderNullStatic = {&SliderNullTrigger};
TlsGuiInSlider TlsGuiInSliderNull = {&InSliderNullStatic};

    /*------------*/

static int CursorNullTrigger(TlsGuiInCursor *this,unsigned int date,int x,int y) {
    return (0!=0);
}
static struct TlsGuiInCursor InCursorNullStatic = {&CursorNullTrigger};
TlsGuiInCursor TlsGuiInCursorNull = {&InCursorNullStatic};

    /*------------*/

static int InterpreterNullTrigger(TlsGuiInInterpreter *this,unsigned int date,unsigned char *msg) {
    return (0!=0);
}
static struct TlsGuiInInterpreter InInterpreterNullStatic = {&InterpreterNullTrigger};
TlsGuiInInterpreter TlsGuiInInterpreterNull = {&InInterpreterNullStatic};

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

void TlsGuiInputDevicesInit(TlsGuiInputDevices *N) {
    int i,j;
    N->Keyboard.Down = N->Keyboard.Repeat = N->Keyboard.Up = &TlsGuiInKeyNull;
    N->Mouse.Down = N->Mouse.Up = N->Mouse.DoubleClick = &TlsGuiInKeyNull;
    N->Mouse.Pos = &TlsGuiInCursorNull;
    N->Mouse.Wheel[0] = N->Mouse.Wheel[1] = &TlsGuiInSliderNull;
    for (i=0;i<4;i++) {
         N->Joy[i].Down = N->Joy[i].Up = &TlsGuiInKeyNull;
         for(j=0;j<12;j++) { N->Joy[i].Axis[j] = &TlsGuiInSliderNull; }
    }
    N->Window.Close = &TlsGuiInTriggerNull;
    N->Window.Iconize = N->Window.Restore = 
    N->Window.MouseEnter = N->Window.MouseLeave = 
    N->Window.KeyboardAcquired = N->Window.KeyboardLost = &TlsGuiInTriggerNull;
    N->Window.Resize = N->Window.Move = &TlsGuiInCursorNull;
}


/*______________________________________________________________
 |
 | Outputs
 |______________________________________________________________
*/

/*______________________________________________________________
 |
 | Palettes
 |______________________________________________________________
*/

static void NullPaletteUpdate(TlsGuiMappedPalette *this,int b,int e) {}
static int NullPaletteRelease(TlsGuiMappedPalette *this) {return (0!=0);}
static struct TlsGuiMappedPalette NullPaletteStatic = {NullPaletteUpdate,NullPaletteRelease};
TlsGuiMappedPalette TlsGuiMappedPaletteNull = {&NullPaletteStatic};

TlsRGBA TlsGuiPaletteGreyScale={0,0,0,0};
TlsRGBA TlsGuiPaletteColorKey={1,0,0,0xff};

TlsRGBA *TlsGuiPaletteBitmap(int bpp,int transparency) {
    static int Inited = (0!=0);
    static TlsRGBA 
        BM1bpp[] = {{0xff,0xff,0xff,0},{0xff,0xff,0xff,0xff}},
        Grey1bpp[] = {{0,0,0,0xff},{0xff,0xff,0xff,0xff}},
        BM2bpp[] = {{0xff,0xff,0xff,0},{0xff,0xff,0xff,0x55},{0xff,0xff,0xff,0xaa},{0xff,0xff,0xff,0xff}},
        Grey2bpp[] = {{0,0,0,0xff},{0x55,0x55,0x55,0xff},{0xaa,0xaa,0xaa,0xff},{0xff,0xff,0xff,0xff}},
        BM4bpp[16],Grey4bpp[16],BM8bpp[256],Grey8bpp[256];
    ;
    static TlsRGBA *Res[]={
        BM1bpp,Grey1bpp,BM2bpp,Grey2bpp,BM4bpp,Grey4bpp,BM4bpp,Grey4bpp,
        BM8bpp,Grey8bpp,BM8bpp,Grey8bpp,BM8bpp,Grey8bpp,BM8bpp,Grey8bpp
    };
    if (!Inited) {
        int i;
        TlsRGBA *bm,*gs;
        bm = BM4bpp; gs = Grey4bpp;
        for (i=0;i<0x100;i+=0x11) {
             bm->R=bm->G=bm->B=0xff; bm->A=i;
             gs->R=gs->G=gs->B=i; gs->A=0xff;
             bm++; gs++;
        }
        bm = BM8bpp; gs = Grey8bpp;
        for (i=0;i<0x100;i++) {
             bm->R=bm->G=bm->B=0xff; bm->A=i;
             gs->R=gs->G=gs->B=i; gs->A=0xff;
             bm++; gs++;
        }
        Inited = (0==0);
    }
    return Res[((bpp<<1)-1)-(transparency!=(0!=0))];
}

TlsRGBA *TlsGuiPaletteAddGrey(TlsRGBA *q,unsigned char start,unsigned char end,int nb) {
    int i;
    i = 0;
    while (i<nb) {
        int clr;
        clr = start+(((i+1)*(end-start))/(nb+1));
        q->R=q->G=q->B=clr; q->A=0xff;
        i++; q++;
    }
    return q;
}
TlsRGBA *TlsGuiPaletteAddGradient(TlsRGBA *q,TlsRGBA start,TlsRGBA end,int nb) {
    int i;
    i = 0;
    while (i<nb) {
        i++;
        q->R = start.R+((i*(end.R-start.R))/(nb+1));
        q->G = start.G+((i*(end.G-start.G))/(nb+1));
        q->B = start.B+((i*(end.B-start.B))/(nb+1));
        q->A = start.A+((i*(end.A-start.A))/(nb+1));
        *q++;
    }
    return q;
}

TlsRGBA *TlsGuiPaletteAddNonZero(TlsRGBA *q) {
    static TlsRGBA NZ={0,0,0,0};
    *q++ = NZ;
    return q;
}

TlsRGBA *TlsGuiPaletteAdd666(TlsRGBA *q) {
    static unsigned char c[8]={0,0x33,0x66,0x99,0xCC,0xFF,0,0};
    int r,g,b;
    TlsRGBA n;
    n.A=0xff;
    for (b=0;b<6;b) {
        n.B = c[b];
        for (g=0;g<6;g) {
            n.G = c[g];
            for (r=0;r<6;r) {
                n.R = c[r]; 
                *q++=n; 
            }
        }
    }
    return q;
}
TlsRGBA *TlsGuiPaletteAdd676(TlsRGBA *q) {
    static unsigned char c0[8]={0,0x33,0x66,0x99,0xCC,0xFF,0,0};
    static unsigned char c1[8]={0,0x2a,0x55,0x80,0xaa,0xd4,0xff,0};
    int r,g,b;
    TlsRGBA n;
    n.A=0xff;
    for (b=0;b<6;b) {
        n.B = c0[b];
        for (g=0;g<7;g) {
            n.G = c1[g];
            for (r=0;r<6;r) {
                n.R = c0[r]; 
                *q++=n; 
            }
        }
    }
    return q;
}
TlsRGBA *TlsGuiPaletteAdd685(TlsRGBA *q) {
    static unsigned char c0[8]={0,0x33,0x66,0x99,0xCC,0xFF,0,0};
    static unsigned char c1[8]={0,0x24,0x49,0x6d,0x92,0xb6,0xdb,0xff};
    static unsigned char c2[8]={0,0x40,0x80,0xbf,0xff,0,0,0};
    int r,g,b;
    TlsRGBA n;
    n.A=0xff;
    for (b=0;b<6;b) {
        n.B = c2[b];
        for (g=0;g<8;g) {
            n.G = c1[g];
            for (r=0;r<5;r) {
                n.R = c0[r]; 
                *q++=n; 
            }
        }
    }
    return q;
}
TlsRGBA *TlsGuiPaletteAdd884(TlsRGBA *q) {
    static unsigned char c0[8]={0,0x24,0x49,0x6d,0x92,0xb6,0xdb,0xff};
    static unsigned char c1[8]={0,0x55,0xaa,0xff,0,0,0,0};
    int r,g,b;
    TlsRGBA n;
    n.A=0xff;
    for (b=0;b<4;b) {
        n.B = c1[b];
        for (g=0;g<8;g) {
            n.G = c0[g];
            for (r=0;r<8;r) {
                n.R = c0[r]; 
                *q++=n; 
            }
        }
    }
    return q;
}

static TlsRGBA *TlsGuiPaletteAdd888Half(TlsRGBA *q,int p) {
    TlsRGBA n; n.A=0xff;
    TlsRGBA *Transparent=q+1;
    int i=0;
    while (i<256) {
        int c,c1,ip;
        ip=((i>>2)^(i>>5)^p)&1; ip=ip|(i<<1);
        c=(ip&0x1c0)<<15; c=c|((ip&0x38)<<10); c=c|(ip&7)<<5;
        c1=c|(c>>3)|((c&0xe0e0e0)>>6);
        n.R=c1; n.G=c1>>8; n.B=c1>>16; *q++=n;
        i++;
    }
    Transparent->A=0;
    return q;
}
TlsRGBA *TlsGuiPaletteAdd888Low(TlsRGBA *q) { return TlsGuiPaletteAdd888Half(q,0); }
TlsRGBA *TlsGuiPaletteAdd888High(TlsRGBA *q) { return TlsGuiPaletteAdd888Half(q,1);}


/*______________________________________________________________
 |
 | 
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsGuiOut *c[2];
} OutPair;
static void OutPairDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGetThat(OutPair,TlsGuiOut);
    x_(that->c[0],Display,Tgt);
    x_(that->c[1],Display,Tgt);
}

TlsGuiOut *TlsGuiOutPair(TlsGuiOut *A,TlsGuiOut *B) {
    OutPair *r;
    static struct TlsGuiOut Static = {OutPairDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->c[0] = A;
    r->c[1] = B;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    int nb;
    TlsGuiOut **c;
} OutList;

static void OutListDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGuiOut **c;
    int n;
    TlsGetThat(OutList,TlsGuiOut);
    n = that->nb;
    c = that->c;
    while (n) {
        n--;
        x_(*c,Display,Tgt);
        c++;
    }
}

TlsGuiOut *TlsGuiOutList(TlsGuiOut *A,...) {
    OutList *r;
    int nb;
    static struct TlsGuiOut Static = {OutListDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    {
        va_list l;
        TlsGuiOut *p;
        nb = 0;
        va_start(l,A);
        p = A;
        while (p) {
            nb++;
            p = va_arg(l,TlsGuiOut *);
        }
        va_end(l);
    }
    r->nb = nb;
    rnPush(r->c,nb);
    {
        va_list l;
        TlsGuiOut *p,**q;
        q = r->c;
        va_start(l,A);
        p = A;
        while (p) {
            *q++ = p;
            p = va_arg(l,TlsGuiOut *);
        }
        va_end(l);
    }
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

TlsGuiOut *TlsGuiOutMobArray(int nb,TlsGuiOut **b) {
    OutList *r;
    static struct TlsGuiOut Static = {OutListDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->nb = nb;
    r->c = b;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    int nb,*slct;
    TlsGuiOut **c;
} OutMobSelect;
static void OutMobSelectDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGuiOut *c;
    TlsGetThat(OutMobSelect,TlsGuiOut);
    c = that->c[*that->slct];
    x_(c,Display,Tgt);
}
TlsGuiOut *TlsGuiOutMobSelect(int *Selector,int nb,TlsGuiOut **b) {
    OutMobSelect *r;
    static struct TlsGuiOut Static = {OutMobSelectDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->nb = nb;
    r->slct = Selector;
    r->c = b;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dPoint Pos;
    TlsGuiOut *c;
} OutPos;

static void OutPosDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsI2dPoint Old;
    TlsGetThat(OutPos,TlsGuiOut);
    Old = Tgt->CursorPos;
    Tgt->CursorPos.x[0]+=that->Pos.x[0];
    Tgt->CursorPos.x[1]+=that->Pos.x[1];
    x_(that->c,Display,Tgt);
    Tgt->CursorPos = Old;
}

TlsGuiOut *TlsGuiOutPos(int x,int y,TlsGuiOut *Child) {
    OutPos *r;
    static struct TlsGuiOut Static = {OutPosDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Pos.x[0]=x; r->Pos.x[1]=y;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void OutGrabPointDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsI2dPoint Old;
    TlsGetThat(OutPos,TlsGuiOut);
    Old = Tgt->CursorPos;
    Tgt->CursorPos.x[0]-=that->Pos.x[0];
    Tgt->CursorPos.x[1]-=that->Pos.x[1];
    x_(that->c,Display,Tgt);
    Tgt->CursorPos = Old;
}
TlsGuiOut *TlsGuiOutGrabPoint(int x,int y,TlsGuiOut *Child) {
    OutPos *r;
    static struct TlsGuiOut Static = {OutGrabPointDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Pos.x[0] = x;
    r->Pos.x[1] = y;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/


typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsRGBA clr;
    TlsGuiOut *c;
} OutColor;
static void OutFrontClrDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsRGBA Old;
    TlsGetThat(OutColor,TlsGuiOut);
    Old = Tgt->Front;
    Tgt->Front = that->clr;
    x_(Tgt,SetFront);
    x_(that->c,Display,Tgt);
    Tgt->Front = Old;
    x_(Tgt,SetFront);
}

TlsGuiOut *TlsGuiOutFrontClr(int R,int G,int B,int A,TlsGuiOut *Child) {
    OutColor *r;
    static struct TlsGuiOut Static = {OutFrontClrDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->clr.R = R;
    r->clr.G = G;
    r->clr.B = B;
    r->clr.A = A;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void OutBackClrDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsRGBA Old;
    TlsGetThat(OutColor,TlsGuiOut);
    Old = Tgt->Back;
    Tgt->Back = that->clr;
    x_(Tgt,SetBack);
    x_(that->c,Display,Tgt);
    Tgt->Back = Old;
    x_(Tgt,SetBack);
}
TlsGuiOut *TlsGuiOutBackClr(int R,int G,int B,int A,TlsGuiOut *Child) {
    OutColor *r;
    static struct TlsGuiOut Static = {OutBackClrDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->clr.R = R;
    r->clr.G = G;
    r->clr.B = B;
    r->clr.A = A;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dRange Clip;
    TlsGuiOut *c;
} OutClip;
static void OutClipDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsI2dRange Old,New;
    TlsGetThat(OutClip,TlsGuiOut);
    New.b.x[0] = Tgt->CursorPos.x[0]+that->Clip.b.x[0];
    New.b.x[1] = Tgt->CursorPos.x[1]+that->Clip.b.x[1];
    New.e.x[0] = Tgt->CursorPos.x[0]+that->Clip.e.x[0];
    New.e.x[1] = Tgt->CursorPos.x[1]+that->Clip.e.x[1]; 
    if (New.b.x[0]<Tgt->Clip.b.x[0]) New.b.x[0]=Tgt->Clip.b.x[0];
    if (New.b.x[1]<Tgt->Clip.b.x[1]) New.b.x[1]=Tgt->Clip.b.x[1];
    if (New.e.x[0]>Tgt->Clip.e.x[0]) New.e.x[0]=Tgt->Clip.e.x[0];
    if (New.e.x[1]>Tgt->Clip.e.x[1]) New.e.x[1]=Tgt->Clip.e.x[1];
    if ((New.b.x[0]<New.e.x[0])&&(New.b.x[1]<New.e.x[1])) {
        Old = Tgt->Clip;
        Tgt->Clip = New;
        x_(Tgt,SetClip);
        x_(that->c,Display,Tgt);
        Tgt->Clip = Old;
        x_(Tgt,SetClip);
    }
}
TlsGuiOut *TlsGuiOutClip(TlsI2dRange *Clip,TlsGuiOut *Child) {
     OutClip *r;
     static struct TlsGuiOut Static = {OutClipDisplay};
     rPush(r);
     r->TlsGuiOut.Static = &Static;
     r->Clip = *Clip;
     r->c = Child;
     return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dPoint *pos;
    TlsGuiOut *c;
} OutMobPos;

static void OutMobPosDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsI2dPoint Old;
    TlsGetThat(OutMobPos,TlsGuiOut);
    Old = Tgt->CursorPos;
    Tgt->CursorPos.x[0] += that->pos->x[0];
    Tgt->CursorPos.x[1] += that->pos->x[1];
    x_(that->c,Display,Tgt);
    Tgt->CursorPos = Old;
}

TlsGuiOut *TlsGuiOutMobPos(TlsI2dPoint *Pos,TlsGuiOut *Child) {
     OutMobPos *r;
     static struct TlsGuiOut Static = {OutMobPosDisplay};
     rPush(r);
     r->TlsGuiOut.Static = &Static;
     r->pos = Pos;
     r->c = Child;
     return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void OutMobGrabPointDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsI2dPoint Old;
    TlsGetThat(OutMobPos,TlsGuiOut);
    Old = Tgt->CursorPos;
    Tgt->CursorPos.x[0] -= that->pos->x[0];
    Tgt->CursorPos.x[1] -= that->pos->x[1];
    x_(that->c,Display,Tgt);
    Tgt->CursorPos = Old;
}

TlsGuiOut *TlsGuiOutMobGrabPoint(TlsI2dPoint *Pos,TlsGuiOut *Child) {
    OutMobPos *r;
    static struct TlsGuiOut Static = {OutMobGrabPointDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->pos = Pos;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsRGBA *clr;
    TlsGuiOut *c;
} OutMobClr;

static void OutMobFrontClrDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsRGBA Old;
    TlsGetThat(OutMobClr,TlsGuiOut);
    Old = Tgt->Front;
    Tgt->Front = *that->clr;
    x_(Tgt,SetFront);
    x_(that->c,Display,Tgt);
    Tgt->Front = Old;
    x_(Tgt,SetFront);
}

TlsGuiOut *TlsGuiOutMobFrontClr(TlsRGBA *Color,TlsGuiOut *Child) {
    OutMobClr *r;
    static struct TlsGuiOut Static = {OutMobFrontClrDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->clr = Color;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void OutMobBackClrDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsRGBA Old;
    TlsGetThat(OutMobClr,TlsGuiOut);
    Old = Tgt->Back;
    Tgt->Back = *that->clr;
    x_(Tgt,SetBack);
    x_(that->c,Display,Tgt);
    Tgt->Back = Old;
    x_(Tgt,SetBack);
}
TlsGuiOut *TlsGuiOutMobBackClr(TlsRGBA *Color,TlsGuiOut *Child) {
    OutMobClr *r;
    static struct TlsGuiOut Static =  {OutMobBackClrDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->clr = Color;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dRange *clip;
    TlsGuiOut *c;
} OutMobClip;
static void OutMobClipDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsI2dRange Old,New;
    TlsGetThat(OutMobClip,TlsGuiOut);
    New.b.x[0] = Tgt->CursorPos.x[0]+that->clip->b.x[0];
    New.b.x[1] = Tgt->CursorPos.x[1]+that->clip->b.x[1];
    New.e.x[0] = Tgt->CursorPos.x[0]+that->clip->e.x[0];
    New.e.x[1] = Tgt->CursorPos.x[1]+that->clip->e.x[1];
    Old = Tgt->Clip;
    if (New.b.x[0]<Old.b.x[0]) New.b.x[0]=Old.b.x[0];
    if (New.b.x[1]<Old.b.x[1]) New.b.x[1]=Old.b.x[1];
    if (New.e.x[0]>Old.e.x[0]) New.e.x[0]=Old.e.x[0];
    if (New.e.x[1]>Old.e.x[1]) New.e.x[1]=Old.e.x[1];
    if ((New.e.x[0]>New.b.x[0])&&(New.e.x[1]>New.b.x[1])) {
        Tgt->Clip = New;
        x_(Tgt,SetClip);
        x_(that->c,Display,Tgt);
        Tgt->Clip = Old;
        x_(Tgt,SetClip);
    }
}
TlsGuiOut *TlsGuiOutMobClip(TlsI2dRange *Clip,TlsGuiOut *Child) {
    OutMobClip *r;
    static struct TlsGuiOut Static = {OutMobClipDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->clip = Clip;
    r->c = Child;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void PointDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    x_(Tgt,Point);
}
TlsGuiOut *TlsGuiPoint(void) {
    static struct TlsGuiOut Static = {PointDisplay};
    static TlsGuiOut r = {&Static};
    return &r;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dPoint A;
} OutLine;
static void LineDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGetThat(OutLine,TlsGuiOut);
    x_(Tgt,Line,that->A.x[0],that->A.x[1]);
}
TlsGuiOut *TlsGuiLine(int x1,int y1) {
    OutLine *r;
    static struct TlsGuiOut Static = {LineDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->A.x[0]=x1; r->A.x[1]=y1;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void RectangleDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGetThat(OutLine,TlsGuiOut);
    x_(Tgt,Rectangle,that->A.x[0],that->A.x[1]);
}
TlsGuiOut *TlsGuiRectangle(int x1,int y1) {
    OutLine *r;
    static struct TlsGuiOut Static = {RectangleDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->A.x[0] = x1;
    r->A.x[1] = y1;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void FilledRectangleDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGetThat(OutLine,TlsGuiOut);
    x_(Tgt,FillRectangle,that->A.x[0],that->A.x[1]);
}
TlsGuiOut *TlsGuiFilledRectangle(int x1,int y1){
    OutLine *r;
    static struct TlsGuiOut Static = {FilledRectangleDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->A.x[0] = x1;
    r->A.x[1] = y1;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

static void ClearDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    x_(Tgt,Clear);
}
TlsGuiOut *TlsGuiClear(void) {
    static struct TlsGuiOut Static = {ClearDisplay};
    static TlsGuiOut r = {&Static};
    return &r;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsGuiMappedTexture *Texture;
    TlsI2dRange *Area;
} TxtrArea;
static void TxtAreaDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGetThat(TxtrArea,TlsGuiOut);
    x_(that->Texture,Draw,that->Area);
}
TlsGuiOut *TlsGuiMappedPic(TlsGuiMappedTexture *txt,TlsI2dRange *Area) {
    TxtrArea *r;
    static struct TlsGuiOut Static = {TxtAreaDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Texture = txt;
    r->Area = Area;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 | 
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsGuiMappedTexture *Txtr;
    TlsI2dRange *Area;
    TlsI2dPoint *TgtSize;
} ScaledTxtrArea;
static void ScaledTxtrAreaDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    TlsGetThat(ScaledTxtrArea,TlsGuiOut);
    x_(that->Txtr,ScaleDraw,that->Area,that->TgtSize);
}
TlsGuiOut *TlsGuiScaledPic(TlsGuiMappedTexture *Txtr,TlsI2dRange *Area,TlsI2dPoint *TgtSize) {
    ScaledTxtrArea *r;
    static struct TlsGuiOut Static = {ScaledTxtrAreaDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Area = Area;
    r->Txtr = Txtr;
    r->TgtSize = TgtSize;
    return &r->TlsGuiOut;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsGuiMappedFont *Font;
    char *Txt;
} DisplayedString;

static void DisplayedStringDisplay(TlsGuiOut *this,TlsGuiRenderer *Tgt) {
    char *p;
    int xorg,x,dx;
    TlsI2dRange *Glyphs;
    TlsGetThat(DisplayedString,TlsGuiOut);
    TlsGuiMappedTexture *Map=that->Font->Map;
    p = that->Txt;
    x = 0; dx = that->Font->Font->dx;
    xorg = Tgt->CursorPos.x[0];
    Glyphs = that->Font->Font->Glyphs;
    while (*p) {
        Tgt->CursorPos.x[0] = xorg+x;
        x_(Map,Draw,Glyphs+(*p));
        p++; x+=dx;
    }
    Tgt->CursorPos.x[0] = xorg;
}

TlsGuiOut *TlsGuiDisplayedString(TlsGuiMappedFont *Fnt,char *txt) {
    DisplayedString *r;
    static struct TlsGuiOut Static = {DisplayedStringDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Font = Fnt;
    r->Txt = txt;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dPoint *Scale;
    TlsI2dPoint *Pos;
    TlsGuiOut *Child;
} GuiOutScaledPos;
static void ScaledPosDisplay(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(GuiOutScaledPos,TlsGuiOut);
    TlsI2dPoint oPos = dst->CursorPos;
    dst->CursorPos.x[0]+=that->Scale->x[0]*that->Pos->x[0];
    dst->CursorPos.x[1]+=that->Scale->x[1]*that->Pos->x[1];
    x_(that->Child,Display,dst);
    dst->CursorPos=oPos;
}
TlsGuiOut *TlsGuiOutMobScaledPos(TlsGuiOut *Child,TlsI2dPoint *Scale,TlsI2dPoint *Pos) {
    GuiOutScaledPos *r;
    static struct TlsGuiOut Static = {ScaledPosDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Child = Child; r->Scale = Scale; r->Pos = Pos;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/
typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dPoint *Xtnt;
} ResizableRect;
static void ResizableRectDisplay(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(ResizableRect,TlsGuiOut);
    TlsI2dPoint *Xtnt=that->Xtnt;
    x_(dst,Rectangle,Xtnt->x[0],Xtnt->x[1]);
}
TlsGuiOut *TlsGuiOutMobRect(TlsI2dPoint *Xtnt) {
    ResizableRect *r;
    static struct TlsGuiOut Static = {ResizableRectDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Xtnt = Xtnt;
    return &r->TlsGuiOut;
}
static void ResizableFilledRectDisplay(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(ResizableRect,TlsGuiOut);
    TlsI2dPoint *Xtnt=that->Xtnt;
    x_(dst,FillRectangle,Xtnt->x[0],Xtnt->x[1]);
}
TlsGuiOut *TlsGuiOutMobFilledRect(TlsI2dPoint *Xtnt) {
    ResizableRect *r;
    static struct TlsGuiOut Static = {ResizableFilledRectDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Xtnt = Xtnt;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

TlsI4dMat *TlsI4dMat24Mult(TlsI4dMat *r,TlsI4dMat *A0,TlsI4dMat *B) {
      long long a[3],b[3],v;
      int i;
      TlsI4dMat A=*A0;
      r->x[3][0]=r->x[3][1]=r->x[3][2]=0; r->x[3][3]=1;
      b[0]=B->x[0][3]; b[1]=B->x[1][3]; b[2]=B->x[2][3];
      a[0]=A.x[0][0]; a[1]=A.x[0][1]; a[2]=A.x[0][2];
      v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[0][3]=A.x[0][3]+v;
      a[0]=A.x[1][0]; a[1]=A.x[1][1]; a[2]=A.x[1][2];
      v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[1][3]=A.x[1][3]+v;
      a[0]=A.x[2][0]; a[1]=A.x[2][1]; a[2]=A.x[2][2];
      v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[2][3]=A.x[2][3]+v;
      for (i=0;i<3;i++) {
          b[0]=B->x[0][i]; b[1]=B->x[1][i]; b[2]=B->x[2][i];
          a[0]=A.x[0][0]; a[1]=A.x[0][1]; a[2]=A.x[0][2];
          v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[0][i] =v;
          a[0]=A.x[1][0]; a[1]=A.x[1][1]; a[2]=A.x[1][2];
          v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[1][i] =v;
          a[0]=A.x[2][0]; a[1]=A.x[2][1]; a[2]=A.x[2][2];
          v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[2][i] =v;
      }
    return r;
}
/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI4dMat *Pos;
    TlsGuiOut *Child;
} MobVerticesPos;
static void MobVerticesPosDisplay(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(MobVerticesPos,TlsGuiOut);
    TlsI4dMat oPos=dst->Global;
    { long long a[3],b[3],v;
      int i; 
      TlsI3dMat nv;
      TlsI4dMat *Pos=that->Pos;
      b[0]=Pos->x[0][3]; b[1]=Pos->x[1][3]; b[2]=Pos->x[2][3];
      a[0]=dst->Global.x[0][0]; a[1]=dst->Global.x[0][1]; a[2]=dst->Global.x[0][2];
      v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->Global.x[0][3]+=v;
      a[0]=dst->Global.x[1][0]; a[1]=dst->Global.x[1][1]; a[2]=dst->Global.x[1][2];
      v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->Global.x[1][3]+=v;
      a[0]=dst->Global.x[2][0]; a[1]=dst->Global.x[2][1]; a[2]=dst->Global.x[2][2];
      v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->Global.x[2][3]+=v;
      for (i=0;i<3;i++) {
          b[0]=Pos->x[0][i]; b[1]=Pos->x[1][i]; b[2]=Pos->x[2][i];
          a[0]=dst->Global.x[0][0]; a[1]=dst->Global.x[0][1]; a[2]=dst->Global.x[0][2];
          v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; nv.x[0][i] =v;
          a[0]=dst->Global.x[1][0]; a[1]=dst->Global.x[1][1]; a[2]=dst->Global.x[1][2];
          v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; nv.x[1][i] =v;
          a[0]=dst->Global.x[2][0]; a[1]=dst->Global.x[2][1]; a[2]=dst->Global.x[2][2];
          v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; nv.x[2][i] =v;
      }
      dst->Global.x[0][0]=nv.x[0][0]; dst->Global.x[0][1]=nv.x[0][1]; dst->Global.x[0][2]=nv.x[0][2];
      dst->Global.x[1][0]=nv.x[1][0]; dst->Global.x[1][1]=nv.x[1][1]; dst->Global.x[1][2]=nv.x[1][2];
      dst->Global.x[2][0]=nv.x[2][0]; dst->Global.x[2][1]=nv.x[2][1]; dst->Global.x[2][2]=nv.x[2][2];
    }
    x_(that->Child,Display,dst);
    dst->Global=oPos;
}
TlsGuiOut *TlsGuiOutMobVerticesPos(TlsI4dMat *Pos,TlsGuiOut *Child) {
    MobVerticesPos *r;
    static struct TlsGuiOut Static={MobVerticesPosDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Pos=Pos; r->Child=Child;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/
typedef struct {
    TlsGuiOut TlsGuiOut;
    float *vScale;
    TlsGuiOut *Child;
} vScaleSet;
static void vScaleSetDraw(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(vScaleSet,TlsGuiOut);
    float oScale = dst->vScale;
    dst->vScale = *that->vScale;
    x_(that->Child,Display,dst);
    dst->vScale = oScale;
}
TlsGuiOut *TlsGuiOutMobSetVScale(float *vScale,TlsGuiOut *Child) {
    vScaleSet *r;
    static struct TlsGuiOut Static={vScaleSetDraw};
    rPush(r);
    r->TlsGuiOut.Static=&Static;
    r->vScale=vScale;
    r->Child = Child;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsGuiMappedTexture *Texture;
    TlsIGeoMesh *Mesh;
} OutVertices;

static void VerticesDraw(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(OutVertices,TlsGuiOut);
    rOpen {
        TlsGuiScreenVtx *screened;
        Tlsf2dPoint *projected;
        TlsI2dPoint ScrOrg=dst->CursorPos;
        TlsI4dMat Global=dst->Global;
        TlsGuiScreenMesh ScreenMesh;
        rnPush(projected,that->Mesh->AnchorNb);
        rnPush(ScreenMesh.Vtx,that->Mesh->VertexNb);
        rnPush(ScreenMesh.Idx,that->Mesh->IdxNb);
        ScreenMesh.IdxNb=0; ScreenMesh.VtxNb=that->Mesh->VertexNb;
        /* Project points */ {
        TlsI3dPoint *p = that->Mesh->Anchors; 
        TlsI3dPoint *e=p+that->Mesh->AnchorNb;
        Tlsf2dPoint *q = projected;
        while (p!=e) {
            long long acc[2], a[3],b[3];
            float c;
            b[0]=p->x[0]; b[1]=p->x[1]; b[2]=p->x[2];
            a[0]=Global.x[0][0]; a[1]=Global.x[0][1]; a[2]=Global.x[0][2];
            acc[0] = ((a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24)+Global.x[0][3];
            a[0]=Global.x[1][0]; a[1]=Global.x[1][1]; a[2]=Global.x[1][2];
            acc[1] = ((a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24)+Global.x[1][3];
            c=acc[0]; c=(c*dst->vScale)+ScrOrg.x[0]; q->x[0]=c;
            c=acc[1]; c=(c*dst->vScale)+ScrOrg.x[1]; q->x[1]=c;
            p++; q++;
        } }
      
        /* Fill Vertices */ {
            int j=ScreenMesh.VtxNb;
            struct TlsGeoMeshIdx *anc=that->Mesh->Vertices;
            TlsGuiScreenVtx *sq=ScreenMesh.Vtx;
            while (j) {
                float f;
                sq->Point=projected[anc->Anchor];
                f=anc->uv.uv[0]; f=f/65535.; sq->uv.uv[0]=f;
                f=anc->uv.uv[1]; f=f/65535.; sq->uv.uv[1]=f;
                sq->Color=anc->Color;
                sq++; j--; anc++;
            }
        }
        /* record front facing faces */ {
            int iNb=that->Mesh->IdxNb-2;
            int *q=ScreenMesh.Idx;
            int *p=that->Mesh->Idx;
            while (iNb>0) {
            float a[2],b[2],z;
            Tlsf2dPoint *anc[3];
            anc[0]=&ScreenMesh.Vtx[p[0]].Point;
            anc[1]=&ScreenMesh.Vtx[p[1]].Point;
            anc[2]=&ScreenMesh.Vtx[p[2]].Point;
            a[0]=anc[1]->x[0]-anc[0]->x[0]; 
            a[1]=anc[1]->x[1]-anc[0]->x[1]; 
            b[0]=anc[2]->x[0]-anc[0]->x[0]; 
            b[1]=anc[2]->x[1]-anc[0]->x[1];
            z=a[0]*b[1]-a[1]*b[0];
            if (z<0) {
                q[0]=p[0]; q[1]=p[1]; q[2]=p[2]; q+=3;
            }
            p+=3; iNb-=3;
            }
            ScreenMesh.IdxNb=q-ScreenMesh.Idx;
        }
  
        if (that->Texture) {
            x_(that->Texture,OrthoTriangles,&ScreenMesh);
        } else {
            x_(dst,OrthoTriangles,&ScreenMesh);
        }
    } rClose
}

TlsGuiOut *TlsGuiOutOrthVertices(TlsGuiMappedTexture *Txt,TlsIGeoMesh *Mesh) {
    OutVertices *r;
    static struct TlsGuiOut Static = {VerticesDraw};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Texture=Txt;
    r->Mesh = Mesh;
    return &r->TlsGuiOut;
}

