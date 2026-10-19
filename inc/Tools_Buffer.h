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

#ifndef _Tools_Buffer_h_
#define _Tools_Buffer_h_

#include <Tools_Class.h>

/*_____________________________________________________________________________
 |
 | Buffer0 usage: use p as a pointer to read data sequentially.
 |    Use Check0 when a specific condition (typically a 0) is read.
 | On the end of stream, Check0 will return a empty string, else the buffer will 
 | be update and the new pointer will be returned.
 | If Buffer0 is pointing in a file, the file will be automatically closed at
 | the end.
 |
 | All new items are pushed on the heap.
 |_____________________________________________________________________________
*/

extern unsigned char const TlsBufferEob[], const TlsBufferEof[];

typedef struct {
    struct TlsBuffer0 *Static;
    unsigned char *p;
} TlsBuffer0;
struct TlsBuffer0 {
    unsigned char *(*Check0)(TlsBuffer0 *this,unsigned char *pos);
    unsigned char *(*Reset)(TlsBuffer0 *this);
    void (*Close)(TlsBuffer0 *this);
};

extern TlsBuffer0 TlsBuffer0Null_;
#define TlsBuffer0Null  (&TlsBuffer0Null_)
extern TlsBuffer0 TlsEmptyBuffer0;
TlsBuffer0 *TlsStringBuffer(unsigned char *str);
TlsBuffer0 *TlsBufferBuffer(char *b,char *e);
TlsBuffer0 *TlsFileBuffer(char *FileName);

TlsBuffer0 *TlsBufferCat(TlsBuffer0 **a,int n);
TlsBuffer0 *TlsBufferCat0(TlsBuffer0 *a,...);
TlsBuffer0 *TlsBufferReplace(TlsBuffer0 *Base,TlsBuffer0 *Edit,int b,int e);

TlsClassDef(TlsBuffMacro) {
    int *(*ParamNumber)(TlsBuffMacro *this);
    TlsBuffer0 *(*Apply)(TlsBuffMacro *this,TlsBuffer0 **Var);
};
TlsClassDef(TlsBuffMacroDef) {
    void (*AddSubstitution)(TlsBuffMacroDef *this,int b,int e,int ParamNum);
    TlsBuffMacro *(*Generate)(TlsBuffMacroDef *this);
};

#endif
