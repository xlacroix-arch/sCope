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

#define TlsMaxSignedVal(Atom) (((unsigned Atom)-1)>>1)
struct TlsBNod {TlsBNod *Children[2];};

typedef struct {
    TlsList TlsList;
    TlsI4dMat *ToCam;
    TlsGuiOut *Mesh;
    TlsI3dPoint Pos;
} MeshLayerNod;
typedef struct {
    TlsBNod LayerNum;
    TlsList/*MeshLayerNod.TlsDList*/ Nods;
} MeshLayer;
typedef struct {
    MeshLayer *NewLayer;
    TlsGuiMeshRecord TlsGuiMeshRecord;
    TlsBTreeRoot zOrder;
} MeshRecord;
static int MeshRecordFound(TlsGuiMeshRecord *this,TlsI4dMat *ToCam,TlsGuiMeshInstance *Meshes,int nb) {
    TlsGetThat(MeshRecord,TlsGuiMeshRecord);
    TlsI4dMat *NewMat;
    MeshLayerNod *insert,*q;
    rPush(NewMat);
    rnPush(insert,nb);
    *NewMat = *ToCam;
    MeshLayer *Layer;
    TlsGuiMeshInstance *p=Meshes; q=insert;
    while (nb) {
        long long a[3],b[3],z;
        int Idx; unsigned int idx;
        TlsBNod *lay;
        q->Mesh=p->Model->Mesh;
        q->ToCam=NewMat;
        q->Pos=p->Pos;
        b[0]=p->Pos.x[0]; b[1]=p->Pos.x[1]; b[2]=p->Pos.x[2];
        a[0]=NewMat->x[2][0]; a[1]=NewMat->x[2][1]; a[2]=NewMat->x[2][2];
        z=((a[0]*b[0]+a[1]*b[1]+a[2]*b[2])>>24)+NewMat->x[2][3];
        // since we going in crescent order, we need to somehow negate z
        z-=p->Model->Bound.r;
        idx=-1; idx=idx^(idx>>1); idx+=z;
        /* ToDo: check if insert is forced */
        lay=x_(&that->zOrder.TlsBTree,Insert,&that->NewLayer->LayerNum,idx);
        if (lay==&that->NewLayer->LayerNum) {
            MeshLayer *NL;
            rPush(NL);
            NL->Nods.n=&TlsListEnd;  
            that->NewLayer=NL; 
        }
        Layer=v_(MeshLayer,LayerNum,lay);
        q->TlsList.n=Layer->Nods.n; Layer->Nods.n=&q->TlsList;
        nb--; q++; p++;
    }
    return -1;
}
typedef struct {
    TlsBNodProcess TlsBNodProcess;
    TlsGuiRenderer *tgt;
} ProcessLayerDisplay;
static int MeshRecordLayerDisplay(TlsBNodProcess *this,TlsBNod *N,unsigned int Key) {
    TlsGetThat(ProcessLayerDisplay,TlsBNodProcess);
    TlsGuiRenderer *tgt=that->tgt;
    MeshLayer *n=v_(MeshLayer,LayerNum,N);
    TlsList *l=n->Nods.n;
    TlsI4dMat oGlob=tgt->Global;
    while (l!=&TlsListEnd) {
        TlsI4dMat ToPos;
        MeshLayerNod *mesh=v_(MeshLayerNod,TlsList,l);
        TlsI4dMatPos(&ToPos,mesh->Pos.x[0],mesh->Pos.x[1],mesh->Pos.x[2]);
        TlsI4dMat24Mult(&tgt->Global,mesh->ToCam,&ToPos);
        x_(mesh->Mesh,Display,tgt);
        l=l->n;
    }
    tgt->Global=oGlob;
    return (0!=0);
}
static void MeshRecordDisplay(MeshRecord *this,TlsGuiRenderer *tgt) {
    static struct TlsBNodProcess Static = {MeshRecordLayerDisplay};
    ProcessLayerDisplay prcs = {{&Static},tgt};
    x_(&this->zOrder.TlsBTree,Bck.ForEach,&prcs.TlsBNodProcess);
}
static MeshRecord *MeshRecordNew(void) {
    static struct TlsGuiMeshRecord Static = {MeshRecordFound};
    MeshRecord *r;
    rPush(r);
    Tls31RadixInit(&r->zOrder,sizeof(TlsBNod));
    rPush(r->NewLayer);
    r->TlsGuiMeshRecord.Static = &Static;
    r->NewLayer->Nods.n = &TlsListEnd;
    return r;
}

