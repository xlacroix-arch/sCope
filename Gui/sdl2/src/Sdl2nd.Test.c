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
#include <TlsGui.Local.h>

#include <stdio.h>

/*____________________________________________________
 |
 | 
 |____________________________________________________
*/

typedef struct {
    int *Min,*Max,*x,*dx;
    TlsGuiInTrigger Lower,Raise;
} TlsGui1dTextCursor;
static int Text1dCursorLowerTrigger(TlsGuiInTrigger *this,unsigned int date) {
    int x;
    TlsGetThat(TlsGui1dTextCursor,Lower);
    x = *that->x-*that->dx;
    if (x<*that->Min) x = *that->Min;
    *that->x = x;
    return (0==0);
}
static int Text1dCursorRaiseTrigger(TlsGuiInTrigger *this,unsigned int date) {
    int x;
    TlsGetThat(TlsGui1dTextCursor,Raise);
    x = *that->x+*that->dx;
    if (x>*that->Max) x = *that->Max;
    *that->x = x;
    return (0==0);
}

TlsGui1dTextCursor *TlsGui1dTextCursorInit(TlsGui1dTextCursor *r,int *Min,int *Max,int *x,int *dx) {
    static struct TlsGuiInTrigger Lower = {Text1dCursorLowerTrigger};
    static struct TlsGuiInTrigger Raise = {Text1dCursorRaiseTrigger};
    r->Lower.Static = &Lower;
    r->Raise.Static = &Raise;
    r->Min = Min; r->Max = Max;
    r->x = x; r->dx = dx;
    return r;
}

typedef struct {
    TlsGui1dTextCursor x,y;
} TlsGui2dTextCursor;

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    int w,h,pitch;
    unsigned char *dst;
    int SetPushed,ResetPushed;
    int x,y;
    char fg,bg;
    struct {
        TlsGuiInTrigger Up,Down,Left,Right,Fg,Bg;
    } Pushed,Released;
} Tls2dCharEdit;
static int CharEditUpPushed(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(Tls2dCharEdit,Pushed.Up);
    if (that->y>0) {that->y-=1;}
    return (0==0);
}
static int CharEditDownPushed(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(Tls2dCharEdit,Pushed.Down);
    if (that->y<(that->h-1)) { that->y+=1; }
    return (0==0);
}
static int CharEditLeftPushed(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(Tls2dCharEdit,Pushed.Left);
    if (that->x>0) {that->x-=1;}
    return (0==0);
}
static int CharEditRightPushed(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(Tls2dCharEdit,Pushed.Right);
    if (that->x<(that->w-1)) { that->x+=1; }
    return (0==0);
}
static int CharEditFgPushed(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(Tls2dCharEdit,Pushed.Fg);
    that->dst[that->pitch*that->y+that->x] = that->fg;
    return (0==0);
}



/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    TlsGuiRenderer *Tgt;
    TlsGuiOut *Scene;
    TlsGuiInKey KeyDown;
    TlsGuiInTrigger Quit;
    unsigned int CurrentTime,TimeOrg;
    struct {
        unsigned int TimeOrg;
        TlsI2dPoint Pos;
    } SelectStart,CursorX;
    TlsI2dPoint MobsPos[16];
    struct {
        int Width,Height,Pitch;
        struct {
            unsigned char Front,Back,Glyph,Depth;
        } *Map;
    } GlyphMap;
    struct {
        unsigned char Front[8],Back[8],Glyph[8];
    } GlyphSelect;
    int Finished;
} MapEditor;

typedef struct {
    struct {
    } EditorWindow;
} ScreenComp;

static int DrawScreen(MapEditor *obj) {
    obj->MobsPos[0].x[0] = 20 + 8*obj->CursorX.Pos.x[0];
    obj->MobsPos[0].x[1] = 20 + 8*obj->CursorX.Pos.x[1];
    obj->MobsPos[1].x[0] = (obj->CurrentTime/10)%600;
    obj->MobsPos[1].x[1] = 0; 
    x_(obj->Scene,Display,obj->Tgt);
    x_(obj->Tgt,Display);
}

static int MapEdQuitTrigger(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(MapEditor,Quit);
    that->Finished = (0==0);
    return (0==0);
} 

