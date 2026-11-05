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

static TlsI4dMat DodecaBase =  {{
    InvSqrt2,         0   ,-InvSqrt2,0,
    -InvSqrt3,-InvSqrt3   ,-InvSqrt3,0,
    -InvSqrt6, InvSqrt6<<1,-InvSqrt6,0,
    0,0,0,1
}};

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

TlsIGeoMesh *TlsGuiCubeMesh(int a,TlsGeoUVWindow *uv) {
    TlsIGeoMesh *r;
    int i;
    static int Idx[36],inited=(0!=0);
    if (!inited) {
        int *p=Idx;
        for (i=0;i<24;i+=4) {
            p[0]=i; p[1]=i+1; p[2]=i+3; p[3]=i+2; p[4]=i+3; p[5]=i+1;
            p+=6;
        }
    }
    rPush(r);
    r->IdxNb=36;
    r->Idx=Idx;
    /* */{
        TlsI3dPoint *p;
        r->AnchorNb=8;
        rnPush(r->Anchors,8);
        p = r->Anchors;
        p->x[0]=0; p->x[1]=0; p->x[2]=0; p++;
        p->x[0]=a; p->x[1]=0; p->x[2]=0; p++;
        p->x[0]=0; p->x[1]=a; p->x[2]=0; p++;
        p->x[0]=a; p->x[1]=a; p->x[2]=0; p++;
        p->x[0]=0; p->x[1]=0; p->x[2]=a; p++;
        p->x[0]=a; p->x[1]=0; p->x[2]=a; p++;
        p->x[0]=0; p->x[1]=a; p->x[2]=a; p++;
        p->x[0]=a; p->x[1]=a; p->x[2]=a;
    }
    /* */ {
        struct TlsGeoMeshIdx *p;
        unsigned int u[4],v[3],a,b,l,c;
        TlsRGBA White={255,255,255,255};
        r->VertexNb=24;
        rnPush(r->Vertices,24);
        p=r->Vertices;
        l = uv->TextureSize.x[0]; a=uv->Window.b.x[0]; b=uv->Window.e.x[0];
        c = a; c =((c<<16)-c)/l; u[0]=c;
        c = a+((b-a)/3); c =((c<<16)-c)/l; u[1]=c;
        c = a+(((b-a)*2)/3); c=((c<<16)-c)/l; u[2]=c;
        c = b; c =((c<<16)-c)/l; u[3]=c;
        l = uv->TextureSize.x[1]; a=uv->Window.b.x[1]; b=uv->Window.e.x[1];
        c = a; c =((c<<16)-c)/l; v[0]=c;
        c = a+((b-a)/2); c=((c<<16)-c)/l; v[1]=c;
        c = b; c =((c<<16)-c)/l; v[2]=c;
        #define NextFace(i0,i1,i2,i3,u0,v0)\
            p[0].Color=p[1].Color=p[2].Color=p[3].Color=White;\
            p[0].Anchor=i0; p[1].Anchor=i1; p[2].Anchor=i2; p[3].Anchor=i3;\
            p[0].uv.uv[0]=u[u0]; p[0].uv.uv[1]=v[v0];\
            p[1].uv.uv[0]=u[u0]; p[1].uv.uv[1]=v[v0+1];\
            p[2].uv.uv[0]=u[u0+1]; p[2].uv.uv[1]=v[v0+1];\
            p[3].uv.uv[0]=u[u0+1]; p[3].uv.uv[1]=v[v0];\
            p+=4
        NextFace(4,0,1,5,0,0);
        NextFace(5,1,3,7,1,0);
        NextFace(7,3,2,6,2,0);
        NextFace(7,6,4,5,0,1);
        NextFace(5,7,6,4,0,1);
        NextFace(4,6,2,0,1,1);
        NextFace(0,2,3,1,2,1);
        #undef NextFace
    }
    return r;
}