typedef struct {
    TlsGuiOut TlsGuiOut;
    TlsGuiMeshBag *Scene;
    TlsGuiCamera *Camera;
} TlsGuiMeshScene;
static void TlsGuiMeshSceneDisplay(TlsGuiOut *this,TlsGuiRenderer *dst) {
    TlsGetThat(TlsGuiMeshScene,TlsGuiOut);
    TlsI4dMat oGlob;
    oGlob = dst->Global;
    rOpen {
        TlsI4dMat CamMat;
        TlsI3dMat *Rot=&that->Camera->Ref.Axis;
        TlsI4dPoint O,N;
        MeshRecord *AllMeshes;
        AllMeshes = MeshRecordNew();
        CamMat.x[3][0]=CamMat.x[3][1]=CamMat.x[3][2]=0; CamMat.x[3][3]=1;
        CamMat.x[0][3]=CamMat.x[1][3]=CamMat.x[2][3]=0;
        CamMat.x[0][0]=Rot->x[0][0]; CamMat.x[0][1]=Rot->x[1][0]; CamMat.x[0][2]=Rot->x[2][0];
        CamMat.x[1][0]=Rot->x[0][1]; CamMat.x[1][1]=Rot->x[1][1]; CamMat.x[1][2]=Rot->x[2][1];
        CamMat.x[2][0]=Rot->x[0][2]; CamMat.x[2][1]=Rot->x[1][2]; CamMat.x[2][2]=Rot->x[2][2];
        O.x[0]=that->Camera->Ref.O.x[0]; O.x[1]=that->Camera->Ref.O.x[1]; O.x[2]=that->Camera->Ref.O.x[2];
        TlsI4dMat24Trans(&N,&CamMat,&O);
        CamMat.x[0][3]=N.x[0]; CamMat.x[1][3]=N.x[1]; CamMat.x[2][3]=N.x[2];
        x_(that->Scene,ForEach,that->Camera,&CamMat,&AllMeshes->TlsGuiMeshRecord);
        MeshRecordDisplay(AllMeshes,dst);
    } rClose
    dst->Global = oGlob;
}
TlsGuiOut *TlsGuiMeshSceneNew(TlsGuiCamera *Camera,TlsGuiMeshBag *Scene) {
    TlsGuiMeshScene *r;
    static struct TlsGuiOut Static = {TlsGuiMeshSceneDisplay};
    rPush(r);
    r->TlsGuiOut.Static = &Static;
    r->Camera =Camera; r->Scene=Scene;
    return &r->TlsGuiOut;
}

/*____________________________________________________
 |
 |
 |____________________________________________________
*/

struct RhombCell {unsigned char Tile; };

typedef struct {
    TlsGuiWorldChunk TlsGuiWorldChunk;
    TlsGuiMeshBound *Library;
    int LibrarySize,MaxLibSize;
    TlsMemPool *Leaf,*OctCells;
    int Scale,Lvl;
} WorldScene;

typedef struct {
    TlsGuiWorldChunk TlsGuiWorldChunk;
    TlsGuiMeshBound *Library;
    int LibrarySize,MaxLibSize;
    WorldScene *Root;
    int Scale;
    struct RhombCell Tiles[256];
} RhombicChunk;



