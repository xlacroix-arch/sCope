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

#ifndef _TlsGui_Local_h_
#define _TlsGui_Local_h_

#include <TlsGuiCtrl.h>

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

#define Pi24 0x3243F6A
#define TwoPi24 0x6487ED5
#define Sqrt3_24 0x1BB67AF
#define Sqrt2_24 0x16A09E6

#define InvSqrt3 0x93CD3A
#define InvSqrt2 0xB504F3 
#define InvSqrt6 0x6882F5

void SinCos(int *sin,int *cos,int Alpha);

typedef struct {
    TlsI3dPoint O;
    TlsI3dMat Axis;
} TlsGuiI3dRefer;

typedef struct {
    TlsGuiI3dRefer Ref;
    struct TlsGuiClippingBox {
        // For orthogonal projection, this would be a box orthogonal to the screen 
        // For projective, this would be a frustrum.
        TlsI3dVec TopLeft; // point toward the top left corner of the screen in screen coordinate;
        int z0,z1;  // clipping distance.
    } Clip;
} TlsGuiCamera;


extern const TlsI4dMat I4dMatId;
TlsI4dMat *TlsI4dMatPos(TlsI4dMat *r,int x,int y,int z);
TlsI4dMat *TlsI4dMatSetRot(TlsI4dMat *r,int alpha,int axis); /* alpha in 8:24 */
TlsI3dVec *TlsI3dMat24Trans(TlsI3dVec *r,TlsI3dMat *A,TlsI3dVec *M);
TlsI3dPoint *TlsI3dPointTrans(TlsI3dPoint *dst,TlsI4dMat *A,TlsI3dPoint *B);
TlsI3dPoint *TlsI3dMatTrans(TlsI3dMat *dst,TlsI4dMat *A,TlsI3dMat *B);
TlsI4dPoint  *TlsI4dMat24Trans(TlsI4dPoint *m,TlsI4dMat *A,TlsI4dPoint *n);
TlsI4dMat *TlsI4dMat24AffRot(TlsI4dMat *r,TlsI3dPoint *O,TlsI3dVec *Axis,int Alpha);
TlsI4dMat *TlsI4dMatInv(TlsI4dMat *r,TlsI4dMat *A);

TlsGuiI3dRefer *TlsGuiI3dReferTrans(TlsGuiI3dRefer *dst,TlsI4dMat *A,TlsGuiI3dRefer *C);
TlsI4dMat *TlsGuiToI3dRefer(TlsI4dMat *r,TlsGuiI3dRefer *src);
TlsI4dMat *TlsGuiFromI3dRefer(TlsI4dMat *r,TlsGuiI3dRefer *src);

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    TlsI2dPoint TextureSize;
    TlsI2dRange Window;
} TlsGeoUVWindow;

/*____________________________________________________
 |
 |
 |____________________________________________________
*/


typedef struct {
    TlsISphere Bound,Occlusion;
    TlsGuiOut *Mesh;
} TlsGuiMeshBound;

typedef struct {
    TlsGuiMeshBound *Model;
    TlsI3dPoint Pos;
} TlsGuiMeshInstance;

     /*-----------------------------*/

typedef struct { struct TlsGuiMeshStream *Static; } TlsGuiMeshStream;
struct TlsGuiMeshStream {
    // pass a buffer and a buffer size, receive a buffer and a effective number
    // the returned buffer is not necessary the buffer passed; an null indicates
    // the end of the stream.
    // Returns the Mesh in object coordinate and the transformation matrix to convert
    // to camera coordinates.
    TlsGuiMeshInstance *(*Next)(TlsGuiMeshStream *this,TlsI4dMat *ToCamera,TlsGuiMeshInstance *dst,int *nb);
    void (*End)(TlsGuiMeshStream *this);
};
typedef struct { struct TlsGuiMeshChunk *Static; } TlsGuiMeshChunk;
struct TlsGuiMeshChunk {
    TlsGuiMeshStream *(*Start)(TlsGuiMeshChunk *this,TlsGuiCamera *Camera);
};

     /*-----------------------------*/

typedef struct { struct TlsGuiMeshRecord *Static; } TlsGuiMeshRecord;
struct TlsGuiMeshRecord {
    int (*Found)(TlsGuiMeshRecord *this,TlsI4dMat *ToCamera,TlsGuiMeshInstance *Meshes,int nb);
};
typedef struct { struct TlsGuiMeshBag *Static; } TlsGuiMeshBag;
struct TlsGuiMeshBag {
    TlsGuiMeshInstance *(*ForEach)(TlsGuiMeshBag *this,TlsGuiCamera *Clip,TlsI4dMat *CamRef,TlsGuiMeshRecord *Tgt);
};


/*____________________________________________________
 |
 |
 |____________________________________________________
*/

typedef struct {
    TlsGuiMeshBag TlsGuiMeshBag;
    struct TlsGuiWorldChunk *Static;
} TlsGuiWorldChunk;
typedef struct {
    TlsI3dPoint First,Prec; // In cell coordinates. First-Prec gives the contact face and the normal
    TlsI3dPoint Contact; // In world coordinates.
} TlsGuiCellIntersect;
struct TlsGuiWorldChunk {
    int (*SetCell)(TlsGuiWorldChunk *this,int x,int y,int z,int Val); // return (0!=0) if failed
    int (*GetCell)(TlsGuiWorldChunk *this,int x,int y,int z);
    int (*BlockRecord)(TlsGuiWorldChunk *this,TlsGuiMeshBound *New);
    TlsI3dPoint *(*CellFromWorldPos)(TlsGuiWorldChunk *this,TlsI3dPoint *r,TlsI3dPoint *O);
    TlsGuiCellIntersect *(*CellIntersect)(TlsGuiWorldChunk *this,TlsGuiCellIntersect *R,TlsI3dPoint *O,TlsI3dVec *v);
};

TlsGuiWorldChunk *TlsRhombicChunk888(int Scale,TlsGuiMeshBound *Lib,int entryNb,int MaxNb);
TlsGuiOut *TlsGuiMeshSceneNew(TlsGuiCamera *Camera,TlsGuiMeshBag *Scene);

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

extern void SDL2_Main(int width,int height,int (*Go)(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll));



#endif
