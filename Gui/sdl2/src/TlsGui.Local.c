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

#include <TlsGui.Local.h>

#include <string.h>

#define TlsMin(a,b) ((a)<=(b))?(a):(b);

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

/*
int ILg2(unsigned int v) {
    unsigned int v0,r,p;
    v=v|(v>>1); v=v|(v>>2); v=v|(v>>4); v=v|(v>>8); v=v|(v>>16);
    v0=v>>16; p=v0&1; r=p<<4; v= v^v0;
    v0=v>>8;  p= p^(v0&1); r|=p<<3; v=v^v0;
    v0=v>>4;  p= p^(v0&1); r|=p<<2; v=v^v0;
    v0=v>>2;  p= p^(v0&1); r|=p<<1; v=v^v0;
    p= p^((v>>1)&1); r|=p;
    return r;
}
*/

const TlsI4dMat I4dMatId={{{1<<24,0,0,0},{0,1<<24,0,0},{0,0,1<<24,0},{0,0,0,1}}};
void SinCos(int *sin,int *cos,int Alpha) {
   int r[2],n,c[4];
   long long inc,alpha;
   static struct { int CosIdx,CosSgn,SinSgn; } quadrant[8] = {
       {0,1,1},{1,1,1},{1,-1,1},{0,-1,1},{0,-1,-1},{1,-1,-1},{1,1,-1},{0,1,-1}
   },*quad;
   Alpha = (Alpha<0)?(TwoPi24-((-Alpha)%TwoPi24)):Alpha%TwoPi24;
   quad = quadrant+((Alpha<<3)/TwoPi24);
   Alpha = Alpha%(TwoPi24>>2);
   if (Alpha>(TwoPi24>>3)) { Alpha = (TwoPi24>>2)-Alpha; }
   r[0]=1<<24; r[1]=Alpha; n=2; inc=alpha=Alpha;
   while (inc!=0) {
       inc = ((inc*alpha)/n)>>24; c[0]=inc; n++; 
       inc = ((inc*alpha)/n)>>24; c[1]=inc; n++; 
       inc = ((inc*alpha)/n)>>24; c[2]=inc; n++; 
       inc = ((inc*alpha)/n)>>24; c[3]=inc; n++;
       r[0]+=(c[2]-c[0]); r[1]+=c[3]-c[1];
   }
   *cos=r[quad->CosIdx]; if (quad->CosSgn<0) *cos=-*cos;
   *sin=r[1-quad->CosIdx]; if (quad->SinSgn<0) *sin=-*sin;
}

TlsI4dMat *TlsI4dMatPos(TlsI4dMat *r,int x,int y,int z) {
    *r=I4dMatId;
    r->x[0][3]=x; r->x[1][3]=y; r->x[2][3]=z;
    return r;
}
TlsI4dMat *TlsI4dMatSetRot(TlsI4dMat *r,int alpha,int axis) {
    int s,c,i0,i1,i2;
    SinCos(&s,&c,alpha);
    *r=I4dMatId;
    i0 = axis; i1=(i0+1)%3; i2=(i0+2)%3;
    r->x[i1][i1]=r->x[i2][i2]=c;
    r->x[i1][i2]=-s; r->x[i2][i1]=s;
    return r;
}

