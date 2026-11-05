/*  MIT License
 *
 *  Copyright (c) 2025 mtxavier
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

#include <TlsGui.Ext.h>

#define TlsMin(a,b) ((a)<=(b))?(a):(b)
#define TlsMax(a,b) ((a)>=(b))?(a):(b)

/*---------------------------------------------*/

TlsGuiSimpleFont *TlsGuiNewAsciiFont8x8(void);

typedef struct {
    TlsGuiMappedFont Font;
    TlsGuiMappedPalette *Bitmap1Palette;
    TlsI2dRange SheetDim;
    TlsGuiOut *AllSheetImg;
} FontParam;

static void FontParamInit(FontParam *Font,TlsGuiRenderer *Tgt) {
    Font->Bitmap1Palette = x_(Tgt,MapPalette,TlsGuiTxtrFormatBitmap(),TlsGuiPaletteBitmap(1,(0==0)));
    Font->Font.Font = TlsGuiNewAsciiFont8x8();
    Font->Font.Map = x_(Tgt,MapTexture,(0!=0),Font->Bitmap1Palette,Font->Font.Font->Sheet);
    Font->SheetDim.b.x[0] = Font->SheetDim.b.x[1]=0;
    Font->SheetDim.e.x[0] = Font->Font.Font->Sheet->w; Font->SheetDim.e.x[1] = Font->Font.Font->Sheet->h;
    Font->AllSheetImg = TlsGuiMappedPic(Font->Font.Map,&Font->SheetDim);
}

static void FontParamRelease(FontParam *Font) {
    x_(Font->Font.Map,Release);
    x_(Font->Bitmap1Palette,Release);
}

/*____________________________________________________
 |
 |____________________________________________________
*/

typedef struct {
    int offset,period,filled;
} TlsGuiStippledPattern;

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsI2dPoint *dv,*extent;
    TlsGuiStippledPattern *Ptrn;
} OutAlignmentGrid;
static void AlignmentGridDisplay(TlsGuiOut *this,TlsGuiRenderer *State) {
    TlsGetThat(OutAlignmentGrid,TlsGuiOut);
    { TlsI2dPoint oCPos;
    int x=0, y=0, w=that->extent->x[0], h=that->extent->x[1], dx=that->dv->x[0], dy=that->dv->x[1];
    int period=that->Ptrn->period,offset=that->Ptrn->offset,filled=that->Ptrn->filled;
    oCPos=State->CursorPos;
    while (y<h) {
        int xe;
        if (offset<0) {
            xe = (filled+offset<w)?filled+offset:w;
            x_(State,Line,xe,0); State->CursorPos.x[0]+=period;
        }
        State->CursorPos.x[0]+=offset; xe=State->CursorPos.x[0]+filled;
        while (xe<w) { x_(State,Line,filled,0); State->CursorPos.x[0]+=period; xe+=period; }
        if (State->CursorPos.x[0]<w) { x_(State,Line,w-State->CursorPos.x[0],0); }
        State->CursorPos.x[0]=oCPos.x[0];
        State->CursorPos.x[1]+=dy; y+=dy;
    }
    State->CursorPos=oCPos;
    while (x<w) {
        int ye;
        if (offset<0) {
            ye = (filled+offset<h)?filled+offset:h;
            x_(State,Line,0,ye); State->CursorPos.x[1]+=period;
        }
        State->CursorPos.x[1]+=offset; ye=State->CursorPos.x[1]+filled;
        while (ye<h) { x_(State,Line,0,filled); State->CursorPos.x[1]+=period; ye+=period;}
        if (State->CursorPos.x[1]<h) { x_(State,Line,0,h-State->CursorPos.x[1]); }
        State->CursorPos.x[0]+=dx; x+=dx;
        State->CursorPos.x[1]=oCPos.x[1];
    }
    State->CursorPos=oCPos;
    }
}