static TlsI3dPoint *RhombChunkCell(TlsI3dPoint *r,TlsI3dPoint *O,int Scale) {
    int a[3],r0[3],v;
    struct {int a,r;} negAdj[2] = {{0,0},{-1,Scale}};
    v=O->x[0]; a[0]=(v/Scale)+negAdj[v<0].a; r0[0]=(v%Scale)+negAdj[v<0].r;
    v=O->x[1]; a[1]=(v/Scale)+negAdj[v<0].a; r0[1]=(v%Scale)+negAdj[v<0].r;
    v=O->x[2]; a[2]=(v/Scale)+negAdj[v<0].a; r0[2]=(v%Scale)+negAdj[v<0].r;
    if ((a[0]^a[1]^a[2])&1) {
        int half = Scale>>1;
        static int Idx[8]={0,1,2,2,0,1,0,-1}; // idx[0] happens for x==y==z
        int r1[3]={r0[0]-half,r0[1]-half,r0[2]-half};
        #define abso(b,a) { int sgn; sgn=(a<0); b=(a^(-sgn))+sgn;}
        int m[3]; abso(m[0],r1[0]); abso(m[1],r1[1]); abso(m[2],r1[2]);
        #undef abso
        int idx=Idx[((m[2]<m[0])<<2) | ((m[1]<m[2])<<1) | (m[0]<m[1])];
        static int Sgn[2]={1,-1};
        a[idx]+=Sgn[r0[idx]<0];
    }
    r->x[0]=a[0]; r->x[1]=a[1]; r->x[2]=a[2];
    return r;
}
static TlsI3dPoint *RCCellFromWorldPos(TlsGuiWorldChunk *this,TlsI3dPoint *r,TlsI3dPoint *O) {
    TlsGetThat(RhombicChunk,TlsGuiWorldChunk);
    return RhombChunkCell(r,O,that->Scale);
}

