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

#ifndef _Geo_h_
#define _Geo_h_

typedef struct {
    int x,y;
} Geo2dPoint;
typedef struct {
    int w,h;
} Geo2dExtent;
typedef struct {
    Geo2dPoint Pos;
    Geo2dExtent Extent;
} Geo2dRectangle;
#define Geo2dRectangleClip(r,A,B) {\
    int eA,eB,er;\
    (r).Pos.x = ((A).Pos.x<(B).Pos.x)?(B).Pos.x:(A).Pos.x;\
    (r).Pos.y = ((A).Pos.y<(B).Pos.y)?(B).Pos.y:(A).Pos.y;\
    eA = (A).Pos.x+(A).Extent.w;\
    eB = (B).Pos.x+(B).Extent.w;\
    er = ((eA>eB)?eB:eA)-(r).Pos.x;\
    (r).Extent.w = er>=0?er:0;\
    eA = (A).Pos.y+(A).Extent.h;\
    eB = (B).Pos.y+(B).Extent.h;\
    er = ((eA>eB)?eB:eA)-(r).Pos.y;\
    (r).Extent.h = er>=0?er:0;\
}
#define Geo2dRectangleExtend(r,d0,d1) {\
    if (\
        ((d0).Extent.w<=0)||((d0).Extent.h<=0)||\
        ((d1).Extent.w<=0)||((d1).Extent.h<=0)\
    ) {\
        (r)=(((d0).Extent.w<=0)||((d0).Extent.h<=0))?(d1):(d0);\
    } else {\
        int w0,w1;\
        (r).Pos.x = ((d0).Pos.x<=(d1).Pos.x)?(d0).Pos.x:(d1).Pos.x;\
        (r).Pos.y = ((d0).Pos.y<=(d1).Pos.y)?(d0).Pos.y:(d1).Pos.y;\
        w0 = (d0).Pos.x+(d0).Extent.w;\
        w1 = (d1).Pos.x+(d1).Extent.w;\
        (r).Extent.w = ((w0>=w1)?w0:w1)-(r).Pos.x;\
        w0 = (d0).Pos.y+(d0).Extent.h;\
        w1 = (d1).Pos.y+(d1).Extent.h;\
        (r).Extent.h = ((w0>=w1)?w0:w1)-(r).Pos.y;\
    }\
}

/*___________________________________________________________________
 |
 | Integer math allow for a greater control of rounding behaviour.
 | You have to be very careful of the range of intermediate results.
 |  Intermediate results are often in their own ballpark when it 
 | comes to range. When storing and using them, you should always take 
 | that into account. For instance: 
 |   * Square norm of a vector will usually have a meaningful range in 
 | the 0..62 bits range (depending on the vector) and therefore should 
 | probably stored on a long long or a float. Depending on your 
 | problem, you might want to keep the norm of a vector, alongside 
 | its coordinates, instead of the square norm, because the norm will
 | usually be more comparable to the coordinates and therefore hold 
 | better information/bit.
 |   * Ratios will usually hold a meaningful rational part as well
 | as a scale factor. So, you will usually want to keep them in 
 | a fixed point integer, alongside an exponant factor, or both of
 | those options (aka, float).
 |   * In our case: quaternion will usually be used as unitary 
 | quaternions and might better be used as 7:24 fixed integers. Using
 | an exponent in this case is not useful.
 |___________________________________________________________________
*/

typedef struct { int x[2]; } TlsI2dVec,TlsI2dPoint;
typedef struct { int x[3]; } TlsI3dVec,TlsI3dPoint;
typedef struct { int x[4]; } TlsI4dVec,TlsI4dPoint,TlsIQuaternion;
typedef struct { float x[4]; } TlsfQuaternion;
typedef struct { int x[2][2]; } TlsI2dMat;
typedef struct { int x[3][3]; } TlsI3dMat;
typedef struct { int x[4][4]; } TlsI4dMat;
typedef struct { TlsI3dPoint O; TlsfQuaternion v; } TlsI3dReferential;

typedef struct { float x[2]; } Tlsf2dVec,Tlsf2dPoint;

#define TlsI2dVecSum(R,A,B) {\
    (R).x[0]=(A).x[0]+(B).x[0];\
    (R).x[1]=(A).x[1]+(B).x[1];\
}

#define TlsI2dVecProd(R,a,v) {\
    int A; A=a;\
    (R).x[0]=A*(v).x[0];\
    (R).x[1]=A*(v).x[1];\
}

