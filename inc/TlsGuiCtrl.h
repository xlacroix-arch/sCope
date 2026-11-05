/*  MIT License
 *
 *  Copyright (c) 2017 mtxavier
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

#ifndef _TlsGuiCtrl_h_
#define _TlsGuiCtrl_h_


/*______________________________________________________________
 |
 |______________________________________________________________
*/

struct c_Gui {
    struct {
        char Unknown[4],A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z;
        char k1,k2,k3,k4,k5,k6,k7,k8,k9,k0;
        char Return,Escape,Backspace,Tab,Space;
        char Minus,Equal,LeftBracket;
        char RightBracket,Backslash,NonUsHash,SemiColon,Apostrophe,Grave,Comma,Period;
        char Slash,CapsLock,F1,F2,F3,F4,F5,F6;
        char F7,F8,F9,F10,F11,F12,PrintScreen,ScrollLock;
        char Pause,Insert,Home,PgUp,Delete,End,PgDown,Right;
        char Left,Down,Up,NumLock,xDiv,xMul,xMinus,xPlus;
        char xEnter,x1,x2,x3,x4,x5,x6,x7;
        char x8,x9,x0,xPeriod,NonUsBackslash,Application,Power,xEquals;
        char F13,F14,F15,F16,F17,F18,F19,F20;
        char F21,F22,F23,F24,Execute,Help,Menu,Select;
        char Stop,Again,Undo,Cut,Copy,Paste,Find,Mute;
        char Gap0[96];
        char LCtrl,LShift,LAlt,LGui,RCtrl,RShift,RAlt,RGui;
        char _Nb;
    } ScanCode;
    struct {
        char Left,Middle,Right,X1,X2;
    } MouseButton;
};

/*______________________________________________________________
 |
 | As a convention, the various controls should return (0==0) if
 | the event was treated or (0!=0) if it was ignored.
 |______________________________________________________________
*/

typedef struct { struct TlsGuiInTrigger *Static; } TlsGuiInTrigger;
struct TlsGuiInTrigger {
    int (*Trigger)(TlsGuiInTrigger *this,unsigned int date);
};
extern TlsGuiInTrigger TlsGuiInTriggerNull;


typedef struct { struct TlsGuiInKey *Static; } TlsGuiInKey;
struct TlsGuiInKey {
    int (*Trigger)(TlsGuiInKey *this,unsigned int date,int ScanCode,int CtxtedKey);
};
extern TlsGuiInKey TlsGuiInKeyNull;

typedef struct { struct TlsGuiInSlider *Static; } TlsGuiInSlider;
struct TlsGuiInSlider {
    int (*Trigger)(TlsGuiInSlider *this,unsigned int date,int Value);
};
extern TlsGuiInSlider TlsGuiInSliderNull;

typedef struct { struct TlsGuiInCursor *Static; } TlsGuiInCursor;
struct TlsGuiInCursor {
    int (*Trigger)(TlsGuiInCursor *this,unsigned int date,int x,int y);
};
extern TlsGuiInCursor TlsGuiInCursorNull;

typedef struct { struct TlsGuiInInterpreter *Static; } TlsGuiInInterpreter;
struct TlsGuiInInterpreter {
    int (*Trigger)(TlsGuiInInterpreter *this,unsigned int date,unsigned char *Text);
};
extern TlsGuiInInterpreter TlsGuiInInterpreterNull;

/*______________________________________________________
 |
 |
 |______________________________________________________
*/

typedef struct {
    struct {
        TlsGuiInKey *Down,*Repeat,*Up;
    } Keyboard;
    struct {
        TlsGuiInKey *Down,*Up,*DoubleClick;
        TlsGuiInCursor *Pos;
        TlsGuiInSlider *Wheel[2];
    } Mouse;
    struct {
        TlsGuiInKey *Down,*Up;
        TlsGuiInSlider *Axis[12];
    } Joy[4];
    struct {
        TlsGuiInTrigger *Iconize,*Restore,*Close,*MouseEnter,*MouseLeave,*KeyboardAcquired,*KeyboardLost;
        TlsGuiInCursor *Resize,*Move;
    } Window;
} TlsGuiInputDevices;
void TlsGuiInputDevicesInit(TlsGuiInputDevices *N);