void TlsGuiRhombDodecaUniform(TlsIGeoMesh *dst,TlsRGBA *Color) {
    int i;
    struct TlsGeoMeshIdx *p;
    TlsRGBA col={255,255,255,255};
    if (Color) col=*Color;
    p = dst->Vertices;
    for (i=0;i<48;i++) { p->Color=col; p++; }
}
void TlsGuiRhombDodecaStraight(TlsIGeoMesh *dst,TlsGeoUVWindow *uv) {
    /* */ {
        struct TlsGeoMeshIdx *p;
        unsigned int u[8],v[12],a,b,l,c;
        p=dst->Vertices;
        { int i,j;
          l = uv->TextureSize.x[0]; a=uv->Window.b.x[0]; b=uv->Window.e.x[0];
          for (i=0;i<8;i++) {
              c=a+((i*(b-a))/7); c=((c<<16)-c)/l;  u[i]=c;
          }
          l = uv->TextureSize.x[1]; a=uv->Window.b.x[1]; b=3+uv->Window.e.x[1];
          j=0;
          for (i=0;i<4;i++) {
              c=a+((i*(b-a))/5); c=((c<<16)-c)/l;  v[j]=c;
              c=a+(((i+2)*(b-a))/5)-3; c=((c<<16)-c)/l;  v[j+2]=c;
              v[j+1]=(v[j]+v[j+2])>>1;
              j+=3;
          }
        }
        #define NextFace(i0,i1,i2,i3,u0,v0)\
            p[0].Anchor=i0; p[1].Anchor=i1; p[2].Anchor=i2; p[3].Anchor=i3;\
            p[0].uv.uv[0]=u[u0]; p[0].uv.uv[1]=v[v0];\
            p[1].uv.uv[0]=u[u0-1]; p[1].uv.uv[1]=v[v0+1];\
            p[2].uv.uv[0]=u[u0]; p[2].uv.uv[1]=v[v0+2];\
            p[3].uv.uv[0]=u[u0+1]; p[3].uv.uv[1]=v[v0+1];\
            p+=4
        NextFace(7,11,3,12,1,0);
        NextFace(7,12,6,13,3,0);
        NextFace(7,13,5,11,5,0);
        NextFace(1,9,0,10, 2,9);
        NextFace(1,10,3,11,2,3);
        NextFace(1,11,5,9, 4,3);
        NextFace(2,10,0,8, 4,9);
        NextFace(2,8,6,12, 6,3);
        NextFace(2,12,3,10,1,6);
        NextFace(4,9,5,13, 3,6);
        NextFace(4,13,6,8, 5,6);
        NextFace(4,8,0,9,  6,9);
        #undef NextFace
    }
}

TlsIGeoMesh *TlsGuiRhombDodecaMesh(int a,TlsGeoUVWindow *uv,
    void (*ColorScheme)(TlsIGeoMesh *dst,TlsRGBA *Colors),TlsRGBA *Colors) {
    TlsIGeoMesh *r;
    int i;
    static int Idx[72],inited=(0!=0);
    TlsRGBA AllWhite[14];
    if (!inited) {
        int *p=Idx,i;
        for (i=0;i<48;i+=4) {
            p[0]=i; p[1]=i+1; p[2]=i+3; p[3]=i+2; p[4]=i+3; p[5]=i+1;
            p+=6;
        }
    }
    rPush(r);
    r->IdxNb=72;
    r->Idx=Idx;
    /* */{
        TlsI3dPoint *p;
        r->AnchorNb=14;
        rnPush(r->Anchors,14);
        p = r->Anchors;
        p->x[0]=0; p->x[1]=0; p->x[2]=0; p++;
        p->x[0]=a; p->x[1]=0; p->x[2]=0; p++;
        p->x[0]=0; p->x[1]=a; p->x[2]=0; p++;
        p->x[0]=a; p->x[1]=a; p->x[2]=0; p++;
        p->x[0]=0; p->x[1]=0; p->x[2]=a; p++;
        p->x[0]=a; p->x[1]=0; p->x[2]=a; p++;
        p->x[0]=0; p->x[1]=a; p->x[2]=a; p++;
        p->x[0]=a; p->x[1]=a; p->x[2]=a; p++;
        p->x[0]=-(a>>1); p->x[1]=(a>>1); p->x[2]=(a>>1); p++;
        p->x[0]= (a>>1); p->x[1]=-(a>>1); p->x[2]=(a>>1); p++;
        p->x[0]= (a>>1); p->x[1]=(a>>1); p->x[2]=-(a>>1); p++;
        p->x[0]=((3*a)>>1); p->x[1]=(a>>1); p->x[2]=(a>>1); p++;
        p->x[0]= (a>>1); p->x[1]=((3*a)>>1); p->x[2]=(a>>1); p++;
        p->x[0]= (a>>1); p->x[1]=(a>>1); p->x[2]=((3*a)>>1);
    }
    r->VertexNb=48;
    rnPush(r->Vertices,48);
    ColorScheme(r,Colors);
    TlsGuiRhombDodecaStraight(r,uv);
    return r;
}

