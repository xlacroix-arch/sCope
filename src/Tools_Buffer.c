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
#include <stdio.h>
#include <stdargs.h>
#include <Tools.h>
#include <Tools_Buffer.h>

/*-------------------------------*/

static unsigned char *NoBuff="";
static int NoCheck0(TlsBuffer0 *this,unsigned char *p) { return TlsEob; }
static unsigned char *NoReset(TlsBuffer0 *this) { return NoBuff;};
static void NoClose(TlsBuffer0 *this) {}
static struct TlsBuffer0 NoBuffStatic = {NoCheck0,NoReset,NoClose};
static TlsBuffer0 TlsBuffer0Null_ = {&NoBuffStatic};
static TlsBuffer0 TlsEmptyBuffer0 = {&NoBuffStatic};

/*-------------------------------*/

typedef struct {
    TlsBuffer0 TlsBuffer0;
    unsigned char *p;
} StringBuff;
static int StringCheck0(TlsBuffer0 *this,unsigned char *pos){
    int r=*pos; 
    this->p=pos;
    return (r==0)?TlsEob0:r; 
}
static unsigned char *StringReset(TlsBuffer0 *this) { 
    TlsGetThat(StringBuff,TlsBuffer0);
    this->p = that->p;
    return this->p;
}
TlsBuffer0 *TlsStringBuffer(char *str) {
    StringBuff *r;
    static struct TlsBuffer0 Static = {StringCheck0,StringReset,NoClose};
    rPush(r);
    r->TlsBuffer0.p = r->p = (unsigned char *)str;
    r->TlsBuffer0.Static = &Static;
    return &r->TlsBuffer0;
}

/*-------------------------------*/

typedef struct {
    TlsBuffer0 TlsBuffer0
    unsigned char *b,*e;
} BufferBuffer;
static int BufferCheck0(TlsBuffer0 *this,unsigned char *p) {
    TlsGetThat(BufferBuffer,TlsBuffer0);
    this->p = p;
    return ((that->e-p)>0)?*p:TlsEob;
}
static unsigned char *BufferReset(TlsBuffer *this) {
    TlsGetThat(BufferBuffer,TlsBuffer0);
    this->p = that->b;
    return this->p;
}
TlsBuffer0 *TlsBufferBuffer(char *b,char *e) {
    BufferBuffer *r;
    static struct TlsBuffer0 Static = {BufferCheck0,BufferReset,NoClose};
    rPush(r);
    r->TlsBuffer0.Static = Static;
    r->TlsBuffer0.p = r->b = (unsigned char *)b;
    r->e = (unsigned char *)e;
    return &r->TlsBuffer0;
}

/*-------------------------------*/

typedef struct {
    TlsBuffer0 TlsBuffer0;
    char *Name;
    FILE *F;
    unsigned char Buffer[256+4];
} BufferFile;
static int FileCheck0(TlsBuffer0 *this,unsigned char *p) {
    TlsGetThat(BufferFile,TlsBuffer0);
    int r = TlsEob0;
    if ((!that->F)&&((this->p-that->Buffer)==257)) {
        r=TlsEof; 
    } else {
        this->p = p;
        if (!that->F) {
            that->F = fopen(that->Name,"r");
            this->p = that->Buffer+256;
        }
        if ((p-that->Buffer)==256) {
            int nb=fread(that->Buffer,1,256,that->F);
            that->Buffer[nb]=0;
            this->p = that->Buffer; r = *this->p;
            if (nb==0) {
                fclose(that->F);
                that->F = 0;
                this->p = that->Buffer+257;
                r = TlsEof;
            } 
        }
    }
    return r;
}
static unsigned char *FileBufferReset(TlsBuffer0 *this) {
    TlsGetThat(BufferFile,TlsBuffer0);
    if (that->F==NULL) {
        that->F = fopen(that->Name,"rb");
    } else {
        rewind(that->F);
    }
    this->p = that->Buffer+256;
    return this->p;
}
static void FileBufferClose(TlsBuffer0 *this) {
    TlsGetThat(BufferFile,TlsBuffer0);
    if (that->F!=NULL) {
        fclose(that->F);
        that->F = NULL;
    }
    this->p = that->Buffer+257;
}

TlsBuffer0 *TlsFileBuffer(char *FileName) {
    static struct TlsBuffer0 Static = {FileBufferCheck0,FileBufferReset,FileBufferClose};
    BufferFile *r;
    rPush(r);
    r->TlsBuffer0.Static = &Static;
    r->TlsBuffer0.p = r->Buffer+256;
    r->Name = FileName;
    r->F = 0;
    r->Buffer[256]=r->Buffer[257]=0;
    return &r->TlsBuffer0;
}

/*---------------------------------------*/

typedef struct {
    TlsBuffer0 TlsBuffer0;
    TlsBuffer0 **Buff;
    int nb,idx;
} BufferCat;
static int BufferCatCheck0(TlsBuffer0 *this,unsigned char *pos) {
    int r;
    TlsGetThat(BufferCat,TlsBuffer0);
    int idx=that->idx,nb=that->nb;
    r = TlsEob0;
    while ((idx<nb)&&(r<0)) {
        r = x_(that->Buff[idx],Check0,pos);
        if (r<0) {
            x_(that->Buff[idx],Close);
            idx++; 
            if (idx<nb) {
                pos = x_(that->Buff[idx],Reset);
                r = *pos;
                if (r==0) { 
                    r=x_(that->Buff[idx],check0,pos);
                    pos = that->Buff[idx]->p;
                }
            }
        }
    }
    that->idx = idx;
    if (idx<nb) {
        this->p = that->Buffer[idx]->p;
    } else {
        this->p = TlsEmptyBuffer0.p;
    } 
    return r;
}
static unsigned char *BufferCatReset(TlsBuffer0 *this) {
    TlsGetThat(BufferCat,TlsBuffer0);
    if ((idx<nb)&&(idx>=0)) {
        x_(that->Buffer[idx],Close);
    }
    that->idx = -1;
    this->p = x_(&TlsEmptyBuffer0,Reset);
    return this->p;
}
static void BufferCatClose(TlsBuffer0 *this) {
    TlsGetThat(BufferCat,TlsBuffer0);
    int idx = that->idx, nb=that->nb;
    if (idx<0) idx++;
    while (idx<nb) {
        x_(that->Buff[idx++],Close);
    }
    that->idx = idx;
}
TlsBuffer0 *TlsBufferCat(TlsBuffer0 **b,int nb) {
    TlsBuffer0 *R = &TlsEmptyBuffer0;
    if (nb>0) {
        BufferCat *r;
        static struct TlsBuffer0 Static = {BufferCatCheck0,BufferCatReset,BufferCatClose};
        rPush(r);
        R = &r->TlsBuffer;
        r->TlsBuffer0.Static = &Static;
        r->TlBuffer.nb = nb;
        r->Buff = b;
        r->idx = 0;
        R->p=x_(b[0],Reset);
    }
    return R;
}
TlsBuffer0 *TlsBufferCat0(TlsBuffer0 *a,...) {
    int cnt = 0;
    va_list l;
    TlsBuffer0 *p,**pp,**Buff;
    p = a;
    va_start(l,a);
    while (p) {
        cnt++;
        p = va_arg(l,TlsBuffer0 *);
    }
    va_end(l);
    rnPush(Buff,cnt);
    pp = Buff;
    *pp++=a;
    va_start(l,a);
    while (p) { *pp++ = va_arg(l,TlsBuffer0 *); }
    va_end(l);
    return TlsBufferCat(Buff,cnt);
}