typedef struct { struct TlsGuiEventsPoll *Static;} TlsGuiEventsPoll;
struct TlsGuiEventsPoll {
    unsigned int (*msTime)(TlsGuiEventsPoll *this);
    int (*Process)(TlsGuiEventsPoll *this);
    TlsGuiInputDevices *(*CallBacks)(TlsGuiEventsPoll *this);
};

/*______________________________________________________
 |
 |
 |______________________________________________________
*/

/* Alpha: 0xff == Full opacity. */
typedef struct { unsigned char R,G,B,A; } TlsRGBA;

/* Those colors are set at the Dst position, you're free to put the extra colors before */
/*  or after. Returns the end of added colors. */
TlsRGBA *TlsGuiPaletteAddNonZero(TlsRGBA *Dst); // #00000000
/* Excluding start and end. */
TlsRGBA *TlsGuiPaletteAddGrey(TlsRGBA *Dst,unsigned char start,unsigned char end,int nb);
TlsRGBA *TlsGuiPaletteAddGradient(TlsRGBA *Dst,TlsRGBA start,TlsRGBA end,int nb); // #00000000
TlsRGBA *TlsGuiPaletteAdd666(TlsRGBA *Dst);  // 216+40 
TlsRGBA *TlsGuiPaletteAdd676(TlsRGBA *Dst);  // 252+4
TlsRGBA *TlsGuiPaletteAdd685(TlsRGBA *Dst);  // 240+16
TlsRGBA *TlsGuiPaletteAdd884(TlsRGBA *Dst);  // 256
TlsRGBA *TlsGuiPaletteAdd888Low(TlsRGBA *Dst); // half 888 following a checkered pattern
TlsRGBA *TlsGuiPaletteAdd888High(TlsRGBA *Dst);

TlsRGBA *TlsGuiPaletteBitmap(int bpp,int transparency); // For bitmaps grey or transparent

/*_______________________________________________________
 |
 | The following are actually special constant to
 | identify bitmaps and greyscale images.
 | Bitmap and greyscale images use front and back colors 
 | for their actual color.
 | PaletteColorKey are for the true color format with no
 | alpha chanel: the #010000 color is the transparent color for
 | the format that use ColorKey.
 |_______________________________________________________
*/

    extern TlsRGBA TlsGuiPaletteGreyScale,TlsGuiPaletteColorKey;

typedef struct {
    struct TlsGuiTextureFormat *Static;
    int bpp;
    int CmpntNb;
    int bpcmpnt[4]; // in crescent order. Less significant bit is offset 0.
    struct { int r,g,b,a; } CmpntIdx;
} TlsGuiTextureFormat;
typedef struct {
    TlsGuiTextureFormat *Format;
    TlsRGBA *Palette;
    int w,h;
    int pitch; // in bytes
    unsigned char *data;
} TlsGuiTexture;

typedef struct { struct TlsGuiTextureRowUnpack *Static;} TlsGuiTextureRowUnpack;
typedef struct { struct TlsGuiTextureRowPack *Static;} TlsGuiTextureRowPack;
struct TlsGuiTextureRowUnpack {
    TlsRGBA *(*Next)(TlsGuiTextureRowUnpack *this,int *nb);
};
struct TlsGuiTextureRowPack {
    void (*Next)(TlsGuiTextureRowPack *this,TlsRGBA *Unpacked,int nb);
};

struct TlsGuiTextureFormat {
    /* This is for translating data: the source unpack the data in a private buffer that will be Packed by the dst. */
    TlsGuiTextureRowUnpack *(*Unpack)(TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb);
    TlsGuiTextureRowPack *(*Pack)(TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y);
    void (*GetColor)(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y);
    void (*SetColor)(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y);
};