/*____________________________________________________
 |
 | Generates test texture.
 |____________________________________________________
*/

typedef struct {
    int w,h;
    unsigned char *pattern;
} AsciiPattern;

//  | !"#$%&'()*+,-./
//  |0123456789:;<=>?
//  |@ABCDEFGHIJKLMNO
//  |PQRSTUVWXYZ[\]^_
//  |`abcdefghijklmno
//  |pqrstuvwxyz{|}~

// 4 colors : " -*#"

static AsciiPattern *ArrowUp(void) {
    static unsigned char data[] = {
"   ##   "
"  ####  "
" ###### "
"## ## ##"
"   ##   "
"   ##   "
"   ##   "
"   ##   "
    };
    static AsciiPattern Pattern = {8,8,data};
    return &Pattern;
}
static AsciiPattern *Circle(void) {
    static unsigned char data[] = {
"   ##   "
" *####* "
" ##  ## "
"##    ##"
"##    ##"
" ##  ## "
" *####* "
"   ##   "
    };
    static AsciiPattern Pattern = {8,8,data};
    return &Pattern;
}

static void AddPattern(TlsRGBA *data,AsciiPattern *P,TlsRGBA Color,TlsRGBA Color1,TlsRGBA Color2) {
    int  i,j;
    TlsRGBA c[4]={{0xff,0xff,0xff,0},Color,Color1,Color2};
    TlsRGBA *p,*q;
    unsigned char *s=P->pattern;
    p=data;
    for (j=0;j<P->h;j++) {
        q=p; p+=256;
        for (i=0;i<P->w;i++){
            if (s[0]&3) {*q=c[s[0]&3];}
            q++; s++;
        }
    }
}

static struct {int offset,length;} RhombicShape[20] = {
    {10,4},{9,6},{7,10}, {6,12},{5,14},{3,18},{2,20},
    {0,24}, {0,24},{0,24},
    {2,20},{3,18},{5,14},{6,12}, {7,10}, {9,6},{10,4},
    {24,0}, {24,0},{24,0}
};

static void FillRhombic(TlsRGBA *data,TlsRGBA Color,TlsRGBA Color1) {
    int i,j,o;
    TlsRGBA *p=data;
    TlsRGBA c[2]; c[0]=Color; c[1]=Color1; o=0;
    for (i=0;i<17;i++) {
         TlsRGBA *q=p+RhombicShape[i].offset; p+=256;
         o=i+RhombicShape[i].offset;
         j=RhombicShape[i].length;
         while (j>0) { *q++=c[(o++)&1]; j--; }
    }
}