#define TlsI2dVecDiff(R,A,B) {\
    (R).x[0]=B.x[0]-A.x[0];\
    (R).x[1]=B.x[1]-A.x[1];\
}

#define TlsI2dVecScal(A,B) \
    (((A).x[0]*(B).x[0])+((A).x[1]*(B).x[1]))


#define TlsI3dVecSet(R,X,Y,Z) {\
    (R).x[0]=(X); (R).x[1]=(Y); (R).x[2]=(Z);\
}

#define TlsI3dVecSum(R,A,B) {\
    (R).x[0]=(A).x[0]+(B).x[0];\
    (R).x[1]=(A).x[1]+(B).x[1];\
    (R).x[2]=(A).x[2]+(B).x[2];\
}

#define TlsI3dVecProd(R,A,v) {\
    int a; a=A;\
    (R).x[0]=a*(v).x[0];\
    (R).x[1]=a*(v).x[1];\
    (R).x[2]=a*(v).x[2];\
}

#define TlsI3dVecDiff(R,A,B) {\
    (R).x[0]=(B).x[0]-(A).x[0];\
    (R).x[1]=(B).x[1]-(A).x[1];\
    (R).x[2]=(B).x[2]-(A).x[2];\
}

#define TlsI3dVecScal(A,B) \
    ((A).x[0]*(B).x[0])+((A).x[1]*(B).x[1])+((A).x[2]*(B).x[2])

typedef struct { TlsI2dPoint O; int r; } TlsICircle;
typedef struct { TlsI3dPoint O; int r; } TlsISphere;
typedef struct { TlsI3dPoint O; TlsI3dVec A; int r; } TlsICone;
typedef struct { TlsISphere Base; TlsI3dVec d; } TlsICylinder;
typedef struct { TlsI2dPoint O; TlsI2dVec n; } TlsI2dBox,TlsI2dSegment;
typedef struct { TlsI3dPoint O; TlsI3dVec n; } TlsIPlane,TlsI3dBox,TlsI3dSegment;
typedef struct { TlsI3dPoint A[3]; } TlsI3dTriangle;
typedef struct { TlsI2dPoint A[3]; } TlsI2dTriangle;

/*----------------------------------------------*/

/*_____________________________________________________________________
 |
 | Neighbourhood:
 |    Usage: insert your objects into the neighbourhood. You might
 | then be able to use the "for each interact" method, which will list
 | the pair of objects likely to intersect each other.
 |    The neighbourhood constructs a 3d map of the objects location and
 | rules out some of the possible interactions based on it. Depending 
 | on your problem, it can be quite a lot.
 |    That method fits mainly with problems where objects can only 
 | slightly overlap. Typically this is the case with solid objects
 | that aren't too "empty".
 |    In that case, we can expect the neighbourhood to be constructed
 | in a NLog(s) operations, and the listing in about mN operations. 
 | Where s is the volume of the occupied space compared to the volume 
 | of the smallest object, and m is a constant that represent the
 | number of neighbours of the same size an object can typically have.
 |    The memory taken by the neighbourhood is in NLog(s).
 |    Object relocation is possible. But the constant s will typically
 | only grow, which means you should only do this if you're going to 
 | stay in the same overall location. Usually, if the number of still
 | objects is not much greater than the number of moving objects, it
 | might be faster to construct the neighbourhood, use it a few times
 | and then dispose of it and build a new one.
 |_____________________________________________________________________
*/

/*___________________________________________________________________________
 |
 | Bounding spheres are defined with a point and a radius. You can use any 
 | definition of distance you want here (meaning round, cubic, octagonal...
 | spheres). However what will be used here is the "max distance", which 
 | means cubic spheres with a radius equal to the radius given and sides 
 | parallel to the axes. You should make sure that your spheres fit into 
 | those, depending on your distance, you might have to adjust your radius
 | by a fixed factor...
 |___________________________________________________________________________
*/
/*___________________________________________________________________________
 |
 | We use the notions of "cells" here. Cells have the following properties:
 |     * We use the TlsISphere to represent cells, however:
 |     * They are cubic, parallel to the axes
 |     * r represent the cube size and is a power of 2
 |     * The O(x,y,z) fields represent a corner of the cube. The cube itself
 | is the block situated between (x,y,z) and C(x+r,y+r,z+r).
 |___________________________________________________________________________
*/