TlsI3dVec *TlsI3dMat24Trans(TlsI3dVec *r,TlsI3dMat *A,TlsI3dVec *M) {
    long long v,a[3],b[3];
    b[0]=M->x[0]; b[1]=M->x[1]; b[2]=M->x[2];
    a[0]=A->x[0][0]; a[1]=A->x[0][1]; a[2]=A->x[0][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[0]=v;
    a[0]=A->x[1][0]; a[1]=A->x[1][1]; a[2]=A->x[1][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[1]=v;
    a[0]=A->x[2][0]; a[1]=A->x[2][1]; a[2]=A->x[2][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[2]=v;
    return r;
}

TlsI4dPoint  *TlsI4dMat24Trans(TlsI4dPoint *m,TlsI4dMat *A,TlsI4dPoint *n) {
    long long a[3],b[3],v;
    b[0]=n->x[0]; b[1]=n->x[1]; b[2]=n->x[2];
    a[0]=A->x[0][0]; a[1]=A->x[0][1]; a[2]=A->x[0][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; m->x[0]=v+A->x[0][3];
    a[0]=A->x[1][0]; a[1]=A->x[1][1]; a[2]=A->x[1][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; m->x[1]=v+A->x[1][3];
    a[0]=A->x[2][0]; a[1]=A->x[2][1]; a[2]=A->x[2][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; m->x[2]=v+A->x[2][3];
    m->x[3]=0;
    return m;
}

TlsI3dPoint *TlsI3dPointTrans(TlsI3dPoint *dst,TlsI4dMat *A,TlsI3dPoint *B) {
    long long v, a[3],b[3];
    b[0]=B->x[0]; b[1]=B->x[1]; b[2]=B->x[2];
    a[0]=A->x[0][0]; a[1]=A->x[0][1]; a[2]=A->x[0][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->x[0]=A->x[0][3]+v;
    a[0]=A->x[1][0]; a[1]=A->x[1][1]; a[2]=A->x[1][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->x[1]=A->x[1][3]+v;
    a[0]=A->x[2][0]; a[1]=A->x[2][1]; a[2]=A->x[2][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->x[2]=A->x[2][3]+v;
    return dst;
}
TlsI3dPoint *TlsI3dMatTrans(TlsI3dMat *dst,TlsI4dMat *A,TlsI3dMat *B) {
    long long v,a[3],b[3];
    int i;
    for (i=0;i<3;i++) {
        b[0]=B->x[0][i]; b[1]=B->x[1][i]; b[2]=B->x[2][i];
        a[0]=A->x[0][0]; a[1]=A->x[0][1]; a[2]=A->x[0][2];
        v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->x[0][i]=v;
        a[0]=A->x[1][0]; a[1]=A->x[1][1]; a[2]=A->x[1][2];
        v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->x[1][i]=v;
        a[0]=A->x[2][0]; a[1]=A->x[2][1]; a[2]=A->x[2][2];
        v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; dst->x[2][i]=v;
    }
}
TlsGuiI3dRefer *TlsGuiI3dReferTrans(TlsGuiI3dRefer *dst,TlsI4dMat *A,TlsGuiI3dRefer *C) {
    TlsI3dPointTrans(&dst->O,A,&C->O);
    TlsI3dMatTrans(&dst->Axis,A,&C->Axis);
    return dst;
}
TlsI4dMat *TlsGuiToI3dRefer(TlsI4dMat *r,TlsGuiI3dRefer *src) {
    long long b[3],a[3],v;
    r->x[0][0]=src->Axis.x[0][0]; r->x[0][1]=src->Axis.x[1][0]; r->x[0][2]=src->Axis.x[2][0];
    r->x[1][0]=src->Axis.x[0][1]; r->x[1][1]=src->Axis.x[1][1]; r->x[1][2]=src->Axis.x[2][1];
    r->x[2][0]=src->Axis.x[0][2]; r->x[2][1]=src->Axis.x[1][2]; r->x[2][2]=src->Axis.x[2][2];
    b[0]=-src->O.x[0]; b[1]=-src->O.x[1]; b[2]=-src->O.x[2];
    a[0]=r->x[0][0]; a[1]=r->x[0][1]; a[2]=r->x[0][2];
    v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[0][3]=v;
    a[0]=r->x[1][0]; a[1]=r->x[1][1]; a[2]=r->x[1][2];
    v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[1][3]=v;
    a[0]=r->x[2][0]; a[1]=r->x[2][1]; a[2]=r->x[2][2];
    v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[2][3]=v;
    r->x[3][0]=r->x[3][1]=r->x[3][2]=0; r->x[3][3]=1;
    return r;
}
TlsI4dMat *TlsGuiFromI3dRefer(TlsI4dMat *r,TlsGuiI3dRefer *src) {
    r->x[0][0]=src->Axis.x[0][0]; r->x[0][1]=src->Axis.x[0][1]; r->x[0][2]=src->Axis.x[0][2];
    r->x[1][0]=src->Axis.x[1][0]; r->x[1][1]=src->Axis.x[1][1]; r->x[1][2]=src->Axis.x[1][2];
    r->x[2][0]=src->Axis.x[2][0]; r->x[2][1]=src->Axis.x[2][1]; r->x[2][2]=src->Axis.x[2][2];
    r->x[0][3]=src->O.x[0]; r->x[1][3]=src->O.x[1]; r->x[2][3]=src->O.x[2];
    r->x[3][0]=r->x[3][1]=r->x[3][2]=0; r->x[3][3]=1;
    return r;
}


// Ru = R(u0+u1) = u0+v , with u0=(a.u)a, u1=u-(a.u)a; v=Ru1
// v = cos u1+sin vect(a,u1)
TlsI4dMat *TlsI4dMat24AffRot(TlsI4dMat *r,TlsI3dPoint *O,TlsI3dVec *Axis,int Alpha) {
    int Sin,Cos,Ccos;
    long long sin,cos,ccos,ccosa[3];
    int sq[3],crprod[3],sina[3];
    long long v,a[3],b[3];
    SinCos(&Sin,&Cos,Alpha);
    Ccos = 0x1000000-Cos;
    sin = Sin; cos=Cos; ccos=Ccos;
    a[0]=Axis->x[0]; a[1]=Axis->x[1]; a[2]=Axis->x[2];
    v=(Sin*a[0])>>24; sina[0]=v;
    v=(Sin*a[1])>>24; sina[1]=v;
    v=(Sin*a[2])>>24; sina[2]=v;
    ccosa[0]=(ccos*a[0])>>24; ccosa[1]=(ccos*a[1])>>24; ccosa[2]=(ccos*a[2])>>24;
    v=(ccosa[0]*a[0])>>24; sq[0]=v;
    v=(ccosa[1]*a[1])>>24; sq[1]=v;
    v=(ccosa[2]*a[2])>>24; sq[2]=v;
    v=(ccosa[0]*a[1])>>24; crprod[2]=v;
    v=(ccosa[1]*a[2])>>24; crprod[0]=v;
    v=(ccosa[2]*a[0])>>24; crprod[1]=v;
    r->x[0][0]=sq[0]+cos; r->x[0][1]=crprod[2]-sina[2]; r->x[0][2]=crprod[1]+sina[1];
    r->x[1][0]=crprod[2]+sina[2]; r->x[1][1]=sq[1]+cos; r->x[1][2]=crprod[0]-sina[0];
    r->x[2][0]=crprod[1]-sina[1]; r->x[2][1]=crprod[0]+sina[0]; r->x[2][2]=sq[2]+cos;
    r->x[3][0]=r->x[3][1]=r->x[3][2]=0; r->x[3][3]=1;
    b[0]=O->x[0]; b[1]=O->x[1]; b[2]=O->x[2];
    a[0]=0x1000000-r->x[0][0]; a[1]=r->x[0][1]; a[2]=r->x[0][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[3][0]=v;
    a[0]=r->x[1][0]; a[1]=0x1000000-r->x[1][1]; a[2]=r->x[1][2];
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[3][1]=v;
    a[0]=r->x[2][0]; a[1]=r->x[2][1]; a[2]=0x1000000-r->x[2][2]; 
    v=(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[3][2]=v;
    return r;
}

// BA=I; N=AM+a ; BN+b=BAM+Ba+b  BN-Ba=BAM = M
TlsI4dMat *TlsI4dMatInv(TlsI4dMat *r,TlsI4dMat *A) {
    long long v,a[3],b[3];
    r->x[0][0]=A->x[0][0]; r->x[0][1]=A->x[1][0]; r->x[0][2]=A->x[2][0];
    r->x[1][0]=A->x[0][1]; r->x[1][1]=A->x[1][1]; r->x[1][2]=A->x[2][1];
    r->x[2][0]=A->x[0][2]; r->x[2][1]=A->x[1][2]; r->x[2][2]=A->x[2][2];
    r->x[3][0]=r->x[3][1]=r->x[3][2]=0; r->x[3][3]=1;
    b[0]=-A->x[0][3]; b[1]=-A->x[1][3]; b[2]=-A->x[2][3];
    a[0]=r->x[0][0]; a[1]=r->x[0][1]; a[2]=r->x[0][2];
    v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[0][3]=v;
    a[1]=r->x[1][0]; a[1]=r->x[1][1]; a[2]=r->x[1][2];
    v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[1][3]=v;
    a[2]=r->x[2][0]; a[1]=r->x[2][1]; a[2]=r->x[2][2];
    v = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24; r->x[2][3]=v;
    return r;
}


/*_____________________________________________________________
 |
 |
 |_____________________________________________________________
*/

static void GreyTextureTransfert(TlsGuiTextureArea *dst,TlsGuiTextureArea *src) {
    int dy,dx,xs,ys,xd,yd;
    TlsGuiTexture *stxt,*dtxt;
    TlsRGBA c;
    stxt = src->Texture;
    dtxt = dst->Texture;
    xs = src->Pic.b.x[0]; ys = src->Pic.b.x[1];
    xd = dst->Pic.b.x[0]; yd = dst->Pic.b.x[1];
    dy = dst->Pic.e.x[1] - yd;
    while (dy>0) {
        dy--;
        dx = dst->Pic.e.x[0] - xd;
        while(dx>0) {
            dx--;
            x_(stxt->Format,GetColor,&c,stxt,xs+dx,ys+dy);
            x_(dtxt->Format,SetColor,c,dtxt,xd+dx,yd+dy);
        }
    }
}

void ColorTextureTransfert(TlsGuiTextureArea *dst,TlsGuiTextureArea *src){
    int dy,dx,xs,ys,xd,yd,nb,xnb;
    TlsGuiTexture *stxt,*dtxt;
    stxt = src->Texture;
    dtxt = dst->Texture;
    xs = src->Pic.b.x[0]; ys = src->Pic.b.x[1];
    xd = dst->Pic.b.x[0]; yd = dst->Pic.b.x[1];
    dx = dst->Pic.e.x[0] - xd;
    dy = dst->Pic.e.x[1] - yd;
    nb = dy;
    while (nb) {
        TlsGuiTextureRowPack *Pack;
        TlsGuiTextureRowUnpack *Unpack;
        rOpen {
            xnb = dx;
            Pack = x_(dtxt->Format,Pack,dtxt,xd,yd);
            Unpack = x_(stxt->Format,Unpack,stxt,xs,ys,dx);
            while (xnb) {
                TlsRGBA *data;
                int i;
                data = x_(Unpack,Next,&i);
                xnb-=i;
                x_(Pack,Next,data,i);
            }
            ys++; yd++; nb--;
        } rClose;
    }
}
void TlsGuiTextureTransfert(TlsGuiTextureArea *dst,TlsGuiTextureArea *src){
    if (dst->Texture->Format->bpp<=8) {
        GreyTextureTransfert(dst,src);
    } else {
        ColorTextureTransfert(dst,src);
    }
}
TlsGuiTexture *TlsGuiTextureNew(TlsGuiTextureFormat *Format,TlsRGBA *Palette,int dx,int dy) {
    #define TexturePitchAlign 16
    int pitch;
    TlsGuiTexture *r;
    rPush(r);
    r->Format = Format;
    pitch = (Format->bpp*dx)>>3;
    pitch = (pitch+(TexturePitchAlign-1))&(-TexturePitchAlign);
    r->Palette = Palette;
    r->w = dx; r->h = dy;
    r->pitch = pitch;
    rnPush(r->data,dy*pitch);
    return r;
}

/*_____________________________________________________________
 |
 | Bitmap MSB
 |_____________________________________________________________
*/

typedef struct {
     TlsGuiTextureFormat TlsGuiTextureFormat;
     struct TlsGuiTextureRowUnpack RowUnpack;
     struct TlsGuiTextureRowPack RowPack;
     int OffsetMsk,OffsetShft;
} IndexTextureFormat;

typedef struct {
    TlsGuiTextureRowUnpack TlsGuiTextureRowUnpack;
    TlsRGBA dst[32];
    TlsRGBA *color;
    unsigned char *src;
    int offset;
    int nb;
} BitmapUnpackCtx;

static TlsRGBA *BitmapUnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,i,j,ofs;
    unsigned char *p,d;
    TlsRGBA *q,*qe,color[2];
    TlsGetThat(BitmapUnpackCtx,TlsGuiTextureRowUnpack);
    color[0]=that->color[0]; color[1]=that->color[1];
    n=0; p=that->src; i=TlsMin(that->nb,32); q=that->dst; ofs=that->offset;
    if ((ofs)||(i<8)) {
        j=8-ofs; j=TlsMin(j,i);
        i-=j; n+=j; q+=j; qe=q; that->offset=(ofs+j)&7;
        d=*p++; d=d>>ofs;
        while (j>0) {  qe--; j--; *qe=color[d&1]; d=d>>1; }
        if ((i>0)&&(i<8)) {
            d=*p; n+=i; q+=8; qe=q;
            while (i>0) { qe--; *qe=color[d&1]; d=d>>1; i--; }
        }
    }
    while (i>7) {
        d=*p++; n+=8; i-=8; q+=8; qe=q;
        for (j=0;j<8;j++) {--qe; *qe=color[d&1]; d=d>>1; }
    }
    that->src=p; that->nb-=n; *nb=n;
    return that->dst+0;
}

static TlsRGBA *BitmapLSBUnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,i,j,ofs;
    unsigned char *p,d;
    TlsRGBA *q,color[2];
    TlsGetThat(BitmapUnpackCtx,TlsGuiTextureRowUnpack);
    n=0; p=that->src; q=that->dst; i=TlsMin(that->nb,32); ofs=that->offset;
    color[0]=that->color[0]; color[1]=that->color[1];
    if ((ofs)||(i<8)) {
        j=8-ofs; j=TlsMin(j,i);
        that->offset=(ofs+j)&7;
        i-=j; n+=j; d=*p++; d=d>>ofs;
        while (j>0) {  *q++=color[d&1]; d=d>>1; j--; }
        if ((i>0)&&(i<8)) {
            d=*p; n+=i;
            while (i>0) { *q++=color[d&1]; d=d>>1; i--; }
        }
    }
    while (i>7) {
        d=*p++; n+=8; i-=8;
        for (j=0;j<8;j++) { *q++=color[d&1]; d=d>>1; }
    }
    that->src=p; that->nb-=n; *nb = n;
    return that->dst+0;
}
static TlsGuiTextureRowUnpack *IndexRowUnpack(
    TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb
){
    BitmapUnpackCtx *r;
    TlsGetThat(IndexTextureFormat,TlsGuiTextureFormat);
    rPush(r);
    r->TlsGuiTextureRowUnpack.Static = &that->RowUnpack;
    r->color = src->Palette;
    if (r->color==&TlsGuiPaletteGreyScale) {
        r->color=TlsGuiPaletteBitmap(src->Format->bpp,(0==0)); 
    }
    r->src = (src->data+y*src->pitch)+(x>>(that->OffsetShft));
    r->offset = x&that->OffsetMsk;
    r->nb=nb;
    return &r->TlsGuiTextureRowUnpack;
}

typedef struct {
    TlsGuiTextureRowPack TlsGuiTextureRowPack;
    unsigned char *dst;
    int Cmpnt,CmpntMask;
    int offset;
} BitmapPackCtx;
static void BitmapPackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    unsigned char c0,c,msk,CmpntMask;
    unsigned char *p,*q;
    int i,inc,ofs;
    TlsGetThat(BitmapPackCtx,TlsGuiTextureRowPack);
    i=nb; q=that->dst; p=(unsigned char *)src; p+=that->Cmpnt; ofs=that->offset;
    CmpntMask=that->CmpntMask; that->offset=(ofs+nb)&7;
    if (ofs) {
        inc=8-ofs; inc=TlsMin(inc,i); i-=inc;
        c0=0; msk=~((0xff<<(8-inc))>>ofs); c=(*q)&msk; 
        while (inc) { c0=c0|(((*p&CmpntMask)!=0)<<(8-inc)); p+=sizeof(TlsRGBA); inc--; }
        *q++=c|(c0>>ofs);
    }
    while (i>=8) {
        inc=0; c=0;
        while (inc<8) { c=c|(((*p&CmpntMask)!=0)<<inc); p+=sizeof(TlsRGBA); inc++; }
        *q++=c;
    }
    if (i>0) {
        c0 = 0; msk = (0xff>>i); c=(*q)&msk; 
        while (i) {
            c0=c0|(((*p&CmpntMask)!=0)<<(8-i));
            p+=sizeof(TlsRGBA); i--;
        }
        *q = c|c0;
    }
    that->dst=q;
}

static void BitmapLSBPackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    unsigned char c0,c,msk,CmpntMask;
    unsigned char *p,*q;
    int i,inc,ofs;
    TlsGetThat(BitmapPackCtx,TlsGuiTextureRowPack);
    p=(unsigned char *)src; p+=that->Cmpnt;
    i=nb; q=that->dst; ofs=that->offset;
    CmpntMask=that->CmpntMask; that->offset=(ofs+nb)&7;
    if (ofs) {
        inc=8-ofs; inc=TlsMin(inc,i); i-=inc;
        c0=0; msk=~((0xff>>(8-inc))<<ofs); c=(*q)&msk; 
        while (inc) { c0=(c0<<1)|((*p&CmpntMask)!=0); p+=sizeof(TlsRGBA); inc--; }
        *q++=c|(c0<<ofs);
    }
    while (i>=8) {
        inc = 8;
        while (inc) { c=(c<<1)|((*p&CmpntMask)!=0); p+=sizeof(TlsRGBA); inc--; }
        *q++=c;
    }
    if (i>0) {
        c0=0; msk=(0xff<<i); c=(*q)&msk; 
        while (i) {
            c0=(c0<<1)|((*p&CmpntMask)!=0);
            p+=sizeof(TlsRGBA); i--;
        }
        *q = c|c0;
    }
    that->dst=q;
}
static TlsGuiTextureRowPack *BitmapRowPack(
    TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y
){
    /**/
    BitmapPackCtx *r;
    TlsGetThat(IndexTextureFormat,TlsGuiTextureFormat);
    rPush(r);
    r->TlsGuiTextureRowPack.Static = &that->RowPack;
    r->dst = dst->data+(dst->pitch*y)+(x>>that->OffsetShft);
    r->offset = x&that->OffsetMsk;
    if (dst->Palette[0].A!=dst->Palette[1].A) {
        r->Cmpnt = o_(TlsRGBA,A);
        r->CmpntMask = dst->Palette[1].A;
    } else {
        r->Cmpnt = o_(TlsRGBA,R);
        r->CmpntMask = dst->Palette[1].R;
    }
    return &r->TlsGuiTextureRowPack;
}

static void BitmapGetColor(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y) {
    static TlsRGBA c[2]={{0xff,0xff,0xff,0},{0xff,0xff,0xff,0xff}}; 
    unsigned char *p;
    p = txt->data+txt->pitch*y+(x>>3);
    *r = c[((*p)>>(7-(x&7)))&1];
}
static void BitmapSetColor(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,m;
    p = txt->data+txt->pitch*y+(x>>3);
    m = 0x80>>(x&7); *p=((*p)&(~m))|(m&c.A);
}
static void BitmapLSBGetColor(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y) {
    static TlsRGBA c[2]={{0xff,0xff,0xff,0},{0xff,0xff,0xff,0xff}}; 
    unsigned char *p;
    p = txt->data+txt->pitch*y+(x>>3);
    *r = c[((*p)>>(x&7))&1];
}
static void BitmapLSBSetColor(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,m;
    p = txt->data+txt->pitch*y+(x>>3);
    m = 1<<(x&7); *p=((*p)&(~m))|(m&c.A);
}
TlsGuiTextureFormat *TlsGuiTxtrFormatBitmap(void) {
    static struct TlsGuiTextureFormat Static = { IndexRowUnpack,BitmapRowPack,BitmapGetColor,BitmapSetColor };
    static IndexTextureFormat r = {
        .TlsGuiTextureFormat = {&Static,1,1,{0,0,0,0},{0,0,0,0}},
        .RowUnpack={BitmapUnpackNext},.RowPack={BitmapPackNext},
        .OffsetMsk=7,.OffsetShft=3
    };
    return &r.TlsGuiTextureFormat;
}

TlsGuiTextureFormat *TlsGuiTxtrFormatBitmapLSB(void) {
    static struct TlsGuiTextureFormat Static = { IndexRowUnpack,BitmapRowPack,BitmapLSBGetColor,BitmapLSBSetColor };
    static IndexTextureFormat r = {
         .TlsGuiTextureFormat = {&Static,1,1,{0,0,0,0},{0,0,0,0}},
         .RowUnpack={BitmapLSBUnpackNext},.RowPack={BitmapLSBPackNext},
         .OffsetMsk=7,.OffsetShft=3
    };
    return &r.TlsGuiTextureFormat;
}

/*_____________________________________________________________
 |
 | Index 4 to true color
 |_____________________________________________________________
*/

static TlsRGBA *Index4UnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,i,ofs;
    unsigned char *p,d,c;
    TlsRGBA *q,*palette;
    TlsGetThat(BitmapUnpackCtx,TlsGuiTextureRowUnpack);
    n=0; p=that->src; i=TlsMin(that->nb,32); q=that->dst; ofs=that->offset; palette=that->color;
    if ((ofs&&(i>0))||(i==1)) {
        i--; n++; that->offset=(ofs+1)&2;
        d=*p++; c=(d>>(ofs<<2)&0xf); *q++=palette[c];
    }
    n+=(i&(-2));
    while (i>1) { d=*p++; i-=2; *q++=palette[d&0xf]; *q++=palette[d>>4]; }
    that->src=p; that->nb-=n; *nb=n;
    return that->dst+0;
}
static TlsRGBA *Index4LSBUnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,i,ofs;
    unsigned char *p,d,c;
    TlsRGBA *q,*palette;
    TlsGetThat(BitmapUnpackCtx,TlsGuiTextureRowUnpack);
    n=0; p=that->src; i=TlsMin(that->nb,32); q=that->dst; ofs=that->offset; palette=that->color;
    if ((ofs&&(i>0))||(i==1)) {
        i--; n++; that->offset=(ofs+1)&2;
        d=*p++; c=(d>>((1-ofs)<<2)&0xf); *q++=palette[c];
    }
    n+=(i&(-2));
    while (i>1) { d=*p++; i-=2; *q++=palette[d>>4]; *q++=palette[d&0xf]; }
    that->src=p; that->nb-=n; *nb=n;
    return that->dst+0;
}

static void Index4PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    TlsGetThat(BitmapPackCtx,TlsGuiTextureRowPack);
    /* TODO */
    /* note that true color to index is a sampling problem outside the scope of most apps,
    and might require its own library. */
}

static void Index4GetColor(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,a;
    TlsRGBA c={0xff,0xff,0xff,0};
    p = txt->data+txt->pitch*y+(x>>1);
    a = (*p>>((1-(x&1))<<2))&0xf; c.A=a|(a<<4); *r = c;
}
static void Index4SetColor(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,m;
    p = txt->data+txt->pitch*y+(x>>1);
    m = 0xf0>>((x&1)<<2); *p=((*p)&(~m))|(m&c.A);
}
static void Index4LSBGetColor(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,a;
    TlsRGBA c={0xff,0xff,0xff,0};
    p = txt->data+txt->pitch*y+(x>>1);
    a = (*p>>((x&1)<<2))&0xf; c.A=a|(a<<4); *r = c;
}
static void Index4LSBSetColor(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,m;
    p = txt->data+txt->pitch*y+(x>>1);
    m = 0xf<<((x&1)<<2); *p=((*p)&(~m))|(m&c.A);
}
TlsGuiTextureFormat *TlsGuiTxtrFormatIndex4(void) {
    static struct TlsGuiTextureFormat Static = { IndexRowUnpack,BitmapRowPack,Index4GetColor,Index4SetColor };
    static IndexTextureFormat r = {
        .TlsGuiTextureFormat = {&Static,4,1,{0,0,0,0},{0,0,0,0}},
        .RowUnpack={Index4UnpackNext},.RowPack={Index4PackNext},
        .OffsetMsk=1,.OffsetShft=1
    };
    return &r.TlsGuiTextureFormat;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatIndex4LSB(void) {
    static struct TlsGuiTextureFormat Static = { IndexRowUnpack,BitmapRowPack,Index4LSBGetColor,Index4LSBSetColor };
    static IndexTextureFormat r = {
        .TlsGuiTextureFormat = {&Static,4,1,{0,0,0,0},{0,0,0,0}},
        .RowUnpack={Index4LSBUnpackNext},.RowPack={Index4PackNext},
        .OffsetMsk=1,.OffsetShft=1
    };
    return &r.TlsGuiTextureFormat;
}


/*_____________________________________________________________
 |
 | Index 8 to true color
 |_____________________________________________________________
*/

typedef struct {
    TlsGuiTextureRowUnpack TlsGuiTextureRowUnpack;
    TlsRGBA dst[32];
    TlsRGBA *Palette;
    unsigned char *src;
    int nb;
} Index8UnpackCtx;

static TlsRGBA *Index8UnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,ri,gi,bi,ai;
    unsigned char *p;
    TlsRGBA *q,*clr;
    TlsGetThat(Index8UnpackCtx,TlsGuiTextureRowUnpack);
    n = TlsMin(32,that->nb);
    clr = that->Palette;
    p = that->src; q = that->dst;
    *nb = n;
    that->nb -= n; that->src +=n;
    while(n) {
        *q++ = clr[*p++];
        n--;
    }
    return that->dst+0;
}
static TlsGuiTextureRowUnpack *Index8RowUnpack(
    TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb
){
    Index8UnpackCtx *r;
    static struct TlsGuiTextureRowUnpack Static = {Index8UnpackNext};
    rPush(r);
    r->TlsGuiTextureRowUnpack.Static = &Static;
    r->Palette = src->Palette;
    r->src = (src->data+(src->pitch*y))+x;
    r->nb = nb;
    return &r->TlsGuiTextureRowUnpack;
}

typedef struct {
    TlsGuiTextureRowPack TlsGuiTextureRowPack;
    TlsRGBA *Palette;
    unsigned char *dst;
} Index8PackCtx;
static void Index8PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    /* TODO */
    /* note that true color to index is a sampling problem outside the scope of most apps,
    and might require its own library. */
    unsigned char *q;
    TlsGetThat(Index8PackCtx,TlsGuiTextureRowPack);
    that->dst+=nb;
}
static TlsGuiTextureRowPack *Index8RowPack(TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y) {
    Index8PackCtx *r;
    static struct TlsGuiTextureRowPack Static = {Index8PackNext};
    rPush(r);
    r->TlsGuiTextureRowPack.Static = &Static;
    r->dst = dst->data+(dst->pitch*y)+x;
    r->Palette = dst->Palette;
    return &r->TlsGuiTextureRowPack;
}

static void Index8GetColor(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p;
    TlsRGBA c={0xff,0xff,0xff,0};
    p = txt->data+txt->pitch*y+x; c.A=*p; *r = c;
}
static void Index8SetColor(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y) {
    unsigned char *p,m;
    p = txt->data+txt->pitch*y+x; *p=c.A; 
}


TlsGuiTextureFormat *TlsGuiTxtrFormatIndex8(void) {
    static struct TlsGuiTextureFormat Static = {
        Index8RowUnpack,Index8RowPack,Index8GetColor,Index8SetColor
    };
    static TlsGuiTextureFormat r = {
        &Static,8,1,{0,0,0,0},{0,0,0,0}
    };
    return &r;
}

/*_____________________________________________________________
 |
 | RGB 565, BGR 565
 |_____________________________________________________________
*/

static void TrueColorGetColor(TlsGuiTextureFormat *this,TlsRGBA *r,TlsGuiTexture *txt,int x,int y){
    rOpen{
        TlsRGBA *p;
        int i;
        TlsGuiTextureRowUnpack *unpck=x_(this,Unpack,txt,x,y,1);
        p=x_(unpck,Next,&i);
        *r=*p;
    }rClose
}
static void TrueColorSetColor(TlsGuiTextureFormat *this,TlsRGBA c,TlsGuiTexture *txt,int x,int y){
    rOpen{
        TlsGuiTextureRowPack *pck=x_(this,Pack,txt,x,y);
        x_(pck,Next,&c,1);
    }rClose
}

typedef struct {
     TlsGuiTextureFormat TlsGuiTextureFormat;
     struct TlsGuiTextureRowUnpack *RowUnpack;
     struct TlsGuiTextureRowPack *RowPack;
} Texture16Format;

typedef struct {
    TlsGuiTextureRowUnpack TlsGuiTextureRowUnpack;
    TlsRGBA dst[32];
    int Key;
    unsigned short *src;
    int nb;
    struct RGBAi {unsigned char c[4];} RGBAi;
} RGB565UnpackCtx;
static TlsRGBA *RGB565UnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,Key;
    struct RGBAi RGBAi;
    unsigned short *p;
    TlsRGBA *q,color;
    TlsGetThat(RGB565UnpackCtx,TlsGuiTextureRowUnpack);
    n=TlsMin(32,that->nb); RGBAi=that->RGBAi;
    *nb=n; Key=that->Key; that->nb-=n; p=that->src; q=that->dst;
    color.A=0xff;
    while (n>0) {
        unsigned int clr;
        clr = *p;
        if (clr==Key) { static TlsRGBA noColor={0,0,0,0}; *q=noColor; } else {
            unsigned int c;
            unsigned char cmpnt[4];
            c=clr&0x1f;      cmpnt[0]=(c<<3)|(c>>2);
            c=(clr>>5)&0x3f; cmpnt[1]=(c<<2)|(c>>4);
            c=clr>>11;       cmpnt[2]=(c<<3)|(c>>2);
            color.R = cmpnt[RGBAi.c[0]];
            color.G = cmpnt[RGBAi.c[1]];
            color.B = cmpnt[RGBAi.c[2]]; 
            *q=color; 
        }
        n--; p++; q++;
    }
    that->src=p;
    return that->dst+0;
}
static TlsRGBA *RGBA4444UnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,Key;
    struct RGBAi RGBAi;
    unsigned short *p;
    TlsRGBA *q,color;
    TlsGetThat(RGB565UnpackCtx,TlsGuiTextureRowUnpack);
    n=TlsMin(32,that->nb); RGBAi=that->RGBAi;
    *nb=n; Key=that->Key; that->nb-=n; p=that->src; q=that->dst;
    while (n>0) {
        unsigned int clr;
        clr = *p;
        if (clr==Key) { static TlsRGBA noColor={0,0,0,0}; *q=noColor; } else {
            unsigned int c;
            unsigned char cmpnt[4];
            c=clr&0xf;       cmpnt[0]=(c<<4)|c;
            c=(clr>>4)&0xf;  cmpnt[1]=(c<<4)|c;
            c=(clr>>8)&0xf;  cmpnt[2]=(c<<4)|c;
            c=(clr>>12)&0xf; cmpnt[3]=(c<<4)|c;
            color.R = cmpnt[RGBAi.c[0]];
            color.G = cmpnt[RGBAi.c[1]];
            color.B = cmpnt[RGBAi.c[2]]; 
            color.A = cmpnt[RGBAi.c[3]];
            *q=color; 
        }
        n--; p++; q++;
    }
    that->src=p;
    return that->dst+0;
}
static TlsGuiTextureRowUnpack *RGB16RowUnpack(TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb){
    RGB565UnpackCtx *r;
    TlsGetThat(Texture16Format,TlsGuiTextureFormat);
    rPush(r);
    r->TlsGuiTextureRowUnpack.Static = that->RowUnpack;
    r->src=(unsigned short *)(src->data+(src->pitch*y));
    r->src+=x; r->nb=nb; r->Key=-1;
    r->RGBAi.c[0]=this->CmpntIdx.r; r->RGBAi.c[1]=this->CmpntIdx.g; r->RGBAi.c[2]=this->CmpntIdx.b;
    if (src->Palette==&TlsGuiPaletteColorKey) { r->Key = 1; }
    return &r->TlsGuiTextureRowUnpack;
}
typedef struct {
    TlsGuiTextureRowPack TlsGuiTextureRowPack;
    unsigned short *dst;
    struct RGBAi RGBAi;
} RGB565PackCtx; 
static void RGB565PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    unsigned short *q;
    struct RGBAi RGBAi;
    TlsGetThat(RGB565PackCtx,TlsGuiTextureRowPack);
    q=that->dst;
    RGBAi=that->RGBAi;
    while (nb>0) {
        unsigned char cmpnt[4];
        cmpnt[RGBAi.c[0]]=src->R; cmpnt[RGBAi.c[1]]=src->G; cmpnt[RGBAi.c[2]]=src->B;
        *q = (cmpnt[0]>>3)|((cmpnt[1]&0xfc)<<3)|((cmpnt[2]&0xf8)<<8);
        q++; nb--; src++;
    }
    that->dst=q;
}
static void RGBA4444PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    unsigned short *q;
    struct RGBAi RGBAi;
    TlsGetThat(RGB565PackCtx,TlsGuiTextureRowPack);
    q=that->dst;
    RGBAi=that->RGBAi;
    while (nb>0) {
        unsigned char cmpnt[4];
        cmpnt[RGBAi.c[0]]=src->R; cmpnt[RGBAi.c[1]]=src->G; 
        cmpnt[RGBAi.c[2]]=src->B; cmpnt[RGBAi.c[3]]=src->A;
        *q = (cmpnt[0]>>4)|((cmpnt[1]&0xf)<<4)|((cmpnt[2]&0xf)<<8)|((cmpnt[3]&0xf)<<12);
        q++; nb--; src++;
    }
    that->dst=q;
}
static TlsGuiTextureRowPack *RGB16RowPack(TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y) {
    RGB565PackCtx *r;
    TlsGetThat(Texture16Format,TlsGuiTextureFormat);
    rPush(r);
    r->TlsGuiTextureRowPack.Static = that->RowPack;
    r->dst = (unsigned short *)(dst->data+(dst->pitch*y));
    r->dst+=x;
    r->RGBAi.c[0]=this->CmpntIdx.r; r->RGBAi.c[1]=this->CmpntIdx.g; r->RGBAi.c[2]=this->CmpntIdx.b;
    return &r->TlsGuiTextureRowPack;
}