static void RhomboDodecaTxtrTemplate0(TlsRGBA *data,TlsRGBA *Pattern) {
    FillRhombic(data,Pattern[0],Pattern[1]); 
    FillRhombic(data+24,Pattern[0],Pattern[1]);
    FillRhombic(data+48,Pattern[0],Pattern[1]);
    FillRhombic(data+(9+1)*256+12,Pattern[2],Pattern[3]);
    FillRhombic(data+(9+1)*256+12+24,Pattern[2],Pattern[3]);
    FillRhombic(data+(9+1)*256+12+48,Pattern[2],Pattern[3]);
    FillRhombic(data+(17+3)*256,Pattern[2],Pattern[3]);
    FillRhombic(data+(17+3)*256+24,Pattern[2],Pattern[3]);
    FillRhombic(data+(17+3)*256+48,Pattern[2],Pattern[3]);
    FillRhombic(data+(9+17+4)*256+12,Pattern[4],Pattern[5]);
    FillRhombic(data+(9+17+4)*256+12+24,Pattern[4],Pattern[5]);
    FillRhombic(data+(9+17+4)*256+12+48,Pattern[4],Pattern[5]);
}

#define cBrown (TlsRGBA){0x89,0x51,0x29,0xff}
#define cSpringGreen (TlsRGBA){0x0,0xff,0x7f,0xff}
#define cLimeGreen (TlsRGBA){0x32,0xCD,0x32,0xff}
#define cDarkSlateBlue (TlsRGBA){0x48,0x3D,0x8B,0xff}
#define cPurple (TlsRGBA){0x80,0,0x80,0xff}
#define cSienna (TlsRGBA){0xAB,0x52,0x2D,0xff}
#define cSaddleBrown (TlsRGBA){0x8B,0x45,0x13,0xff}
#define cRed (TlsRGBA){0xff,0,0,0xff}
#define cDimGray  (TlsRGBA){0x69,0x69,0x69,0xff}
#define cGray (TlsRGBA){0x80,0x80,0x80,0xff}
#define cDarkSlateGray (TlsRGBA){0x2F,0x4F,0x4F,0xff}
#define cSlateGray (TlsRGBA){0x70,0x80,0x90,0xff}
#define cLightSlateGray (TlsRGBA){0x77,0x88,0x99,0xff}
#define cBlue (TlsRGBA){0,0,0xff,0xff}

static TlsGuiTexture *TestTexture(void) {
    TlsGuiTexture *r;
    TlsRGBA *data,*q;
    TlsRGBA color;
    int i,j;
    r = TlsGuiTextureNew(TlsGuiTxtrFormatRGBA8888(),0,256,256);
    data = ((void *)(r->data));
    color = cBlue; q=data; for (i=0;i<0x10000;i++) { *q++=color; }
    TlsRGBA Colors[12] = {
        cSpringGreen,cLimeGreen,cSaddleBrown,cSienna,cDarkSlateBlue,cPurple,
        cDimGray,cLightSlateGray,cSlateGray,cGray,cDarkSlateGray,cDarkSlateBlue,
    };
    RhomboDodecaTxtrTemplate0(data,Colors);
    RhomboDodecaTxtrTemplate0(data+72,Colors+6);
    AddPattern(data+3*256+8,ArrowUp(),cSienna,cSienna,cSienna);
    AddPattern(data+3*256+32,ArrowUp(),cBlue,cBlue,cBlue);
    AddPattern(data+3*256+56,ArrowUp(),cRed,cRed,cRed);
    return r;
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
        struct { 
            TlsGuiTexture *Src;
            TlsGuiMappedTexture *Map;
        } Test0;
    } Textures;
    struct {
        TlsGuiMeshBound Lib[64];
        TlsGuiCamera POV;
        TlsGuiWorldChunk *World;
        TlsGuiOut *WorldView;
    } BlockLib;
    struct {
        unsigned int TimeOrg;
        TlsI2dPoint Pos;
    } SelectStart,CursorX;
    struct {
        int Scale;
        TlsI4dMat SceneOrient;
        TlsI4dMat Orientation;
    } SolidArea;
    struct { int Width,Height; } GlyphMap;
    TlsI2dPoint MobsPos[16];
    int Finished;
} TestItf;