typedef struct {
    struct TlsNeighObject *Static;
} TlsNeighObject;
struct TlsNeighObject {
    /* CellIntersect returns: 0->disjoint; 1->intersection; 2->Cell embeded in the object. */
    /* SubCellIntersect do so for the 8 subcells of a cell.
     * Each subcell extent from its origin O to O+{s,s,s}. Since their origin are offsets with a s/2 step,
     * they do overlap.
     * The result is packed in an integer:
     * (2 bits per subcell. {0,0,0} stored at bits 0..1, {s/2,0,0} at bits 2..3, {0,s/2,0} at bits 4..5, 
     * ..., {s/2,s/2,s/2} at bits 14..15.  For instance, for a wholly embeded cell, the result would be
     * 0x6666 and for a completely disjoint cell, the result would be 0x0. )
     */
    int (*SubCellIntersect)(TlsNeighObject *this,TlsI3dPoint *O,int s);
};
/* functions that should help create your own NeighObject: they return 0,1 or 2 when 
 * the cell is respectively disjoint, intersecting or within the object.
 * Round Cylinder are useful because a sphere moving along a segment will present that shape.
 */
int TlsI3dCellCellIntersect(TlsISphere *Cell,TlsISphere *Ob);
int TlsI3dCellSphereIntersect(TlsISphere *Cell,TlsISphere *Ob);
int TlsI3dCellRoundCylinderIntersect(TlsISphere *Cell,TlsICylinder *Ob);
int TlsI3dCellSegmentIntersect(TlsISphere *Cell,TlsI3dSegment *Ob);
int TlsI3dCellTriangleIntersect(TlsISphere *Cell,TlsI3dTriangle *Ob);
int TlsI3dSubCellIntersect(TlsISphere *Cell,int (*Intersect)(TlsISphere *cel,void *Clos),void *clos);


typedef struct {
    struct TlsI3dNeighHandle *Static;
    TlsNeighObject *Obj;
} TlsI3dNeighHandle;
struct TlsI3dNeighHandle {
    void (*GetLoc)(TlsI3dNeighHandle *this,TlsISphere *R);
    void (*Relocate)(TlsI3dNeighHandle *this,TlsISphere *Loc);
    void (*Remove)(TlsI3dNeighHandle *this);
    void (*ForEachNeighbour)(TlsI3dNeighHandle *this,
        int (*Intersect)(TlsI3dNeighHandle *Obj,void *Clos),void *Clos
    );
    void (*PeekCell)(TlsI3dNeighHandle *this,TlsISphere *rCell,TlsISphere *rBound);
};

typedef struct {
    struct TlsI3dNeighbourhood *Static;
} TlsI3dNeighbourhood;

struct TlsI3dNeighbourhood {
    TlsI3dNeighHandle *(*AddObject)(TlsI3dNeighbourhood *this,TlsISphere *Loc,TlsNeighObject *O);
    void (*ForEachInteract)(TlsI3dNeighbourhood *this,int (*Cont)(TlsI3dNeighHandle *o0,TlsI3dNeighHandle *o1,void *clos),void *Clos);
};

TlsI3dNeighbourhood *TlsI3dNeighbourhoodNew(void);


/*____________________________________________________________
 |
 | Color and 2d geometrical shapes.
 |____________________________________________________________
*/

typedef struct {
    unsigned char r,g,b,a;
} GeoRGBA;

#define GeoRGBAlpha_Opaque 0xff
#define GeoRGBAlpha_Transparent 0

/*----------------------------------------------*/

typedef struct {
    struct Geo2dShapeMap *Static;
} Geo2dShapeMap;
struct Geo2dShapeMap {
    void (*Boundary)(Geo2dShapeMap *this,Geo2dRectangle *R);
    int (*MapLine)(Geo2dShapeMap *this,int x,int y,int dx);
};
extern Geo2dShapeMap Geo2dShapeMapNull;

typedef struct {
    struct Geo2dShape *Static;
} Geo2dShape;
struct Geo2dShape {
    void (*ForEach)(Geo2dShape *this,Geo2dShapeMap *Dst);
};
extern Geo2dShape Geo2dShapeNull;

Geo2dShape *Geo2dShapeLine(Geo2dPoint *A,Geo2dPoint *B);
Geo2dShape *Geo2dShapeDisk(Geo2dPoint *O,int r);
Geo2dShape *Geo2dShapeCircle(Geo2dPoint *O,int r,int w);
Geo2dShape *Geo2dShapeWLine(Geo2dPoint *A,Geo2dPoint *B,int Width);

// Check that the Child doesn't mess with the boundaries.
// For efficiency, should only be put at the top level.
Geo2dShape *Geo2dSafeShape(Geo2dShape *Child); 


#endif