static int KeyboardDown(TlsGuiInKey *this,unsigned int date,int c,int Key) {
    int processed;
    TlsGetThat(MapEditor,KeyDown);
    processed = (0!=0);
    that->Finished = (that->Finished)||(c==c_(Gui,ScanCode,F1))||(c==c_(Gui,ScanCode,Escape));
    processed = that->Finished;
    switch (c) {
        case c_(Gui,ScanCode,Up): {
            processed = (0==0);
            int y; y = that->CursorX.Pos.x[1]-1;
            if (y<0) y=0;
            that->CursorX.Pos.x[1] = y;
        } break;
        case c_(Gui,ScanCode,Down): {
            processed = (0==0);
            int y; y = that->CursorX.Pos.x[1]+1;
            if (y>=that->GlyphMap.Height) y = that->GlyphMap.Height-1;
            that->CursorX.Pos.x[1] = y;
        } break;
        case c_(Gui,ScanCode,Left): {
            processed = (0==0);
            int x; x = that->CursorX.Pos.x[0]-1;
            if (x<0) x=0;
            that->CursorX.Pos.x[0] = x;
        } break;
        case c_(Gui,ScanCode,Right): {
            processed = (0==0);
            int x; x = that->CursorX.Pos.x[0]+1;
            if (x>=that->GlyphMap.Width) x = that->GlyphMap.Width-1;
            that->CursorX.Pos.x[0] = x;
        } break;
    }
    return processed;
}

TlsGuiSimpleFont *TlsGuiNewAsciiFont8x8(void);

int SDL2_Main_Go(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll) {
    MapEditor obj;
    TlsGuiInputDevices *Inputs;
    TlsGuiOut *Msg;
    struct {
        TlsGuiMappedFont Font;
        TlsGuiMappedPalette *Bitmap1Palette;
        TlsI2dRange SheetDim;
        TlsGuiOut *AllSheetImg;
    } Font;
    static struct TlsGuiInKey KeyboardDownStatic = {&KeyboardDown};
    static struct TlsGuiInTrigger MapEdQuitStatic = {&MapEdQuitTrigger};
    printf("SDL2 Up and Running. F1 or Esc to quit.\n");
    Font.Bitmap1Palette=x_(Tgt,MapPalette,TlsGuiTxtrFormatBitmap(),TlsGuiPaletteBitmap(1,(0==0)));
    Font.Font.Font = TlsGuiNewAsciiFont8x8();
    Font.Font.Map = x_(Tgt,MapTexture,(0!=0),Font.Bitmap1Palette,Font.Font.Font->Sheet);
    Font.SheetDim.b.x[0] = Font.SheetDim.b.x[1]=0;
    Font.SheetDim.e.x[0] = Font.Font.Font->Sheet->w; Font.SheetDim.e.x[1] = Font.Font.Font->Sheet->h;
    Font.AllSheetImg = TlsGuiMappedPic(Font.Font.Map,&Font.SheetDim);
    Msg = TlsGuiDisplayedString(&Font.Font,"Hello World!");
    obj.Finished = (0!=0);
    obj.KeyDown.Static = &KeyboardDownStatic;
    obj.Quit.Static = &MapEdQuitStatic;
    Inputs = x_(Poll,CallBacks);
    Inputs->Keyboard.Down = &obj.KeyDown;
    Inputs->Keyboard.Repeat = &obj.KeyDown;
    Inputs->Window.Close = &obj.Quit;
    obj.CursorX.Pos.x[0] = 0;
    obj.CursorX.Pos.x[1] = 0;
    obj.GlyphMap.Width = 32;
    obj.GlyphMap.Height = 32;
    {
        TlsGuiOut *s0,*s1,*s2;
        obj.Tgt = Tgt;
        s2 = TlsGuiOutFrontClr(127,127,0,255,TlsGuiOutPos(50,50,TlsGuiOutList(TlsGuiLine(540,0),TlsGuiLine(0,380),TlsGuiOutPos(540,380,TlsGuiOutPair(TlsGuiLine(-540,0),TlsGuiLine(0,-380))),0)));
        s1 = TlsGuiOutList(s2,
            TlsGuiOutFrontClr(0,255,0,255,TlsGuiOutPos(100,100,TlsGuiOutList(TlsGuiRectangle(440,280),Msg,0))),
            TlsGuiOutFrontClr(0,0,255,255,TlsGuiOutPos(150,150,TlsGuiFilledRectangle(340,180))),
            TlsGuiOutFrontClr(0,255,0,255,TlsGuiOutPos(0,0,Font.AllSheetImg)),
        0);
        s0 = TlsGuiOutList(s1,
            TlsGuiOutFrontClr(255,255,0,255,TlsGuiOutMobPos(obj.MobsPos+0,TlsGuiFilledRectangle(8,8))),
            TlsGuiOutFrontClr(255,255,0,255,TlsGuiOutMobPos(obj.MobsPos+1,TlsGuiFilledRectangle(8,8))),
        0);
        obj.Scene = TlsGuiOutBackClr(32,32,32,255,
            TlsGuiOutFrontClr(255,255,255,255,TlsGuiOutPair(TlsGuiClear(),s0)));
    }
    obj.TimeOrg = x_(Poll,msTime);

    do {
        obj.CurrentTime = x_(Poll,msTime);
        DrawScreen(&obj);
        while ((x_(Poll,Process))&&(!obj.Finished)) {}
    } while(!obj.Finished);
    x_(Font.Font.Map,Release);
    x_(Font.Bitmap1Palette,Release);
}

int TlsMain() {
    SDL2_Main(640,480,SDL2_Main_Go);
    return 0;
}