static int DrawScreen(TestItf *obj) {
    TlsI4dMat A,B,C;
    int alpha=0, beta=0,offorg=obj->SolidArea.Scale>>1;
    long long v;
    obj->MobsPos[1].x[0] = (obj->CurrentTime/10)%600;
    obj->MobsPos[1].x[1] = 0;
    v=obj->CursorX.Pos.x[0]; v=((v*TwoPi24)/obj->GlyphMap.Width); alpha=v;
    v=obj->CursorX.Pos.x[1]; v=((v*TwoPi24)/obj->GlyphMap.Height); beta=v;
    TlsI4dMatSetRot(&A,alpha,1);
    TlsI4dMatSetRot(&C,beta,0); 
    TlsI4dMat24Mult(&obj->SolidArea.Orientation,&C,&A);
    /*Set Up the camera */{
         int Scale=obj->SolidArea.Scale;
         TlsGuiCamera POV,POV1;
         TlsI4dMat D;
         TlsI3dPoint O={-3*Scale,-(3*Scale/2),-(3*Scale/2)};
         POV.Ref.O=(TlsI3dPoint){3*Scale,-10*Scale,2*Scale};
          POV.Ref.Axis = (TlsI3dMat){
             InvSqrt2 ,-InvSqrt3,-InvSqrt6,
             0,        -InvSqrt3,InvSqrt6<<1,
             -InvSqrt2,-InvSqrt3,-InvSqrt6
         };
         /* TlsI4dMatPos(&B,-3*Scale,-(3*Scale)/2,-(3*Scale)/2);
         TlsI4dMat24Mult(&D,&A,&B);
         TlsI4dMat24Mult(&B,&C,&D);
         B.x[0][3]+=3*Scale; B.x[1][3]+=(3*Scale)/2; B.x[2][3]+=(3*Scale)/2;
         */
         TlsI3dVec Axis={InvSqrt3,InvSqrt3,InvSqrt3};
         TlsI4dMat24AffRot(&A,&O,&Axis,alpha);
         TlsGuiI3dReferTrans(&POV1.Ref,&A,&POV.Ref);
         Axis = (TlsI3dVec){POV1.Ref.Axis.x[0][0],POV1.Ref.Axis.x[1][0],POV1.Ref.Axis.x[2][0]};
         TlsI4dMat24AffRot(&C,&O,&Axis,beta);
         TlsGuiI3dReferTrans(&obj->BlockLib.POV.Ref,&C,&POV1.Ref);
    }
    x_(obj->Scene,Display,obj->Tgt);
    x_(obj->Tgt,Display);
}

static int MapEdQuitTrigger(TlsGuiInTrigger *this,unsigned int date) {
    TlsGetThat(TestItf,Quit);
    that->Finished = (0==0);
    return (0==0);
} 

static int KeyboardDown(TlsGuiInKey *this,unsigned int date,int c,int Key) {
    int processed;
    TlsGetThat(TestItf,KeyDown);
    processed = (0!=0);
    that->Finished = (that->Finished)||(c==c_(Gui,ScanCode,F1))||(c==c_(Gui,ScanCode,Escape));
    processed = that->Finished;
    switch (c) {
        case c_(Gui,ScanCode,Up): {
            processed = (0==0);
            int y; y = (that->CursorX.Pos.x[1]-1)%that->GlyphMap.Height;
            that->CursorX.Pos.x[1] = y;
        } break;
        case c_(Gui,ScanCode,Down): {
            processed = (0==0);
            int y; y = (that->CursorX.Pos.x[1]+1)%that->GlyphMap.Height;
            that->CursorX.Pos.x[1] = y;
        } break;
        case c_(Gui,ScanCode,Left): {
            processed = (0==0);
            int x; x = (that->CursorX.Pos.x[0]-1)%that->GlyphMap.Width;
            that->CursorX.Pos.x[0] = x;
        } break;
        case c_(Gui,ScanCode,Right): {
            processed = (0==0);
            int x; x = (that->CursorX.Pos.x[0]+1)%that->GlyphMap.Width;
            that->CursorX.Pos.x[0] = x;
        } break;
    }
    return processed;
}