static TlsGuiCellIntersect *RhombChunk1stInter(TlsGuiWorldChunk *this,TlsGuiCellIntersect *R,TlsI3dPoint *O,TlsI3dVec *v) {
    TlsGetThat(RhombicChunk,TlsGuiWorldChunk);
    TlsGuiCellIntersect *r=0;
    int Scale=that->Scale;
    int xtnt[2]={-(Scale>>1),(17*Scale)>>1};
    int dx[3][2],incX[3];
    long long zb=0,ze=TlsMaxSignedVal(long long),z;
    int frst_slab=(0==0);
    int i=0,out=(0!=0);
    while ((!out)&&(i<3)) {
        incX[i]=v->x[i]>=0; dx[i][0]=xtnt[incX[i]^1]-O->x[i]; dx[i][1]=xtnt[incX[i]]-O->x[i];
        if (v->x[i]==0) { out=(O->x[i]<xtnt[0])||(O->x[i]>=xtnt[1]);} else {
            long long b,e,a=dx[i][0]; b=(a<<24)/v->x[i]; a=dx[i][1]; e=(a<<24)/v->x[i];
            if (frst_slab) { zb=b; ze=e; frst_slab=(0!=0); } else {
                if (b>zb) zb=b; if (e<ze) ze=e;
            } 
            out=((dx[i][1]>=0)==(incX[i]));
        }
        i++;
    }
    z=zb; if (z<0) z=0;
    out = out||(z>=ze);
    if (!out) {
        TlsI3dPoint cCell,A,Op;
        struct ClipPlane {TlsDList List; long long z,dz; TlsI3dVec dCell;} clip[12],*cc,*L,*N;
        TlsDList itr,*l; itr.n=itr.p=&itr;
        long long PlaneDist=Sqrt2_24*Scale;
        void AddFrontPlane(TlsDList *itr,struct ClipPlane *L) {
            TlsDList *p,*l;
            int found;
            p = itr->n; found = (p==itr); l=&L->List;
            while (!found) {
                struct ClipPlane *P=v_(struct ClipPlane,List,p);
                found = (L->z<P->z);
                if (!found) { p=p->n; found=(p==itr);}
            }
            l->n=p; l->p=p->p; l->n->p=l->p->n=l;
        }
        {   long long a;
            a=(v->x[0]*z)>>24; A.x[0]=O->x[0]+a;
            a=(v->x[1]*z)>>24; A.x[1]=O->x[1]+a;
            a=(v->x[2]*z)>>24; A.x[2]=O->x[2]+a;
        }
        RhombChunkCell(&cCell,&A,Scale);
        cc=clip;
        Op=(TlsI3dPoint){{cCell.x[0]*Scale,cCell.x[1]*Scale,cCell.x[2]*Scale}};
        for (i=0;i<3;i++) {
            TlsI3dPoint Op0,Op1;
            int i1=(i+1)%3;
            long long z0,z1,dz,a,vDotN;
            vDotN=v->x[i]+v->x[i1];
            if (vDotN!=0) {
                Op0=Op1=Op;
                Op1.x[i]+=Scale; Op1.x[i1]+=Scale;
                cc->dCell=(TlsI3dVec){0,0,0};
                if (vDotN>0) {
                    a=(Op1.x[i]-O->x[i])+(Op1.x[i1]-O->x[i1]);
                    a=(a<<24)/vDotN; cc->z=a;
                    dz=PlaneDist/(vDotN);
                    cc->dCell.x[i]=cc->dCell.x[i1]=1;
                } else {
                    a=(Op0.x[i]-O->x[i])+(Op0.x[i1]-O->x[i1]);
                    a=(a<<24)/(-vDotN); cc->z=a; 
                    dz=PlaneDist/(-vDotN);
                    cc->dCell.x[i]=cc->dCell.x[i1]=-1;
                }
                cc->dz=dz;
                AddFrontPlane(&itr,cc++);
            }
            vDotN=v->x[i]-v->x[i1];
            if (vDotN!=0) {
                Op0=Op1=Op;
                Op0.x[i1]+=Scale; Op1.x[i]+=Scale;
                if (vDotN>0) {
                    a=(Op1.x[i]-O->x[i])-(Op1.x[i1]-O->x[i1]);
                    a=(a<<24)/vDotN; cc->z= a; 
                    dz=PlaneDist/(vDotN);
                    cc->dCell.x[i]=1; cc->dCell.x[i1]=-1;
                } else {
                    a=(Op0.x[i]-O->x[i])-(Op0.x[i1]-O->x[i1]);
                    a=(a<<24)/(-vDotN); cc->z=a; 
                    dz=PlaneDist/(-vDotN);
                    cc->dCell.x[i]=-1; cc->dCell.x[i1]=1;
                }
                cc->dz=dz;
                AddFrontPlane(&itr,cc++);
            }
        }
        int found =(0!=0);
        l=itr.n; L=v_(struct ClipPlane,List,l);
        N=v_(struct ClipPlane,List,l->n);
        while ((!found)&&(z<ze)) {
            if (!((cCell.x[0]|cCell.x[1]|cCell.x[2])&(-8))) {
                int i = (cCell.x[0]|(cCell.x[1]<<3)|(cCell.x[2]<<6))>>1;
                found=(that->Tiles[i].Tile!=255);
                if (found) {
                    long long c;
                    R->First=cCell; r=R;
                    c=(v->x[0]*z)>>24; R->Contact.x[0]=c+O->x[0];
                    c=(v->x[1]*z)>>24; R->Contact.x[1]=c+O->x[1];
                    c=(v->x[2]*z)>>24; R->Contact.x[2]=c+O->x[2];
                }
            }
            if (!found) {
                L->z+=L->dz;
                if (L->z>N->z) {
                    l=&L->List; l->n->p=l->p; l->p->n=l->n;
                    AddFrontPlane(&itr,L);
                    L=N; l=N->List.n; N=v_(struct ClipPlane,List,l);
                }
                cCell.x[0]+=L->dCell.x[0]; cCell.x[1]+=L->dCell.x[1]; cCell.x[2]+=L->dCell.x[2];
                z=L->z;
            }
        } 
    }
    
    return r;
}