TlsGuiTextureFormat *TlsGuiTxtrFormatRGB565(void) {
    static struct TlsGuiTextureFormat Static = { RGB16RowUnpack,RGB16RowPack,TrueColorGetColor,TrueColorSetColor};
    static struct TlsGuiTextureRowUnpack RowUnpack =  {RGB565UnpackNext};
    static struct TlsGuiTextureRowPack RowPack = {RGB565PackNext};
    static Texture16Format r= {
    .RowUnpack = &RowUnpack, .RowPack = &RowPack, .TlsGuiTextureFormat = {
        .Static = &Static, .bpp=16, .CmpntNb=3,
        .bpcmpnt = {5,6,5,0}, .CmpntIdx={0,1,2,0}
    }};
    return &r.TlsGuiTextureFormat;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatBGR565(void) {
    static struct TlsGuiTextureFormat Static = { RGB16RowUnpack,RGB16RowPack,TrueColorGetColor,TrueColorSetColor };
    static struct TlsGuiTextureRowUnpack RowUnpack =  {RGB565UnpackNext};
    static struct TlsGuiTextureRowPack RowPack = {RGB565PackNext};
    static Texture16Format r= {
    .RowUnpack = &RowUnpack, .RowPack = &RowPack, .TlsGuiTextureFormat = {
        .Static = &Static, .bpp=16, .CmpntNb=3,
        .bpcmpnt = {5,6,5,0}, .CmpntIdx={2,1,0,0}
    }};
    return &r.TlsGuiTextureFormat;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatRGBA4444(void) {
    static struct TlsGuiTextureFormat Static = { RGB16RowUnpack,RGB16RowPack,TrueColorGetColor,TrueColorSetColor };
    static struct TlsGuiTextureRowUnpack RowUnpack =  {RGBA4444UnpackNext};
    static struct TlsGuiTextureRowPack RowPack = {RGBA4444PackNext};
    static Texture16Format r= {
    .RowUnpack = &RowUnpack, .RowPack = &RowPack, .TlsGuiTextureFormat = {
        .Static = &Static, .bpp=16, .CmpntNb=4,
        .bpcmpnt = {4,4,4,4}, .CmpntIdx={0,1,2,3}
    }};
    return &r.TlsGuiTextureFormat;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatARGB4444(void) {
    static struct TlsGuiTextureFormat Static = { RGB16RowUnpack,RGB16RowPack,TrueColorGetColor,TrueColorSetColor };
    static struct TlsGuiTextureRowUnpack RowUnpack =  {RGBA4444UnpackNext};
    static struct TlsGuiTextureRowPack RowPack = {RGBA4444PackNext};
    static Texture16Format r= {
    .RowUnpack = &RowUnpack, .RowPack = &RowPack, .TlsGuiTextureFormat = {
        .Static = &Static, .bpp=16, .CmpntNb=4,
        .bpcmpnt = {4,4,4,4}, .CmpntIdx={3,0,1,2}
    }};
    return &r.TlsGuiTextureFormat;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatBGRA4444(void) {
    static struct TlsGuiTextureFormat Static = { RGB16RowUnpack,RGB16RowPack,TrueColorGetColor,TrueColorSetColor };
    static struct TlsGuiTextureRowUnpack RowUnpack =  {RGBA4444UnpackNext};
    static struct TlsGuiTextureRowPack RowPack = {RGBA4444PackNext};
    static Texture16Format r= {
    .RowUnpack = &RowUnpack, .RowPack = &RowPack, .TlsGuiTextureFormat = {
        .Static = &Static, .bpp=16, .CmpntNb=4,
        .bpcmpnt = {4,4,4,4}, .CmpntIdx={2,1,0,3}
    }};
    return &r.TlsGuiTextureFormat;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatABGR4444(void) {
    static struct TlsGuiTextureFormat Static = { RGB16RowUnpack,RGB16RowPack,TrueColorGetColor,TrueColorSetColor };
    static struct TlsGuiTextureRowUnpack RowUnpack =  {RGBA4444UnpackNext};
    static struct TlsGuiTextureRowPack RowPack = {RGBA4444PackNext};
    static Texture16Format r= {
    .RowUnpack = &RowUnpack, .RowPack = &RowPack, .TlsGuiTextureFormat = {
        .Static = &Static, .bpp=16, .CmpntNb=4,
        .bpcmpnt = {4,4,4,4}, .CmpntIdx={3,2,1,0}
    }};
    return &r.TlsGuiTextureFormat;
}

/*_____________________________________________________________
 |
 | 24 bits true color
 |_____________________________________________________________
*/

union rgbaVal {
    TlsRGBA color;
    unsigned long val;
};
typedef struct {
    TlsGuiTextureRowUnpack TlsGuiTextureRowUnpack;
    unsigned char *src;
    union rgbaVal Key,dst[32];
    int nb;
    struct RGBAi RGBAi;
} RGB888UnpackCtx;
static TlsRGBA *RGB888UnpackNext(TlsGuiTextureRowUnpack *this,int *Nb) {
    int i,nb;
    struct RGBAi RGBAi;
    unsigned long nz;
    unsigned char *p;
    union rgbaVal *q;
    TlsGetThat(RGB888UnpackCtx,TlsGuiTextureRowUnpack);
    RGBAi = that->RGBAi;
    i = that->nb;
    nb = TlsMin(32,i);
    *Nb = nb;
    p = that->src; q = that->dst;
    nz = that->Key.val;
    for (i=0;i<nb;i++) {
         q->color.R=p[RGBAi.c[0]]; q->color.G=p[RGBAi.c[1]]; q->color.B=p[RGBAi.c[2]]; 
         q->color.A=0xff; p+=3;
         if (q->val==nz) {q->color.A=0x0;}
         q++;
    }
    that->nb-=nb; that->src = p;
    return &that->dst[0].color;
}
static TlsGuiTextureRowUnpack *RGB888RowUnpack(TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb){
    RGB888UnpackCtx *r;
    static struct TlsGuiTextureRowUnpack Static = {RGB888UnpackNext};
    rPush(r);
    r->TlsGuiTextureRowUnpack.Static = &Static;
    r->src = (src->data+(src->pitch*y))+((x<<1)+x);
    r->nb = nb;
    r->Key.val = 0;
    r->RGBAi.c[0]=this->CmpntIdx.r; r->RGBAi.c[1]=this->CmpntIdx.g; r->RGBAi.c[2]=this->CmpntIdx.b;
    if (src->Palette==&TlsGuiPaletteColorKey) {
        static TlsRGBA colorKey = {1,0,0,0xff};
        r->Key.color = colorKey;
    }
    return &r->TlsGuiTextureRowUnpack;
}
typedef struct {
    TlsGuiTextureRowPack TlsGuiTextureRowPack;
    unsigned char *dst;
    struct RGBAi RGBAi;
} RGB888PackCtx;
static void RGB888PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    unsigned char *q;
    struct RGBAi ci;
    TlsGetThat(RGB888PackCtx,TlsGuiTextureRowPack);
    ci = that->RGBAi;
    q = that->dst;
    while (nb>0) {
        if (!src->A) {
            *q++=1; *q++=0; *q++=0;
        } else {
            q[ci.c[0]]=src->R; q[ci.c[1]]=src->G; q[ci.c[2]]=src->B; q+=3;
        }
        nb--; src++;
    }
    that->dst=q;
}
static TlsGuiTextureRowPack *RGB888RowPack(TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y) {
    RGB888PackCtx *r;
    static struct TlsGuiTextureRowPack Static = {RGB888PackNext};
    rPush(r);
    r->TlsGuiTextureRowPack.Static = &Static;
    r->RGBAi.c[0]=this->CmpntIdx.r; r->RGBAi.c[1]=this->CmpntIdx.g; r->RGBAi.c[2]=this->CmpntIdx.b;
    r->dst = (dst->data+(dst->pitch*y))+((x<<1)+x);
    return &r->TlsGuiTextureRowPack;
}

TlsGuiTextureFormat *TlsGuiTxtrFormatRGB888(void) {
    static struct TlsGuiTextureFormat Static = {
        RGB888RowUnpack,RGB888RowPack,TrueColorGetColor,TrueColorSetColor
    };
    static TlsGuiTextureFormat r = {
        .Static = &Static,
        .bpp = 24,.CmpntNb=3,.bpcmpnt={8,8,8,0},.CmpntIdx={0,1,2,0}
    };
    return &r;
}

TlsGuiTextureFormat *TlsGuiTxtrFormatBGR888(void) {
    static struct TlsGuiTextureFormat Static = {
        RGB888RowUnpack,RGB888RowPack,TrueColorGetColor,TrueColorSetColor
    };
    static TlsGuiTextureFormat r = {
        .Static = &Static,
        .bpp = 24,.CmpntNb=3,.bpcmpnt={8,8,8,0},.CmpntIdx={2,1,0,0}
    };
    return &r;
}

/*_____________________________________________________________
 |
 | 32 bits RGBA, BGRA, ARGB and ABGR
 |_____________________________________________________________
*/

typedef struct {
    TlsGuiTextureRowUnpack TlsGuiTextureRowUnpack;
    TlsRGBA *src,dst[32];
    struct RGBAi RGBAi;
    int nb;
} RGBA32UnpackCtx;
static TlsRGBA *RGBA32UnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    int n,i;
    struct RGBAi cI;
    TlsRGBA *q;
    unsigned char *p;
    TlsGetThat(RGBA32UnpackCtx,TlsGuiTextureRowUnpack);
    p = (unsigned char *)that->src;
    q = that->dst;
    i = n = TlsMin(that->nb,32);
    cI = that->RGBAi;
    while (i) {
        q->R=p[cI.c[0]]; q->G=p[cI.c[1]]; q->B=p[cI.c[2]]; q->A=p[cI.c[3]];
        q++; p+=4; i--;
    }
    *nb = n;
    that->src+=n; that->nb-=n;
    return that->dst+0;
}
static TlsGuiTextureRowUnpack *RGBA32RowUnpack(TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb){
    RGBA32UnpackCtx *r;
    TlsRGBA *row;
    static struct TlsGuiTextureRowUnpack Static = {RGBA32UnpackNext};
    rPush(r);
    r->TlsGuiTextureRowUnpack.Static = &Static;
    row = (TlsRGBA *)(src->data+(src->pitch*y));
    r->RGBAi.c[0] = this->CmpntIdx.r;
    r->RGBAi.c[1] = this->CmpntIdx.g;
    r->RGBAi.c[2] = this->CmpntIdx.b;
    r->RGBAi.c[3] = this->CmpntIdx.a;
    r->src = row+x;
    r->nb = nb;
    return &r->TlsGuiTextureRowUnpack;
}

typedef struct {
    TlsGuiTextureRowPack TlsGuiTextureRowPack;
    TlsRGBA *dst;
    struct RGBAi RGBAi;
} RGBA32PackCtx;
static void RGBA32PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    TlsRGBA *p;
    unsigned char *q;
    int i=nb;
    struct RGBAi cI;
    TlsGetThat(RGBA32PackCtx,TlsGuiTextureRowPack);
    cI=that->RGBAi; p=src; q=(unsigned char *)that->dst;
    while (i) {
        q[cI.c[0]]=p->R; q[cI.c[1]]=p->G; q[cI.c[2]]=p->B; q[cI.c[3]]=p->A;
        p++; q+=4; i--;
    }
    that->dst = that->dst+nb;
}
static TlsGuiTextureRowPack *RGBA32RowPack(TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y) {
    RGBA32PackCtx *r;
    TlsRGBA *Dst;
    static struct TlsGuiTextureRowPack Static = {RGBA32PackNext};
    rPush(r);
    r->TlsGuiTextureRowPack.Static = &Static;
    Dst = (TlsRGBA *)(dst->data+(dst->pitch*y));
    r->dst = Dst+x;
    r->RGBAi.c[0] = this->CmpntIdx.r;
    r->RGBAi.c[1] = this->CmpntIdx.g;
    r->RGBAi.c[2] = this->CmpntIdx.b;
    r->RGBAi.c[3] = this->CmpntIdx.a;
    return &r->TlsGuiTextureRowPack;
}

TlsGuiTextureFormat *TlsGuiTxtrFormatRGBA8888(void) {
    static struct TlsGuiTextureFormat Static = {
        RGBA32RowUnpack,RGBA32RowPack,TrueColorGetColor,TrueColorSetColor
    };
    static TlsGuiTextureFormat r = {
        .Static=&Static,.bpp=32,.CmpntNb=4,
        .bpcmpnt={8,8,8,8},.CmpntIdx={0,1,2,3}
    };
    return &r;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatBGRA8888(void) {
    static struct TlsGuiTextureFormat Static = {
        RGBA32RowUnpack,RGBA32RowPack,TrueColorGetColor,TrueColorSetColor
    };
    static TlsGuiTextureFormat r = {
        .Static=&Static,.bpp=32,.CmpntNb=4,
        .bpcmpnt={8,8,8,8},.CmpntIdx={2,1,0,3}
    };
    return &r;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatARGB8888(void) {
    static struct TlsGuiTextureFormat Static = {
        RGBA32RowUnpack,RGBA32RowPack,TrueColorGetColor,TrueColorSetColor
    };
    static TlsGuiTextureFormat r = {
        .Static=&Static,.bpp=32,.CmpntNb=4,
        .bpcmpnt={8,8,8,8},.CmpntIdx={3,0,1,2}
    };
    return &r;
}
TlsGuiTextureFormat *TlsGuiTxtrFormatABGR8888(void) {
    static struct TlsGuiTextureFormat Static = {
        RGBA32RowUnpack,RGBA32RowPack,TrueColorGetColor,TrueColorSetColor
    };
    static TlsGuiTextureFormat r = {
        .Static=&Static,.bpp=32,.CmpntNb=4,
        .bpcmpnt={8,8,8,8},.CmpntIdx={3,2,1,0}
    };
    return &r;
}

/*_____________________________________________________________
 |
 |
 |_____________________________________________________________
*/
/*
typedef struct {
    TlsGuiTextureUnpack TlsGuiTextureUnpack;
    TlsRGBA dst[32];
    int nb;
} UnpackCtx;
static TlsRGBA *UnpackNext(TlsGuiTextureRowUnpack *this,int *nb) {
    TlsGetThat(UnpackCtx,TlsGuiTextureUnpack);
    return that->dst+0;
}
static TlsGuiTextureRowUnpack *RowUnpack(TlsGuiTextureFormat *this,TlsGuiTexture *src,int x,int y,int nb){
    UnpackCtx *r;
    static struct TlsGuiTextureRowUnpack Static = {UnpackNext};
    rPush(r);
    r->TlsGuiTextureUnpack.Static = &Static;
    return &r->TlsGuiTextureUnpack;
}
typedef struct {
    TlsGuiTexturePack TlsGuiTexturePack;
} PackCtx;
static void PackNext(TlsGuiTextureRowPack *this,TlsRGBA *src,int nb) {
    TlsGetThat(PackCtx,TlsGuiTexturePack);
}
static TlsGuiTextureRowPack *RowPack(TlsGuiTextureFormat *this,TlsGuiTexture *dst,int x,int y) {
    PackCtx *r;
    static struct TlsGuiTextureRowPack Static = {PackNext};
    rPush(r);
    r->TlsGuiTextureRowPack.Static = &Static;
    return &r->TlsGuiTextureRowPack;
}

TlsGuiTextureFormat *TlsGuiTxtrFormat(void) {
    static struct TlsGuiTextureFormat Static = {
        RowUnpack,RowPack
    };
}
*/