TlsGuiSimpleFont *TlsGuiNewAsciiFont8x8(void);

int SDL2_Main_Go(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll) {
    TestItf obj;
    TlsGuiInputDevices *Inputs;
    TlsGuiMeshBound BlockLib[32];
    TlsGuiOut *Msg;
    TlsRGBA AllWhite[14];
    struct {
        TlsGuiMappedFont Font;
        TlsGuiMappedPalette *Bitmap1Palette;
        TlsI2dRange SheetDim;
        TlsGuiOut *AllSheetImg;
    } Font;
    static struct TlsGuiInKey KeyboardDownStatic = {&KeyboardDown};
    static struct TlsGuiInTrigger MapEdQuitStatic = {&MapEdQuitTrigger};
    printf("SDL2 Up and Running. F1 or Esc to quit.\n");
    {int i; for (i=0;i<14;i++) { TlsRGBA White={255,255,255,255}; AllWhite[i]=White; } }
    Font.Bitmap1Palette=x_(Tgt,MapPalette,TlsGuiTxtrFormatBitmap(),TlsGuiPaletteBitmap(1,(0==0)));
    Font.Font.Font = TlsGuiNewAsciiFont8x8();
    Font.Font.Map = x_(Tgt,MapTexture,(0!=0),Font.Bitmap1Palette,Font.Font.Font->Sheet);
    Font.SheetDim.b.x[0] = Font.SheetDim.b.x[1]=0;
    Font.SheetDim.e.x[0] = Font.Font.Font->Sheet->w; Font.SheetDim.e.x[1] = Font.Font.Font->Sheet->h;
    Font.AllSheetImg = TlsGuiMappedPic(Font.Font.Map,&Font.SheetDim);
    Msg = TlsGuiDisplayedString(&Font.Font,"Hello World! F1 or Esc To quit.");
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
    obj.SolidArea.Scale = 0x800;
    obj.SolidArea.Orientation = I4dMatId;
    obj.Textures.Test0.Src=TestTexture();
    obj.Textures.Test0.Map=x_(Tgt,MapTexture,(0!=0),0,obj.Textures.Test0.Src);
    {
        int Scale=obj.SolidArea.Scale;
        long long occlrad; occlrad=Scale; occlrad=(occlrad*Sqrt2_24)>>25; 
        TlsGeoUVWindow uv;
        TlsISphere Bound={{Scale/2,Scale/2,Scale/2},Scale},Occlusion={{Scale/2,Scale/2,Scale/2},occlrad};
        obj.BlockLib.Lib[0].Bound=Bound;
        obj.BlockLib.Lib[0].Occlusion=Occlusion;
            uv.TextureSize.x[0]=uv.TextureSize.x[1]=256;
            uv.Window.b.x[0]=uv.Window.b.x[1]=0;
            uv.Window.e.x[0]=84; uv.Window.e.x[1]=47;
        obj.BlockLib.Lib[0].Mesh=TlsGuiOutOrthVertices(obj.Textures.Test0.Map,
            TlsGuiRhombDodecaMesh(obj.SolidArea.Scale,&uv,TlsGuiRhombDodecaUniform,AllWhite));
        obj.BlockLib.Lib[1].Bound=Bound;
        obj.BlockLib.Lib[1].Occlusion=Occlusion;
            uv.Window.b.x[0]=72; uv.Window.e.x[0]=156;
        obj.BlockLib.Lib[1].Mesh=TlsGuiOutOrthVertices(obj.Textures.Test0.Map,
            TlsGuiRhombDodecaMesh(obj.SolidArea.Scale,&uv,TlsGuiRhombDodecaUniform,AllWhite));
        obj.BlockLib.World=TlsRhombicChunk888(Scale,obj.BlockLib.Lib,2,64);
        {int i,j,k; 
             for(k=0;k<3;k++) for(j=0;j<3;j++) for(i=0;i<3;i++) { 
                 x_(obj.BlockLib.World,SetCell,i,j,k,(j+k)&1);
        } }
        /*Set Up the camera */{
            obj.BlockLib.POV.Ref.O=(TlsI3dPoint){2*Scale,8*Scale,2*Scale};
            obj.BlockLib.POV.Ref.Axis = (TlsI3dMat){
                 InvSqrt2 ,-InvSqrt3,-InvSqrt6,
                 0,        -InvSqrt3,InvSqrt6<<1,
                 -InvSqrt2,-InvSqrt3,-InvSqrt6
            };
            obj.BlockLib.POV.Clip.TopLeft=(TlsI3dVec){-320,-240,600};
            obj.BlockLib.POV.Clip.z0=Scale; obj.BlockLib.POV.Clip.z1=16*Scale;
        }
        obj.BlockLib.WorldView=TlsGuiMeshSceneNew(&obj.BlockLib.POV,&obj.BlockLib.World->TlsGuiMeshBag);
    }
    {
        TlsGuiOut *s0,*s1,*s2;
        obj.Tgt = Tgt;
        TlsGeoUVWindow uv;
        TlsRGBA Colors[14]={
           {192,192,192,255},{32,224,224,255},{240,32,224,255},{32,32,224,255},
           {224,224,32,255},{32,224,32,255},{224,32,192,255},{255,180,32,255},
           {255,0,64,255},{64,255,0,255},{0,127,255,255},{127,255,255,255},
           {255,0,255,255},{0,255,255,255}
        };
        uv.TextureSize.x[0]=uv.TextureSize.x[1]=256;
        uv.Window.b.x[0]=uv.Window.b.x[1]=0;
        uv.Window.e.x[0]=84; uv.Window.e.x[1]=47;
        {
            TlsI2dRange *Area;
            rPush(Area); Area->b.x[0]=Area->b.x[1]=0; Area->e.x[0]=84; Area->e.x[1]=47;
            s1 = TlsGuiMappedPic(obj.Textures.Test0.Map,Area);
        }
        TlsI4dMat A;
        TlsI4dMatPos(&A,-obj.SolidArea.Scale>>1,-obj.SolidArea.Scale>>1,-obj.SolidArea.Scale>>1);
        TlsI4dMat24Mult(&obj.SolidArea.SceneOrient,&DodecaBase,&A);
        s2 = TlsGuiOutPos(200,200,obj.BlockLib.WorldView
           // TlsGuiOutMobVerticesPos(&obj.SolidArea.Orientation,TlsGuiOutMobVerticesPos(&obj.SolidArea.SceneOrient,obj.BlockLib.Lib[0].Mesh))
        );
        s0 = TlsGuiOutPos(8,8,TlsGuiOutFrontClr(255,128,0,255,Msg));
        obj.Scene = TlsGuiOutBackClr(32,32,32,255,
            TlsGuiOutFrontClr(255,255,255,255,TlsGuiOutPair(TlsGuiClear(),TlsGuiOutList(
                s0,
                TlsGuiOutPos(320,0,s1),
                s2,0
            ))));
    }
    obj.TimeOrg = x_(Poll,msTime);

    do {
        obj.CurrentTime = x_(Poll,msTime);
        DrawScreen(&obj);
        while ((x_(Poll,Process))&&(!obj.Finished)) {}
    } while(!obj.Finished);
    x_(obj.Textures.Test0.Map,Release);
    x_(Font.Font.Map,Release);
    x_(Font.Bitmap1Palette,Release);
}

int TlsMain() {
    SDL2_Main(640,480,SDL2_Main_Go);
    return 0;
}