static TlsGuiMeshInstance *RhombicChunkForEach(TlsGuiMeshBag *this,TlsGuiCamera *Clip,TlsI4dMat *CamMat,TlsGuiMeshRecord *Tgt) {
    TlsGuiMeshInstance *r=0;
    int nb,i;
    TlsGuiMeshInstance Trnsfrt[32],*p;
    TlsGetThat(RhombicChunk,TlsGuiWorldChunk.TlsGuiMeshBag);
    nb=0; p = Trnsfrt;
    for (i=0;i<256;i++) {  /* TODO the clipping ! */
        unsigned int t = that->Tiles[i].Tile;
        if ((t!=255)&(t<that->LibrarySize)) {
            int par=((i>>2)^(i>>5))&1;
            p->Model = that->Library+t;
            p->Pos.x[0]=(((i&3)<<1)|par)*that->Scale;
            p->Pos.x[1]=((i>>2)&3)*that->Scale;
            p->Pos.x[2]=((i>>5)&3)*that->Scale;
            nb++; p++;
        }
        if (nb>=32) {
            x_(Tgt,Found,CamMat,Trnsfrt,nb); nb=0; p=Trnsfrt;
        }
    }
    if (nb!=0) {
        x_(Tgt,Found,CamMat,Trnsfrt,nb);
    }
    return r;
}

static int WorldChunk888SetCell(TlsGuiWorldChunk *this,int x,int y,int z,int Val) {
    TlsGetThat(RhombicChunk,TlsGuiWorldChunk);
    int idx = ((z<<5)|(y<<2)|x)&0xff;
    that->Tiles[idx].Tile=Val;
    return (0==0);
}
static int WorldChunk888GetCell(TlsGuiWorldChunk *this,int x,int y,int z) {
    TlsGetThat(RhombicChunk,TlsGuiWorldChunk);
    int idx =  ((z<<5)|(y<<2)|x)&0xff;
    return that->Tiles[idx].Tile;
}
static int WorldChunk888BlockRecord(TlsGuiWorldChunk *this,TlsGuiMeshBound *New){
    TlsGetThat(RhombicChunk,TlsGuiWorldChunk);
    int idx = that->LibrarySize;
    int r=(idx<that->MaxLibSize);
    if (r) { r=idx; that->Library[idx]=*New; that->LibrarySize=idx+1;} else { r=-1;}
    return r;
}

static TlsGuiWorldChunk *Chunk888Init(RhombicChunk *r,int Scale,TlsGuiMeshBound *Lib,int entryNb,int MaxNb) {
    static struct TlsGuiMeshBag BagStatic = {RhombicChunkForEach};
    static struct TlsGuiWorldChunk WorldStatic = {
        WorldChunk888SetCell,WorldChunk888GetCell,WorldChunk888BlockRecord,
        RCCellFromWorldPos,RhombChunk1stInter
    };
    r->TlsGuiWorldChunk.TlsGuiMeshBag.Static = &BagStatic;
    r->TlsGuiWorldChunk.Static = &WorldStatic;
    r->Library = Lib; r->LibrarySize=entryNb; r->MaxLibSize;
    r->Scale = Scale;
    {int i; for (i=0;i<256;i++) { r->Tiles[i].Tile=255; } }
    return &r->TlsGuiWorldChunk;
}

TlsGuiWorldChunk *TlsRhombicChunk888(int Scale,TlsGuiMeshBound *Lib,int entryNb,int MaxNb) {
    RhombicChunk *r;
    rPush(r);
    return Chunk888Init(r,Scale,Lib,entryNb,MaxNb);
}

/*--------------------------------------------------------------*/

typedef struct {
    TlsGuiWorldChunk TlsGuiWorldChunk;
    TlsGuiWorldChunk *Cells[8];
    WorldScene *Root;
    int Lvl,Scale;
} OctChunk;
static TlsGuiWorldChunk *WorldSceneAllocateCell(WorldScene *root,int Lvl,int x,int y,int z) {
    TlsGuiWorldChunk *r=0;
    return r;
}
static int WorldSceneSetCell(TlsGuiWorldChunk *this,int x,int y,int z,int Val) {
    TlsGetThat(OctChunk,TlsGuiWorldChunk);
    int r=(0!=0);
    int idx=(((z>>that->Lvl)&1)<<2)|(((y>>that->Lvl)&1)<<1)|((x>>that->Lvl)&1);
    if (!that->Cells[idx]) {
        that->Cells[idx]=WorldSceneAllocateCell(that->Root,that->Lvl-1,x,y,z);
    }
    if (that->Cells[idx]) {
        r = x_(that->Cells[idx],SetCell,x,y,z,Val);   
    }
    return r;
}