TlsGuiOut *TlsGuiOutAlignmentGrid(TlsI2dPoint *dv,TlsI2dPoint *extent,TlsGuiStippledPattern *Ptrn) {
    static struct TlsGuiOut Static= {AlignmentGridDisplay};
    OutAlignmentGrid *r;
    rPush(r); 
    r->TlsGuiOut.Static = &Static;
    r->dv=dv;
    r->extent=extent;
    r->Ptrn=Ptrn;
    int period=Ptrn->period,filled=Ptrn->filled,offset=Ptrn->offset;
    if (period<=1) { filled=period=256; }
    if (filled<0) { filled=period;}
    if (filled>=period) {filled=period; offset=0;}
    offset = offset%period;
    if (offset+filled>period) {offset-=period; }
    Ptrn->period=period;Ptrn->filled=filled;Ptrn->offset=offset;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

/*----------------------------*/

typedef struct {
    TlsGuiInKey Up,Down;
} TlsGuiInKeyButton;

typedef struct {
    TlsGuiInTrigger **TriggerMap;
    TlsGuiInKeyButton Button;
    struct InDispatch0Translate {int child; int ScanCode; int Key; } *Translate;
    TlsGuiInKeyButton **Children;
    int MaxChildren,ChildNum;
} InDispatch0;

static int InDispatch0KeyboardDown(TlsGuiInKey *this,unsigned int date,int c, int Key) {
    int processed = (0!=0);
    TlsGetThat(InDispatch0,Button.Down);
    int n=that->Translate[c].child;
    if (n>=0) {
        TlsGuiInKey *child=&(that->Children[n]->Down);
        processed = x_(child,Trigger,date,that->Translate[c].ScanCode,that->Translate[c].Key);
    }
    if ((!processed)&&(that->TriggerMap[c]!=0)) { processed=x_(that->TriggerMap[c],Trigger,date);}
    return processed;
}
static int InDispatch0KeyboardUp(TlsGuiInKey *this,unsigned int date,int c, int Key) {
    int processed = (0!=0);
    TlsGetThat(InDispatch0,Button.Up);
    int n=that->Translate[c].child;
    if (n>=0) {
        TlsGuiInKey *child=&(that->Children[n]->Up);
        processed = x_(child,Trigger,date,that->Translate[c].ScanCode,that->Translate[c].Key);
    }
    return processed;
}
static InDispatch0 *InDispatch0New(int MaxChildNb) {
    InDispatch0 *r;
    static struct TlsGuiInKey Up={InDispatch0KeyboardUp},Down={InDispatch0KeyboardDown};
    rPush(r);
    rnPush(r->Translate,c_(Gui,ScanCode,_Nb));
    rnPush(r->TriggerMap,c_(Gui,ScanCode,_Nb));
    r->Button.Up.Static = &Up;
    r->Button.Down.Static = &Down;
    rnPush(r->Children,MaxChildNb);
    r->ChildNum=0; r->MaxChildren=MaxChildNb;
    int i=0;
    while (i<c_(Gui,ScanCode,_Nb)) {
        const struct InDispatch0Translate NoTranslate={-1,0,0};
        r->TriggerMap[i]=0;
        r->Translate[i]=NoTranslate; i++;
    }
    return r;
}
static void InDispatch0AddCursor(InDispatch0 *dst,TlsGuiInKeyButton *ctrl,int left,int down,int up,int right) {
    int n=dst->ChildNum;
    dst->Translate[left].child=n; 
    dst->Translate[left].ScanCode=dst->Translate[left].Key=c_(Gui,ScanCode,Left);
    dst->Translate[down].child=n; 
    dst->Translate[down].ScanCode=dst->Translate[down].Key=c_(Gui,ScanCode,Down);
    dst->Translate[up].child=n; 
    dst->Translate[up].ScanCode=dst->Translate[up].Key=c_(Gui,ScanCode,Up);
    dst->Translate[right].child=n; 
    dst->Translate[right].ScanCode=dst->Translate[right].Key=c_(Gui,ScanCode,Right);
    dst->Children[n]=ctrl;
    dst->ChildNum++;
}
static void InDispatch0AddTrigger(InDispatch0 *dst,TlsGuiInTrigger *ctrl,int key) {
    dst->TriggerMap[key]=ctrl;
}
    

/*____________________________________________________
 |
 |____________________________________________________
*/

static void TransfertFont(TlsGuiMappedFont *dst,TlsGuiMappedFont *src) {
    int i,nb;
    TlsGuiTextureArea dr,sr;
    nb = TlsMax(src->Font->GlyphNb,dst->Font->GlyphNb);
    dr.Texture=dst->Font->Sheet;
    sr.Texture=src->Font->Sheet;
    i=0;
    while (i<nb) {
        TlsI2dVec dim,dim1;
        dr.Pic = dst->Font->Glyphs[i]; sr.Pic = src->Font->Glyphs[i];
        dim.x[0]=dr.Pic.e.x[0]-dr.Pic.b.x[0]; dim.x[1]=dr.Pic.e.x[1]-dr.Pic.b.x[1];
        dim1.x[0]=sr.Pic.e.x[0]-sr.Pic.b.x[0]; dim.x[1]=sr.Pic.e.x[1]-sr.Pic.b.x[1];
        dim.x[0]=TlsMin(dim.x[0],dim1.x[0]); dim.x[1]=TlsMin(dim.x[1],dim1.x[1]);
        dr.Pic.e.x[0]=dr.Pic.b.x[0]+dim.x[0]; dr.Pic.e.x[1]=dr.Pic.b.x[1]+dim.x[1];
        sr.Pic.e.x[0]=sr.Pic.b.x[0]+dim.x[0]; sr.Pic.e.x[1]=sr.Pic.b.x[1]+dim.x[1];
        TlsGuiTextureTransfert(&dr,&sr);
        i++;
    }
}

typedef struct {
    TlsGuiSimpleFont Font;
    TlsGuiMappedFont New;
} GlyphSheetEditDst;

static GlyphSheetEditDst *NewGlyphSheet(TlsGuiRenderer *dst,TlsGuiMappedPalette *colors,TlsI2dVec *Dim) {
    GlyphSheetEditDst *r;
    int sz;
    rPush(r);
    r->New.Font = &r->Font;
    r->Font.dx=Dim->x[0]; r->Font.dy=Dim->x[1];
    r->Font.GlyphNb = 256;
    rnPush(r->Font.Glyphs,256);
    r->Font.Sheet = TlsGuiTextureNew(colors->Format,colors->Colors,Dim->x[0],Dim->x[1]);
    int i=0;
    while (i<256) {
        TlsI2dRange ext;
        ext.b.x[0]=(i&0xf)*Dim->x[0]; ext.b.x[1]=(i>>4)*Dim->x[1];
        ext.e.x[0]=ext.b.x[0]+Dim->x[0]; ext.e.x[1]=ext.b.x[1]+Dim->x[1];
        i++;
    }
    r->New.Map = x_(dst,MapTexture,(0==0),colors,r->Font.Sheet);
    return r;
}

/*-----------------------------------*/

typedef struct {
    TlsI2dVec Range;
    TlsI2dPoint Pos;
    TlsGuiInKeyButton Button;
} TlsGuiIn2dCursor;

typedef struct {
    struct {
        TlsGuiOut TlsGuiOut;
        TlsI2dVec GlyphExtent;
        struct {
            TlsGuiOut TlsGuiOut;
            TlsI2dVec Magnification;
            TlsI2dRange TextureArea;
            TlsI2dVec ScreenAreaSize;
            TlsI2dRange Cursor;
        } Image;
        struct {
            TlsI2dRange SheetDim;
            TlsI2dRange Cursor;
        } GlyphSelect;
        struct {
            TlsGuiOut TlsGuiOut;
            TlsRGBA Color[2],*Palette;
        } ColorSelect;
        TlsGuiOut *Composition;
    } Out;
    GlyphSheetEditDst *Tgt;
    int bpp;
    int Color[2];
    struct {
        TlsGuiIn2dCursor Pixel,Glyph,Palette;
        TlsGuiInTrigger colorSet;
        TlsGuiInKeyButton *Dispatch;
    } In;
} SingleGlyphEdit;


static void ColorSelectShowDisplay(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(SingleGlyphEdit,Out.ColorSelect.TlsGuiOut);
    int dx,dy,i,j;
    TlsRGBA oCol,nCol={0,0,0,0xff},*p;
    TlsI2dPoint oPos;
    dx=dy=that->bpp>>1;
    oCol = dst->Front;
    oPos = dst->CursorPos;
    dst->Front=nCol;
    x_(dst,FillRectangle,(6*dx)+1,(6*dy)+1);
    dst->CursorPos.x[1]++;
    p=that->Out.ColorSelect.Palette;
    for (j=0;j<dy;j++) {
        dst->CursorPos.x[0]=oPos.x[0]+1;
        for (i=0;i<dx;i++) {
            dst->Front=*p++;
            x_(dst,FillRectangle,5,5);
            dst->CursorPos.x[0]+=6;
        }
        dst->CursorPos.x[1]+=6;
    }
    dst->CursorPos = oPos;
    dst->Front = oCol;
}

static void GESelectedGlyphUpdate(SingleGlyphEdit *Edit) {
    TlsI2dRange *Txt=&Edit->Out.Image.TextureArea;
    TlsI2dPoint *ScrnSz=&Edit->Out.Image.ScreenAreaSize;
    TlsI2dPoint *Scale=&Edit->Out.Image.Magnification;
    TlsI2dRange *Cursor=&Edit->Out.Image.Cursor;
    TlsI2dPoint *GlyphDim=&Edit->Out.GlyphExtent;
    TlsI2dPoint *GlyphSelect=&Edit->In.Glyph.Pos;
    TlsI2dPoint *PixelSelect=&Edit->In.Pixel.Pos;
    Txt->b.x[0]=GlyphSelect->x[0]*GlyphDim->x[0];
    Txt->b.x[1]=GlyphSelect->x[1]*GlyphDim->x[1];
    Txt->e.x[0]=Txt->b.x[0]+GlyphDim->x[0];
    Txt->e.x[1]=Txt->b.x[1]+GlyphDim->x[1];
    ScrnSz->x[0]=GlyphDim->x[0]*Scale->x[0];
    ScrnSz->x[1]=GlyphDim->x[1]*Scale->x[1];
    Cursor->b.x[0]=PixelSelect->x[0]*Scale->x[0];
    Cursor->b.x[1]=PixelSelect->x[1]*Scale->x[1];
    Cursor->e.x[0]=Cursor->b.x[0]+Scale->x[0];
    Cursor->e.x[1]=Cursor->e.x[1]+Scale->x[1];
    Edit->Color[1]=(Edit->In.Palette.Pos.x[1]<<(Edit->bpp>>1))+Edit->In.Palette.Pos.x[0];
    Edit->Out.GlyphSelect.Cursor=Edit->Out.Image.TextureArea;
}
static void SingleGlyphImageDisplay(TlsGuiOut *this,TlsGuiRenderer *Dst) {
    TlsGetThat(SingleGlyphEdit,Out.Image.TlsGuiOut);
    GESelectedGlyphUpdate(that);
    x_(that->Out.Composition,Display,Dst);
}
static TlsGuiOut *GlyphEditImageArea(SingleGlyphEdit *Edit) {
    TlsGuiOut *r,*GlyphArea,*SheetArea,*PaletteArea;
    GESelectedGlyphUpdate(Edit);
    GlyphArea = TlsGuiOutList(
        TlsGuiScaledPic(Edit->Tgt->New.Map,&Edit->Out.Image.TextureArea,&Edit->Out.Image.ScreenAreaSize)
       ,TlsGuiOutMobPos(&Edit->Out.Image.Cursor.b,TlsGuiOutMobRect(&Edit->Out.Image.Magnification))
    ,0);
    SheetArea = TlsGuiOutList(
        TlsGuiOutFrontClr(144,144,144,255,
            TlsGuiMappedPic(Edit->Tgt->New.Map,&Edit->Out.GlyphSelect.SheetDim))
        ,TlsGuiOutFrontClr(255,255,255,255,
            TlsGuiOutMobPos(&Edit->Out.GlyphSelect.Cursor.b,TlsGuiMappedPic(Edit->Tgt->New.Map,&Edit->Out.GlyphSelect.Cursor)))
    ,0);
    Edit->Out.Composition=TlsGuiOutList(
       TlsGuiOutPos(4,4,GlyphArea),
       TlsGuiOutPos(636-Edit->Out.GlyphSelect.SheetDim.e.x[0],4,SheetArea),
    0);
    return r;
}

static int IgnoreKeyboardUp(TlsGuiInKey *this,unsigned int date,int c,int Key) { return (0!=0); }
static int In2dCursorKeyboardDown(TlsGuiInKey *this,unsigned int date,int c,int Key) {
    int processed;
    TlsGetThat(TlsGuiIn2dCursor,Button.Down);
    processed = (0!=0);
    switch (c) {
        case c_(Gui,ScanCode,Up): {
            processed = (0==0);
            int y; y = that->Pos.x[1]-1;
            if (y<0) y=0;
            that->Pos.x[1] = y;
        } break;
        case c_(Gui,ScanCode,Down): {
            processed = (0==0);
            int y; y = that->Pos.x[1]+1;
            if (y>=that->Range.x[1]) y = that->Range.x[1]-1;
            that->Pos.x[1] = y;
        } break;
        case c_(Gui,ScanCode,Left): {
            processed = (0==0);
            int x; x = that->Pos.x[0]-1;
            if (x<0) x=0;
            that->Pos.x[0] = x;
        } break;
        case c_(Gui,ScanCode,Right): {
            processed = (0==0);
            int x; x = that->Pos.x[0]+1;
            if (x>=that->Range.x[0]) x = that->Range.x[0]-1;
            that->Pos.x[0] = x;
        } break;
    }
    return processed;
}

TlsGuiInKeyButton *TlsGuiIn2dCursorInit(TlsGuiIn2dCursor *tgt,int w,int h) {
    static struct TlsGuiInKey Static = {In2dCursorKeyboardDown};
    static struct TlsGuiInKey UpStatic = {IgnoreKeyboardUp};
    tgt->Button.Down.Static = &Static;
    tgt->Button.Up.Static = &UpStatic;
    tgt->Range.x[0]=w; tgt->Range.x[1]=h;
    tgt->Pos.x[0]=tgt->Pos.x[1]=0;
    return &tgt->Button;
}

/*---------------------------*/

static int SGEColorSet(TlsGuiInTrigger *this,unsigned int date) {
    TlsI2dPoint x;
    TlsI2dRange area;
    TlsRGBA c;
    TlsGuiTexture *Tgt;
    TlsGetThat(SingleGlyphEdit,In.colorSet);
    x.x[0]=(that->In.Glyph.Pos.x[0]*that->In.Pixel.Range.x[0])+that->In.Pixel.Pos.x[0];
    x.x[1]=(that->In.Glyph.Pos.x[1]*that->In.Pixel.Range.x[1])+that->In.Pixel.Pos.x[1];
    Tgt=that->Tgt->New.Font->Sheet;
    that->Color[1]=(that->In.Palette.Pos.x[1]<<(that->bpp>>1))+that->In.Palette.Pos.x[0];
    x_(Tgt->Format,GetColor,&c,Tgt,x.x[0],x.x[1]);
    if (c.A!=that->Color[1]) { c.A=that->Color[1];} else {c.A=that->Color[0];}
    x_(Tgt->Format,SetColor,c,Tgt,x.x[0],x.x[1]);
    area.b=x; area.e.x[0]=x.x[0]+1; area.e.x[1]=x.x[1]+1;
    x_(that->Tgt->New.Map,Update,&area);
    return (0==0);
}

typedef struct {
    TlsGuiRenderer *dst;
    struct {
        TlsGuiMappedPalette *Grey16;
    } Palettes;
} GuiCtrlEnv;

/*---------------------------------------------*/

static SingleGlyphEdit *GlyphEditNew(TlsGuiMappedFont *Model,GuiCtrlEnv *dst) {
    SingleGlyphEdit *r;
    static struct TlsGuiOut ImageStatic={SingleGlyphImageDisplay};
    static struct TlsGuiInTrigger ColorSetStatic = {&SGEColorSet};
    static struct TlsGuiOut ColorSelectStatic = {ColorSelectShowDisplay};
    rPush(r);
    r->Out.Image.TlsGuiOut.Static = &ImageStatic;
    r->bpp=4;
    r->Color[0]=0; r->Color[1]=0xf;
    r->Out.GlyphExtent.x[0]=r->Out.GlyphExtent.x[1]=8;
    r->Out.GlyphSelect.SheetDim.b.x[0]=r->Out.GlyphSelect.SheetDim.b.x[1]=0;
    r->Out.GlyphSelect.SheetDim.e.x[0]=r->Out.GlyphSelect.SheetDim.e.x[1]=16*8;
    r->Out.Image.Magnification.x[0]=r->Out.Image.Magnification.x[1]=8;
    r->Out.Image.Cursor.b.x[0]=r->Out.Image.Cursor.b.x[1]=0;
    r->Out.Image.Cursor.e.x[0]=r->Out.Image.Cursor.e.x[1]=8;
    r->Tgt = NewGlyphSheet(dst->dst,dst->Palettes.Grey16,&r->Out.GlyphExtent);
    TransfertFont(&r->Tgt->New,Model);
    r->Out.ColorSelect.TlsGuiOut.Static = &ColorSelectStatic;
    r->Out.ColorSelect.Palette = dst->Palettes.Grey16->Colors;
    r->Out.Composition = GlyphEditImageArea(r);
    TlsGuiIn2dCursorInit(&r->In.Pixel,8,8);
    TlsGuiIn2dCursorInit(&r->In.Glyph,16,16);
    TlsGuiIn2dCursorInit(&r->In.Palette,4,4);
    r->In.colorSet.Static = &ColorSetStatic;
    {   InDispatch0 *Dispatch;
        Dispatch = InDispatch0New(4);
        InDispatch0AddCursor(Dispatch,&r->In.Palette.Button,c_(Gui,ScanCode,Left),c_(Gui,ScanCode,Down),c_(Gui,ScanCode,Up),c_(Gui,ScanCode,Right));
        InDispatch0AddCursor(Dispatch,&r->In.Glyph.Button,c_(Gui,ScanCode,H),c_(Gui,ScanCode,J),c_(Gui,ScanCode,K),c_(Gui,ScanCode,L));
        InDispatch0AddCursor(Dispatch,&r->In.Pixel.Button,c_(Gui,ScanCode,x4),c_(Gui,ScanCode,x5),c_(Gui,ScanCode,x8),c_(Gui,ScanCode,x6));
        InDispatch0AddTrigger(Dispatch,&r->In.colorSet,c_(Gui,ScanCode,x0));
        r->In.Dispatch = &Dispatch->Button;
    }
    return r;
}

/*____________________________________________________
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
    int Finished;
} MapEditor;

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

/*-------------------------------------------------------*/

int SDL2_Main_Go(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll) {
    MapEditor obj;
    TlsGuiInputDevices *Inputs;
    TlsGuiOut *Msg;
    TlsGuiOut *ScaledSheet;
    TlsI2dPoint ScaledSize;
    FontParam Font;
    static struct TlsGuiInKey KeyboardDownStatic = {&KeyboardDown};
    static struct TlsGuiInTrigger MapEdQuitStatic = {&MapEdQuitTrigger};
    printf("SDL2 Up and Running. F1 or Esc to quit.\n");
    FontParamInit(&Font,Tgt);
    ScaledSize.x[0]=Font.SheetDim.e.x[0]<<1;
    ScaledSize.x[1]=Font.SheetDim.e.x[1]<<1;
    ScaledSheet = TlsGuiScaledPic(Font.Font.Map,&Font.SheetDim,&ScaledSize);
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
            TlsGuiOutFrontClr(255,192,0,255,TlsGuiOutPos(0,0,ScaledSheet)),
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
    FontParamRelease(&Font);
}

int TlsMain() {
    rOpen {
        SDL2_Main(640,480,SDL2_Main_Go);
    } rClose
    return 0;
}