#include <Geo.h>

typedef struct {
    TlsI2dPoint b,e;
} TlsI2dRange;

    /*-----------------------*/

TlsGuiTextureFormat *TlsGuiTxtrFormatBitmap(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatBitmapLSB(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatIndex4(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatIndex4LSB(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatIndex8(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatRGBA4444(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatARGB4444(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatBGRA4444(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatABGR4444(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatRGB565(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatBGR565(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatRGB888(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatBGR888(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatRGBA8888(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatBGRA8888(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatARGB8888(void);
TlsGuiTextureFormat *TlsGuiTxtrFormatABGR8888(void);

    /*-----------------------*/

TlsGuiTexture *TlsGuiTextureNew(TlsGuiTextureFormat *Format,TlsRGBA *Palette,int dx,int dy);
typedef struct {
    TlsGuiTexture *Texture;
    TlsI2dRange Pic;
} TlsGuiTextureArea;
void TlsGuiTextureTransfert(TlsGuiTextureArea *dst,TlsGuiTextureArea *src);

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct { unsigned short uv[2]; } TlsUVCoord;

typedef struct {
    TlsI3dPoint *Anchors;
    int AnchorNb;
    struct TlsGeoMeshIdx {
        int Anchor;
        TlsUVCoord uv;
        TlsRGBA Color;
    } *Vertices;
    int VertexNb;
    int *Idx;
    int IdxNb;
} TlsIGeoMesh;

typedef struct { // !!! This structures mirrors SDL_Vertex and should only be changed accordingly.
    Tlsf2dPoint Point;
    TlsRGBA Color;
    struct { float uv[2]; } uv; 
} TlsGuiScreenVtx;
typedef struct {
    TlsGuiScreenVtx *Vtx;
    int *Idx;
    int VtxNb,IdxNb;
} TlsGuiScreenMesh;

typedef struct {
    TlsRGBA Color;
    TlsI3dPoint x;
    TlsUVCoord uv;
} TlsIVertex;

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    struct TlsGuiRenderer *Static;
    TlsI2dPoint CursorPos;
    TlsI2dRange Clip;
    TlsRGBA Front,Back;
    float vScale; // pixel/pts.
    TlsI4dMat Global; /* 8:24 fixed point for the rotation part. */
} TlsGuiRenderer;

typedef struct {
    struct TlsGuiMappedPalette *Static;
    TlsGuiTextureFormat *Format;
    TlsRGBA *Colors;
} TlsGuiMappedPalette;
struct TlsGuiMappedPalette {
    void (*Update)(TlsGuiMappedPalette *this,int b,int e);
    int (*Release)(TlsGuiMappedPalette *this); // can fail if Mapped textures are still attached.
};
extern TlsGuiMappedPalette TlsGuiMappedPaletteNull;

typedef struct { struct TlsGuiMappedTexture *Static; } TlsGuiMappedTexture;
struct TlsGuiMappedTexture {
    void (*Draw)(TlsGuiMappedTexture *this,TlsI2dRange *Area);
    void (*ScaleDraw)(TlsGuiMappedTexture *this,TlsI2dRange *Area,TlsI2dPoint *DstDim);
    void (*OrthoTriangles)(TlsGuiMappedTexture *this,TlsGuiScreenMesh *Mesh);
    void (*Update)(TlsGuiMappedTexture *this,TlsI2dRange *Area);
    void (*Release)(TlsGuiMappedTexture *this);
};

struct TlsGuiRenderer {
    void (*SetFront)(TlsGuiRenderer *this);
    void (*SetBack)(TlsGuiRenderer *this);
    void (*SetClip)(TlsGuiRenderer *this);
    void (*Clear)(TlsGuiRenderer *this);
    void (*Point)(TlsGuiRenderer *this);
    void (*Line)(TlsGuiRenderer *this,int x1,int y1);
    void (*Rectangle)(TlsGuiRenderer *this,int x1,int y1);
    void (*FillRectangle)(TlsGuiRenderer *this,int x1,int y1);
    void (*Display)(TlsGuiRenderer *this);
    TlsGuiMappedTexture *(*MapTexture)(TlsGuiRenderer *this,int Updatable,
        TlsGuiMappedPalette *palette,TlsGuiTexture *Txt);
    TlsGuiMappedPalette *(*MapPalette)(TlsGuiRenderer *this,TlsGuiTextureFormat *Format,TlsRGBA *Colors);
    void (*OrthoTriangles)(TlsGuiRenderer *this,TlsGuiScreenMesh *Mesh);
};

typedef struct { struct TlsGuiOut *Static; } TlsGuiOut;
struct TlsGuiOut{
    void (*Display)(TlsGuiOut *this,TlsGuiRenderer *State);   
};

TlsGuiOut *TlsGuiOutPair(TlsGuiOut *A,TlsGuiOut *B);
TlsGuiOut *TlsGuiOutList(TlsGuiOut *A,...);
TlsGuiOut *TlsGuiOutMobArray(int nb,TlsGuiOut **b);
TlsGuiOut *TlsGuiOutMobSelect(int *Selector,int nb,TlsGuiOut **b);

TlsGuiOut *TlsGuiOutPos(int x,int y,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutGrabPoint(int x,int y,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutFrontClr(int R,int G,int B,int A,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutBackClr(int R,int G,int B,int A,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutClip(TlsI2dRange *Clip,TlsGuiOut *Child);

TlsGuiOut *TlsGuiOutMobPos(TlsI2dPoint *Pos,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutMobGrabPoint(TlsI2dPoint *Pos,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutMobFrontClr(TlsRGBA *Color,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutMobBackClr(TlsRGBA *Color,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutMobClip(TlsI2dRange *Clip,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutMobScaledPos(TlsGuiOut *Child,TlsI2dPoint *Scale,TlsI2dPoint *Pos);

TlsGuiOut *TlsGuiOutMobRect(TlsI2dPoint *Xtnt);
TlsGuiOut *TlsGuiOutMobFilledRect(TlsI2dPoint *Xtnt);

TlsGuiOut *TlsGuiPoint(void);
TlsGuiOut *TlsGuiLine(int x1,int y1);
TlsGuiOut *TlsGuiRectangle(int x1,int y1);
TlsGuiOut *TlsGuiFilledRectangle(int x1,int y1);
TlsGuiOut *TlsGuiClear(void);

TlsGuiOut *TlsGuiMappedPic(TlsGuiMappedTexture *txt,TlsI2dRange *Area);
TlsGuiOut *TlsGuiScaledPic(TlsGuiMappedTexture *txt,TlsI2dRange *Area,TlsI2dPoint *TgtSize);


TlsGuiOut *TlsGuiOutMobVerticesPos(TlsI4dMat *Pos,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutMobSetVScale(float *vScale,TlsGuiOut *Child);
TlsGuiOut *TlsGuiOutOrthVertices(TlsGuiMappedTexture *Txt,TlsIGeoMesh *Mesh);

TlsI4dMat *TlsI4dMat24Mult(TlsI4dMat *r,TlsI4dMat *A,TlsI4dMat *B);


/*_____________________________________________________________
 |
 | Fonts
 |_____________________________________________________________
*/

typedef struct {
    TlsGuiTexture *Sheet;
    int dx,dy,GlyphNb;
    TlsI2dRange *Glyphs;
} TlsGuiSimpleFont;
typedef struct {
    TlsGuiSimpleFont *Font;
    TlsGuiMappedTexture *Map;
} TlsGuiMappedFont;

TlsGuiOut *TlsGuiDisplayedString(TlsGuiMappedFont *Fnt,char *txt);

#endif


