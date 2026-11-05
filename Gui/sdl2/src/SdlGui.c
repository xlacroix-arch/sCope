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

#define TlsMax(a,b) (((a)>=(b))?(a):(b))
#define TlsMin(a,b) (((a)<=(b))?(a):(b))

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

/*______________________________________________________________
 |
 | Note on RGBA: we use struct {unsigned char R,G,B,A;} to 
 | represent RGBA, which give consistent result on all 
 | architectures but SDL mysteriously decided to take the
 | opposite convention for some reason.
 |______________________________________________________________
*/

typedef struct {
    TlsGuiEventsPoll TlsGuiEventsPoll;
    TlsGuiInputDevices Inputs;
    char PadIdSock[256];
    struct JoyPad {
        int Id,Device,HatIdx;
        unsigned int HatState;
        SDL_Joystick *Pad;
    } Pad[4];
} PollInputs;

static inline int PadSelect(PollInputs *poll,int id) {
    int num;
    num = poll->PadIdSock[id&0xff];
    if (num>=0) {
        if (poll->Pad[num].Id!=id) {
            int i;
            num = -1; i = 0;
            while ((i<4)&&(num<0)) {
                if (poll->Pad[i].Id==id) { num=i;}
                i++;
            }
        }
    }
    return num;
}

static void JoyPadClear(struct JoyPad *r) {
    r->Id = -1;
    r->Device = -1;
    r->Pad = NULL;
}
static int JoyPadInit(struct JoyPad *r,SDL_Joystick *Pad,int Device) {
    r->Pad = Pad;
    r->Id = SDL_JoystickInstanceID(Pad);
    r->Device = Device;
    r->HatIdx = SDL_JoystickNumButtons(Pad);
    r->HatState = 0;
    return r->Id;
}

static unsigned int PollmsTime(TlsGuiEventsPoll *this) {
    return SDL_GetTicks();
}
static  int PollProcess(TlsGuiEventsPoll *this) {
    int r;
    unsigned int timestamp;
    SDL_Event event;
    TlsGetThat(PollInputs,TlsGuiEventsPoll);
    r = SDL_PollEvent(&event);
    if (r) {
        switch (event.type) {
            case SDL_CONTROLLERAXISMOTION:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEREMAPPED:
            break;
            case SDL_JOYAXISMOTION: {
                int num;
                num = PadSelect(that,event.jaxis.which);
                if (num>=0) {
                    if (event.jaxis.axis<12) {
                        x_(that->Inputs.Joy[num].Axis[event.jaxis.axis],Trigger,event.jaxis.timestamp,event.jaxis.value);
                    }
                }
            } break;
            case SDL_JOYBALLMOTION: {
                int num;
                num = PadSelect(that,event.jball.which);
            } break;
            case SDL_JOYHATMOTION: {
                int num,hatnum,hatstart,butstart,butend,odir,dir,dirx;
                unsigned int oval,mask;
                num = PadSelect(that,event.jhat.which);
                hatnum = event.jhat.hat; 
                hatstart = hatnum<<2;
                butstart = that->Pad[num].HatIdx+hatstart;
                dir = event.jhat.value;
                oval = that->Pad[num].HatState;
                odir = (oval>>(hatstart))&0xf;
                mask = 0xf<<hatstart;
                that->Pad[num].HatState = (oval&(~mask))|(dir<<hatstart);
                dirx = odir^dir;
                butend = butstart+4;
                while (butstart<butend) {
                    if (dirx&1) {
                        if (dir&1) {
                            x_(that->Inputs.Joy[num].Down,Trigger,event.jhat.timestamp,butstart,butstart);
                        } else {
                            x_(that->Inputs.Joy[num].Up,Trigger,event.jhat.timestamp,butstart,butstart);
                        }
                    }
                    dirx>>=1; dir>>=1; butstart++;
                }
            } break;
            case SDL_JOYBUTTONDOWN: {
                int num;
                num = PadSelect(that,event.jbutton.which);
                if (num>=0) {
                    x_(that->Inputs.Joy[num].Down,Trigger,event.jbutton.timestamp,event.jbutton.button,event.jbutton.button);
                }
            } break;
            case SDL_JOYBUTTONUP: {
                int num;
                num = PadSelect(that,event.jbutton.which);
                if (num>=0) {
                    x_(that->Inputs.Joy[num].Up,Trigger,event.jbutton.timestamp,event.jbutton.button,event.jbutton.button);
                }
            } break;
            case SDL_JOYDEVICEADDED: {
                int i,n,Id;
                i=0; n=-1;
                while ((i<4)&&(n==-1)) {
                    if (that->Pad[i].Id==-1) { n=i; }
                    i++;
                }
                if (n>=0) {
                    SDL_Joystick *J;
                    J = SDL_JoystickOpen(i);
                    if (J!=NULL) {
                        Id = JoyPadInit(that->Pad+n,J,event.jdevice.which);
                        that->PadIdSock[Id&0xff]=n;
                    }
                }
            } break;
            case SDL_JOYDEVICEREMOVED: {
                int i,n;
                i = 0; n=-1;
                while ((i<4)&&(n==-1)) {
                    if (that->Pad[i].Device==event.jdevice.which) { n=i; }
                    i++;
                }
                if (n>=0) {
                    SDL_JoystickClose(that->Pad[n].Pad);
                    JoyPadClear(that->Pad+n);
                }
            } break;
            case SDL_KEYDOWN: {
                int scan,code,mod;
                scan = event.key.keysym.scancode;
                code = event.key.keysym.sym;
                mod =  event.key.keysym.mod;
                timestamp = event.key.timestamp;
                if (((mod&KMOD_SHIFT)!=0)!=((mod&KMOD_CAPS)!=0)) {
                    if((code>='a')&&(code<='z')) code+='A'-'a';
                }
                if (event.key.repeat) {
                    x_(that->Inputs.Keyboard.Repeat,Trigger,timestamp,scan,code);
                } else {
                    x_(that->Inputs.Keyboard.Down,Trigger,timestamp,scan,code);
                }
            } break;
            case SDL_KEYUP: {
                int scan,code,mod;
                scan = event.key.keysym.scancode;
                code = event.key.keysym.sym;
                mod =  event.key.keysym.mod;
                timestamp = event.key.timestamp;
                if (((mod&KMOD_SHIFT)!=0)!=((mod&KMOD_CAPS)!=0)) {
                    if((code>='a')&&(code<='z')) code+='A'-'a';
                }
                x_(that->Inputs.Keyboard.Up,Trigger,timestamp,scan,code);
            } break;
            case SDL_MOUSEMOTION: {
                x_(that->Inputs.Mouse.Pos,Trigger,event.motion.timestamp,event.motion.x,event.motion.y);
            } break;
            case SDL_MOUSEBUTTONDOWN: {
                int button;
                TlsGuiInKey *ev;
                button = event.button.button+c_(Gui,MouseButton,Left)-SDL_BUTTON_LEFT; // SDL_BUTTON_LEFT, RIGHT, MIDDLE, X1, X2
                if (event.button.clicks==2) {
                    ev = that->Inputs.Mouse.DoubleClick;
                } else {
                    ev = that->Inputs.Mouse.Down;
                }
                x_(ev,Trigger,event.button.timestamp,button,button);
            } break;
            case SDL_MOUSEBUTTONUP: {
                int button;
                button = event.button.button-SDL_BUTTON_LEFT;
                x_(that->Inputs.Mouse.Up,Trigger,event.button.timestamp,button,button);
            } break;
            case SDL_MOUSEWHEEL: {
                if (event.wheel.x) {
                    x_(that->Inputs.Mouse.Wheel[0],Trigger,event.wheel.timestamp,event.wheel.x);
                }
                if (event.wheel.y) {
                    x_(that->Inputs.Mouse.Wheel[1],Trigger,event.wheel.timestamp,event.wheel.x);
                }
            } break;
            case SDL_QUIT: {
                x_(that->Inputs.Window.Close,Trigger,event.quit.timestamp);
            } break;
            case SDL_WINDOWEVENT: {
                switch(event.window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                    x_(that->Inputs.Window.Close,Trigger,event.window.timestamp);
                break;
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    x_(that->Inputs.Window.KeyboardAcquired,Trigger,event.window.timestamp);
                break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    x_(that->Inputs.Window.KeyboardLost,Trigger,event.window.timestamp);
                break;
                case SDL_WINDOWEVENT_ENTER:
                    // SDL_ShowCursor(SDL_DISABLE);
                    x_(that->Inputs.Window.MouseEnter,Trigger,event.window.timestamp);
                break;
                case SDL_WINDOWEVENT_LEAVE:
                    // SDL_ShowCursor(SDL_ENABLE);
                    x_(that->Inputs.Window.MouseLeave,Trigger,event.window.timestamp);
                break;
                case SDL_WINDOWEVENT_MOVED:
                    x_(that->Inputs.Window.Move,Trigger,event.window.timestamp,event.window.data1,event.window.data2);
                break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    x_(that->Inputs.Window.Resize,Trigger,event.window.timestamp,event.window.data1,event.window.data2);
                break;
                case SDL_WINDOWEVENT_HIDDEN:
                case SDL_WINDOWEVENT_MINIMIZED:
                    x_(that->Inputs.Window.Iconize,Trigger,event.window.timestamp);
                break;
                case SDL_WINDOWEVENT_SHOWN:
                case SDL_WINDOWEVENT_EXPOSED:
                case SDL_WINDOWEVENT_RESTORED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                    x_(that->Inputs.Window.Restore,Trigger,event.window.timestamp);
                break;
                }
            } break;
        }
    }
    return r;
}
static TlsGuiInputDevices *PollCallBacks(TlsGuiEventsPoll *this) {
    TlsGetThat(PollInputs,TlsGuiEventsPoll);
    return &that->Inputs;
}

static PollInputs *NewEventPoll() {
    PollInputs *r;
    static struct TlsGuiEventsPoll Static = {PollmsTime,PollProcess,PollCallBacks};
    rPush(r);
    r->TlsGuiEventsPoll.Static = &Static;
    TlsGuiInputDevicesInit(&r->Inputs);
    /* Joystick */ {
        int i,nb;
        for (i=0;i<4;i++) { JoyPadClear(r->Pad+i); }
        for (i=0;i<256;i++) { r->PadIdSock[i] = -1; }
        nb = SDL_NumJoysticks();
        printf("%d joystick detected.\n",nb);
        if (nb>4) nb=4;
        for (i=0;i<nb;i++) {
            int Id;
            Id = JoyPadInit(r->Pad+i,SDL_JoystickOpen(i),i);
            r->PadIdSock[Id&0xff]=i;
            printf("\t%d:\"%s\"\n",i,SDL_JoystickName(r->Pad[i].Pad));
        }
    }
    return r;
}
static void EventPollClose(PollInputs *r) {
    int i;
    for (i=0;i<4;i++) {
         if (r->Pad[i].Pad!=NULL) { 
             SDL_JoystickClose(r->Pad[i].Pad); 
             r->Pad[i].Pad=NULL; 
         }
    }
}

/*_____________________________________________________
 |
 |
 |_____________________________________________________
*/

typedef struct {
    TlsGuiRenderer TlsGuiRenderer;
    TlsMemPool *Pool,*MappedPool,*PalettePool;
    TlsGuiInputDevices *InDev;
    SDL_Renderer *Tgt;
    SDL_Window *Window;
    Uint32 TextureFormat;
    TlsGuiTextureFormat *MappedFormat;
    TlsDList Textures;
    TlsDList Palettes;
} GuiSDLRenderer;

    /*_______________________________
     |
     |_______________________________
    */

typedef struct {
    TlsGuiMappedPalette TlsGuiMappedPalette;
    TlsDList dList;
    TlsDList AttachedTextures;
    GuiSDLRenderer *Tgt;
    SDL_Palette *Palette;
    int Released;
} MappedPalette;
static void GuiSdlPaletteUpdate(TlsRGBA *src,SDL_Palette *dst,int b,int nb) {
     rOpen {
         SDL_Color *updated,*q;
         TlsRGBA *p;
         int i = nb;
         rnPush(updated,i);
         q=updated; p = src;
         while (i) { q->a=p->A; q->r=p->R; q->g=p->G; q->b=p->B; q++; p++; i--;}
         SDL_SetPaletteColors(dst,updated,b,nb);
     } rClose
}
static void MappedPaletteUpdate(TlsGuiMappedPalette *this,int b,int e);
static void ReleaseMappedPalette(MappedPalette *p) {
    TlsDList *t;
    t=&p->dList; t->n->p=t->p; t->p->n=t->n;
    SDL_FreePalette(p->Palette);
    x_(p->Tgt->PalettePool,Free,p,sizeof(MappedPalette));
}
static int MappedPaletteRelease(TlsGuiMappedPalette *this) {
     TlsDList *t;
     int r;
     TlsGetThat(MappedPalette,TlsGuiMappedPalette);
     that->Released=(0==0);
     t=&that->AttachedTextures; r=(t==t->n); 
     if (r) {
         ReleaseMappedPalette(that);
     } 
     return r;
}
static TlsGuiMappedPalette *RenderMapPalette(TlsGuiRenderer *this,TlsGuiTextureFormat *format,TlsRGBA *Colors) {
    static struct TlsGuiMappedPalette Static={MappedPaletteUpdate,MappedPaletteRelease};
    MappedPalette *r; TlsDList *l;
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    r=x_(that->PalettePool,Allocate);
    r->TlsGuiMappedPalette.Static=&Static;
    r->TlsGuiMappedPalette.Format=format;
    r->TlsGuiMappedPalette.Colors=Colors;
    int nb=1<<(format->bpp);
    l=&r->AttachedTextures; l->n=l->p=l;
    r->Tgt=that; r->Released=(0!=0);
    r->Palette=SDL_AllocPalette(nb);
    GuiSdlPaletteUpdate(Colors,r->Palette,0,nb);
    l=&r->dList; l->p=&that->Palettes; l->n=l->p->n; l->n->p=l->p->n=l;
    return &r->TlsGuiMappedPalette;
}
    /*_______________________________
     |
     |_______________________________
    */
/*
static TlsGuiTextureFormat *TlsGuiFormatFromSDL(Uint32 format) {
    TlsGuiTextureFormat *r;
    r = TlsGuiTxtrFormatRGBA8888();
    switch (format) {
        case SDL_PIXELFORMAT_INDEX1LSB: r=TlsGuiTxtrFormatBitmapLSB(); break;
        case SDL_PIXELFORMAT_INDEX1MSB: r=TlsGuiTxtrFormatBitmap(); break;
        case SDL_PIXELFORMAT_INDEX4LSB: r=TlsGuiTxtrFormatIndex4LSB(); break;
        case SDL_PIXELFORMAT_INDEX4MSB: r=TlsGuiTxtrFormatIndex4(); break;
        case SDL_PIXELFORMAT_RGB332: 
        case SDL_PIXELFORMAT_INDEX8: r=TlsGuiTxtrFormatIndex8(); break;
        case SDL_PIXELFORMAT_RGB444:
        case SDL_PIXELFORMAT_ARGB4444: r=TlsGuiTxtrFormatBGRA4444(); break;
        case SDL_PIXELFORMAT_RGBA4444: r=TlsGuiTxtrFormatABGR4444(); break;
        case SDL_PIXELFORMAT_BGR444:
        case SDL_PIXELFORMAT_ABGR4444: r=TlsGuiTxtrFormatRGBA4444(); break;
        case SDL_PIXELFORMAT_BGRA4444: r=TlsGuiTxtrFormatARGB4444(); break;
        case SDL_PIXELFORMAT_ARGB1555: break;
        case SDL_PIXELFORMAT_RGB555: 
        case SDL_PIXELFORMAT_RGBA5551: break;
        case SDL_PIXELFORMAT_ABGR1555: break;
        case SDL_PIXELFORMAT_BGR555: 
        case SDL_PIXELFORMAT_BGRA5551: break;
        case SDL_PIXELFORMAT_BGR565: r=TlsGuiTxtrFormatBGR565(); break;
        case SDL_PIXELFORMAT_RGB565: r=TlsGuiTxtrFormatRGB565(); break;
        case SDL_PIXELFORMAT_RGB24: r=TlsGuiTxtrFormatRGB888(); break;
        case SDL_PIXELFORMAT_BGR24: r=TlsGuiTxtrFormatBGR888(); break;
        case SDL_PIXELFORMAT_BGRX8888: 
        case SDL_PIXELFORMAT_BGRA8888: r=TlsGuiTxtrFormatARGB8888(); break;
        case SDL_PIXELFORMAT_BGR888: 
        case SDL_PIXELFORMAT_ABGR8888: r=TlsGuiTxtrFormatRGBA8888(); break;
        case SDL_PIXELFORMAT_RGB888:  
        case SDL_PIXELFORMAT_ARGB8888: r=TlsGuiTxtrFormatBGRA8888(); break;
        case SDL_PIXELFORMAT_RGBX8888: 
        case SDL_PIXELFORMAT_RGBA8888: r=TlsGuiTxtrFormatABGR8888(); break;
        case SDL_PIXELFORMAT_ARGB2101010: 
    }
    return r;
}
static void TextureParamsPrint(SDL_Texture *txtr) {
     int w,h,access;
     Uint32 txtFormat; Uint8 alphamod,c[3];
     SDL_BlendMode Blnd;
     SDL_QueryTexture(txtr,&txtFormat,&access,&w,&h);
     TlsGuiFormatFromSDL(txtFormat);
     printf("Format:%x,dim(%d,%d)",txtFormat,w,h);
     SDL_GetTextureBlendMode(txtr,&Blnd);
     SDL_GetTextureAlphaMod(txtr,&alphamod);
     SDL_GetTextureColorMod(txtr,c,c+1,c+2);
     printf(" BlendMode:%d alphamod:%d, colormod:(%d,%d,%d)",Blnd,alphamod,c[0],c[1],c[2]);
     printf("\n");
} */
typedef struct {
    TlsGuiMappedTexture TlsGuiMappedTexture;
    TlsDList/*<MappedTexture.List>*/ List;
    struct {
        TlsDList dList;
        MappedPalette *Palette;
    } Palette;
    GuiSDLRenderer *Renderer;
    TlsGuiTextureFormat *DstFormat;
    SDL_Renderer *Tgt;
    SDL_Texture *Texture;
    TlsGuiTexture *Src;
} MappedTexture;
static void MappedPaletteUpdate(TlsGuiMappedPalette *this,int b,int e) {
     TlsGetThat(MappedPalette,TlsGuiMappedPalette);
     int MaxNb = (1<<this->Format->bpp);
     b=TlsMax(b,0); e=TlsMin(MaxNb,e);
     GuiSdlPaletteUpdate(this->Colors+b,that->Palette,b,e-b);
     /* Update the textures */ {
         TlsDList *Txtr=&that->AttachedTextures;
         TlsDList *l=Txtr->n;
         while (l!=Txtr) {
             TlsI2dRange Whole;
             MappedTexture *L=v_(MappedTexture,Palette.dList,l);
             Whole.b.x[0]=Whole.b.x[1]=0;
             Whole.e.x[0]=L->Src->w; Whole.e.x[1]=L->Src->h;
             x_(&L->TlsGuiMappedTexture,Update,&Whole);
             l=l->n;
         }
     }
}
static Uint32 SDLFormatFromTlsGui(TlsGuiTextureFormat *gui) {
    Uint32 r;
    r = SDL_PIXELFORMAT_UNKNOWN;
    if (gui->CmpntNb==1) {
        switch (gui->bpp) {
            case 1: r = SDL_PIXELFORMAT_INDEX1MSB; break;
            case 4: r = SDL_PIXELFORMAT_INDEX4MSB; break;
            case 8: r = SDL_PIXELFORMAT_INDEX8; break;
        }
    }
    if (gui->CmpntNb==3) {
        switch (gui->bpp) {
            case 16: r = SDL_PIXELFORMAT_RGB565; break;
            case 24: if (gui->CmpntIdx.b<gui->CmpntIdx.r) {
                    r = SDL_PIXELFORMAT_RGB24; 
                } else  {
                    r = SDL_PIXELFORMAT_BGR24;
                }
            break;
        }
    }
    if (gui->CmpntNb==4) {
        int idx;
        idx = ((gui->CmpntIdx.a==0)<<1)|((gui->CmpntIdx.b<gui->CmpntIdx.r))|((gui->bpp==16)<<2);
        static Uint32 rslts[8]= {
            SDL_PIXELFORMAT_ABGR8888,SDL_PIXELFORMAT_ARGB8888,
            SDL_PIXELFORMAT_BGRA8888,SDL_PIXELFORMAT_RGBA8888,
            SDL_PIXELFORMAT_ABGR4444,SDL_PIXELFORMAT_ARGB4444,
            SDL_PIXELFORMAT_BGRA4444,SDL_PIXELFORMAT_RGBA4444,
        };
        r = rslts[idx];
    }
    return r;
}
static void MappedTextureNoUpdate(TlsGuiMappedTexture *this,TlsI2dRange *area) {}

static void ColoredMapDraw(TlsGuiMappedTexture *this,TlsI2dRange *Area) {
    SDL_Rect src,dst;
    TlsI2dPoint *Pos;
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    src.x = Area->b.x[0]; src.y = Area->b.x[1];
    dst.w = src.w = Area->e.x[0]-src.x;
    dst.h = src.h = Area->e.x[1]-src.y;
    Pos = &that->Renderer->TlsGuiRenderer.CursorPos;
    dst.x = Pos->x[0];
    dst.y = Pos->x[1];
    SDL_RenderCopy(that->Tgt,that->Texture,&src,&dst);
}
static void ColoredMapScaleDraw(TlsGuiMappedTexture *this,TlsI2dRange *Area,TlsI2dPoint *DstDim) {
    SDL_Rect src,dst;
    TlsI2dPoint *Pos;
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    src.x = Area->b.x[0]; src.y = Area->b.x[1];
    src.w = Area->e.x[0]-src.x; src.h = Area->e.x[1]-src.y;
    dst.w = DstDim->x[0]; dst.h = DstDim->x[1];
    Pos = &that->Renderer->TlsGuiRenderer.CursorPos;
    dst.x = Pos->x[0];
    dst.y = Pos->x[1];
    SDL_RenderCopy(that->Tgt,that->Texture,&src,&dst);
}
static void MappedTextureOrtTriangles(TlsGuiMappedTexture *this,TlsGuiScreenMesh *Mesh) {
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    SDL_RenderGeometry(that->Tgt,that->Texture,(SDL_Vertex *)(Mesh->Vtx),Mesh->VtxNb,Mesh->Idx,Mesh->IdxNb);
}
static void MappedTextureUpdate(TlsGuiMappedTexture *this,TlsI2dRange *Area) { 
    SDL_Rect area;
    unsigned char *ddst;
    int pitch;
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    area.x = Area->b.x[0]; area.y=Area->b.x[1];
    area.w = Area->e.x[0]-area.x; area.h=Area->e.x[1]-area.y;
    SDL_LockTexture(that->Texture,&area,(void **)(&ddst),&pitch);
    /* Transfert data. */ {
        TlsGuiTexture TxtDst;
        TlsGuiTextureArea src,dst;
        dst.Texture = &TxtDst;
        TxtDst.Format = that->Renderer->MappedFormat; /* For now we get for a RGBA or variation of */
        TxtDst.Palette = 0; //
        TxtDst.w = that->Src->w;
        TxtDst.h = that->Src->h;
        TxtDst.pitch = pitch;
        TxtDst.data = ddst;
        src.Texture = that->Src;
        dst.Pic = *Area;
        src.Pic = *Area;
        TlsGuiTextureTransfert(&dst,&src);
    }
    SDL_UnlockTexture(that->Texture);
}
static void MappedTextureRelease(TlsGuiMappedTexture *this) {
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    SDL_DestroyTexture(that->Texture);
    {   TlsDList *p,*n;
        p  = &that->List;
        p->p->n = p->n; p->n->p = p->p;
    }
    if (that->Palette.Palette!=0) {
        TlsDList *l;
        l = &that->Palette.dList;
        l->p->n=l->n; l->n->p=l->p; l->p=l->n=l;
        if (that->Palette.Palette->Released) {
            TlsGuiMappedPalette *P=&that->Palette.Palette->TlsGuiMappedPalette;
            x_(P,Release);
        }
    }
    x_(that->Renderer->MappedPool,Free,that,sizeof(MappedTexture));
}

static void GreyedMapDraw(TlsGuiMappedTexture *this,TlsI2dRange *Area) {
    SDL_Rect src,dst;
    TlsI2dPoint *Pos;
    TlsGuiRenderer *ctx;
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    src.x = Area->b.x[0]; src.y = Area->b.x[1];
    dst.w = src.w = Area->e.x[0]-src.x;
    dst.h = src.h = Area->e.x[1]-src.y;
    ctx = &that->Renderer->TlsGuiRenderer;
    dst.x = ctx->CursorPos.x[0];
    dst.y = ctx->CursorPos.x[1];
    SDL_SetRenderDrawColor(that->Tgt,ctx->Back.R,ctx->Back.G,ctx->Back.B,ctx->Back.A);
    SDL_RenderFillRect(that->Tgt,&dst);
    SDL_SetRenderDrawColor(that->Tgt,ctx->Front.R,ctx->Front.G,ctx->Front.B,ctx->Front.A);
    SDL_SetTextureColorMod(that->Texture,ctx->Front.R,ctx->Front.G,ctx->Front.B);
    SDL_RenderCopy(that->Tgt,that->Texture,&src,&dst);
}
static void GreyedMapScaleDraw(TlsGuiMappedTexture *this,TlsI2dRange *Area,TlsI2dPoint *DstDim) {
    SDL_Rect src,dst;
    TlsI2dPoint *Pos;
    TlsGuiRenderer *ctx;
    TlsGetThat(MappedTexture,TlsGuiMappedTexture);
    src.x = Area->b.x[0]; src.y = Area->b.x[1];
    src.w = Area->e.x[0]-src.x; src.h = Area->e.x[1]-src.y;
    dst.w = DstDim->x[0]; dst.h = DstDim->x[1];
    ctx = &that->Renderer->TlsGuiRenderer;
    dst.x = ctx->CursorPos.x[0];
    dst.y = ctx->CursorPos.x[1];
    SDL_SetRenderDrawColor(that->Tgt,ctx->Back.R,ctx->Back.G,ctx->Back.B,ctx->Back.A);
    SDL_RenderFillRect(that->Tgt,&dst);
    SDL_SetRenderDrawColor(that->Tgt,ctx->Front.R,ctx->Front.G,ctx->Front.B,ctx->Front.A);
    SDL_SetTextureColorMod(that->Texture,ctx->Front.R,ctx->Front.G,ctx->Front.B);
    SDL_RenderCopy(that->Tgt,that->Texture,&src,&dst);
}

    /*_______________________________
     |
     |_______________________________
    */

static void RenderSetFront(TlsGuiRenderer *this) {
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    SDL_SetRenderDrawColor(that->Tgt,this->Front.R,this->Front.G,this->Front.B,this->Front.A);
}
static void RenderSetBack(TlsGuiRenderer *this) { /* Nothing to do! */ }
static void RenderSetClip(TlsGuiRenderer *this) {
    SDL_Rect clip;
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    clip.x = this->Clip.b.x[0];
    clip.y = this->Clip.b.x[1];
    clip.w = this->Clip.e.x[0]-this->Clip.b.x[0];
    clip.h = this->Clip.e.x[1]-this->Clip.b.x[1];
    SDL_RenderSetClipRect(that->Tgt,&clip);
}
static void RenderClear(TlsGuiRenderer *this) {
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    SDL_SetRenderDrawColor(that->Tgt,this->Back.R,this->Back.G,this->Back.B,this->Back.A);
    SDL_RenderClear(that->Tgt);
    SDL_SetRenderDrawColor(that->Tgt,this->Front.R,this->Front.G,this->Front.B,this->Front.A);
}
static void RenderPoint(TlsGuiRenderer *this) {
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    SDL_RenderDrawPoint(that->Tgt,this->CursorPos.x[0],this->CursorPos.x[1]);
}
static void RenderLine(TlsGuiRenderer *this,int x1,int y1) {
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    SDL_RenderDrawLine(that->Tgt,this->CursorPos.x[0],this->CursorPos.x[1],this->CursorPos.x[0]+x1,this->CursorPos.x[1]+y1);
}
static void RenderRectangle(TlsGuiRenderer *this,int x1,int y1) {
    SDL_Rect r;
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    r.x=this->CursorPos.x[0]; r.y=this->CursorPos.x[1];
    r.w=x1; r.h=y1;
    SDL_RenderDrawRect(that->Tgt,&r);
}
static void RenderFillRectangle(TlsGuiRenderer *this,int x1,int y1) {
    SDL_Rect r;
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    r.x=this->CursorPos.x[0]; r.y=this->CursorPos.x[1];
    r.w=x1; r.h=y1;
    SDL_RenderFillRect(that->Tgt,&r);
}

static void RenderDisplay(TlsGuiRenderer *this) {
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    SDL_RenderPresent(that->Tgt);
}
static TlsGuiMappedTexture *RenderMapTexture(TlsGuiRenderer *this,
    int Updatable,TlsGuiMappedPalette *pal,TlsGuiTexture *Txt) {
     MappedTexture *r;
     TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
     r = x_(that->MappedPool,Allocate);
     if (Updatable) {
         if ((Txt->Format->CmpntNb==1)&&(Txt->Palette==&TlsGuiPaletteGreyScale)) {
             static struct TlsGuiMappedTexture Static = {
                 GreyedMapDraw,GreyedMapScaleDraw,MappedTextureOrtTriangles,MappedTextureUpdate,MappedTextureRelease
             };
             r->TlsGuiMappedTexture.Static = &Static;
         } else {
             static struct TlsGuiMappedTexture Static = {
                 ColoredMapDraw,ColoredMapScaleDraw,MappedTextureOrtTriangles,MappedTextureUpdate,MappedTextureRelease
             };
             r->TlsGuiMappedTexture.Static = &Static;
         }
     } else {
         if ((Txt->Format->CmpntNb==1)&&(Txt->Palette==&TlsGuiPaletteGreyScale)) {
             static struct TlsGuiMappedTexture Static = {
                 GreyedMapDraw,GreyedMapScaleDraw,MappedTextureOrtTriangles,MappedTextureNoUpdate,MappedTextureRelease
             };
             r->TlsGuiMappedTexture.Static = &Static;
         } else {
             static struct TlsGuiMappedTexture Static = {
                 ColoredMapDraw,ColoredMapScaleDraw,MappedTextureOrtTriangles,MappedTextureNoUpdate,MappedTextureRelease
             };
             r->TlsGuiMappedTexture.Static = &Static;
         }
     }
     r->Renderer = that;
     r->Tgt = that->Tgt;
     r->Src = Txt;
     if (Txt->Format->CmpntNb==1) {
         MappedPalette *Pal=v_(MappedPalette,TlsGuiMappedPalette,pal);
         TlsDList *l=&r->Palette.dList;
         r->Palette.Palette=Pal;
         l->n=&Pal->AttachedTextures; l->p=l->n->p; l->p->n=l->n->p=l;
     }  else {
         r->Palette.Palette=0;
         { TlsDList *l; l=&r->Palette.dList; l->p=l->n=l;}
     }
     if (Updatable) {
         TlsI2dRange All;
         All.b.x[0] = All.b.x[1] = 0;
         All.e.x[0] = Txt->w; All.e.x[1] = Txt->h;
         r->Texture = SDL_CreateTexture(that->Tgt,that->TextureFormat,SDL_TEXTUREACCESS_STREAMING,Txt->w,Txt->h);
         x_(&r->TlsGuiMappedTexture,Update,&All);
     } else  {
         SDL_Surface *surf;
         Uint32 Format;
         Format = SDLFormatFromTlsGui(Txt->Format);
         surf = SDL_CreateRGBSurfaceWithFormatFrom(Txt->data,Txt->w,Txt->h,Txt->Format->bpp,Txt->pitch,Format);
         if (Txt->Format->CmpntNb==1) { SDL_SetSurfacePalette(surf,r->Palette.Palette->Palette); }
         r->Texture = SDL_CreateTextureFromSurface(that->Tgt,surf);
         SDL_FreeSurface(surf);
     }
     SDL_SetTextureBlendMode(r->Texture,SDL_BLENDMODE_BLEND);
     /* Record the texture. */{
         TlsDList *L;
         L = &r->List;
         L->p = &that->Textures; 
         L->n = that->Textures.n;
         L->p->n = L->n->p = L;
     }
     return &r->TlsGuiMappedTexture;
}

static void TlsGuiSDLRendererClose(TlsGuiRenderer *this) {
     TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
     TlsDList *l;
     while (that->Textures.n!=(&that->Textures)) {
         MappedTexture *M;
         TlsDList *L;
         L = that->Textures.n; M = v_(MappedTexture,List,L);
         x_(&M->TlsGuiMappedTexture,Release);
     }
     l = &that->Palettes;
     while (l->n!=l) {
         TlsDList *L=l->n;
         MappedPalette *P=v_(MappedPalette,dList,L);
         ReleaseMappedPalette(P);
     }
}

static void RndrGetInfo(GuiSDLRenderer *r) {
    SDL_RendererInfo Info;
    Uint32 format;
    int i,found,best;
    SDL_GetRendererInfo(r->Tgt,&Info);
    r->TextureFormat = Info.texture_formats[0];
    i = 0; found = (0!=0); best = (0!=0);
    do {
        format = Info.texture_formats[i];
        i++;
        if ((format==SDL_PIXELFORMAT_ABGR8888)&&(!found)) {
            found = (0==0);
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatRGBA8888();
        }
        if ((format==SDL_PIXELFORMAT_ARGB8888)&&(!found)) {
            found = (0==0);
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatBGRA8888();
        }
        if ((format==SDL_PIXELFORMAT_BGRA8888)&&(!found)) {
            found=(0==0);
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatARGB8888();
        }
        if (format==SDL_PIXELFORMAT_RGBA8888) {
            found=(0==0); best=(0==0);
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatABGR8888();
        }
        if ((format==SDL_PIXELFORMAT_RGBX8888)&&(!found)) {
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatABGR8888();
        }
        if ((format==SDL_PIXELFORMAT_BGRX8888)&&(!found)) {
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatARGB8888();
        }
        if ((format==SDL_PIXELFORMAT_RGB888)&&(!found)) {
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatABGR8888();
        }
        if ((format==SDL_PIXELFORMAT_BGR888)&&(!found)) {
            r->TextureFormat = format;
            r->MappedFormat = TlsGuiTxtrFormatARGB8888();
        }
    } while ((i<Info.num_texture_formats)&&(!best));
}

static void RenderVertices(TlsGuiRenderer *this,TlsGuiScreenMesh *Mesh) {
    TlsGetThat(GuiSDLRenderer,TlsGuiRenderer);
    SDL_RenderGeometry(that->Tgt,0,(SDL_Vertex *)(Mesh->Vtx),Mesh->VtxNb,Mesh->Idx,Mesh->IdxNb);
}
static TlsGuiRenderer *TlsGuiSDLRenderer(SDL_Window *Window,SDL_Renderer *Rndr,int Width,int Heigth) {
    GuiSDLRenderer *r;
    TlsGuiRenderer *R;
    static struct TlsGuiRenderer Static = {
        RenderSetFront,RenderSetBack,RenderSetClip,RenderClear,RenderPoint,RenderLine,
        RenderRectangle,RenderFillRectangle,RenderDisplay,RenderMapTexture,RenderMapPalette,
        RenderVertices
    };
    rPush(r);
    r->TlsGuiRenderer.Static = &Static;
    r->Tgt = Rndr;
    R = &r->TlsGuiRenderer;
    R->CursorPos.x[0] = R->CursorPos.x[1] = 0;
    R->Clip.b.x[0] = R->Clip.b.x[1] = 0;
    R->Clip.e.x[0] = Width; R->Clip.e.x[1] = Heigth;
    R->Front.R = R->Front.G = R->Front.B = 255; R->Front.A = 255;
    R->Back.R = R->Back.G = R->Back.B = 0; R->Back.A = 255;
    R->vScale = Heigth; R->vScale = R->vScale/(256.*120.); // 256*120pts for the height of the window
    R->Global.x[0][0]=R->Global.x[1][1]=R->Global.x[2][2]=R->Global.x[3][3]=1<<24;
    R->Global.x[0][1]=R->Global.x[0][2]=R->Global.x[0][3]=0;
    R->Global.x[1][0]=R->Global.x[1][2]=R->Global.x[1][3]=0;
    R->Global.x[2][0]=R->Global.x[2][1]=R->Global.x[2][3]=0;
    R->Global.x[3][0]=R->Global.x[3][1]=R->Global.x[3][2]=0;

    r->Pool = TlsMemPoolPush();
    r->Pool = x_(r->Pool,SizeSelect,0x100);
    r->MappedPool = x_(r->Pool,SizeSelect,sizeof(MappedTexture));
    r->Textures.n = r->Textures.p = &r->Textures;
    r->PalettePool = x_(r->Pool,SizeSelect,sizeof(MappedPalette));
    r->Palettes.n = r->Palettes.p = &r->Palettes;
 
    r->Window = Window;
    RndrGetInfo(r);
    return R;
}

/*______________________________________________________________
 |
 |
 |______________________________________________________________
*/


static void PrintRendererInfo(SDL_RendererInfo *Info) {
    int i;
    printf("\nRenderer \"%s\" Info:",Info->name);
    printf("\nMax texture:(%d,%d)",Info->max_texture_width,Info->max_texture_height);
    printf("\nSupported pixel format(%d): ",Info->num_texture_formats);
    i = Info->num_texture_formats;
    while (i>0) {
        Uint32 format,typ,ordr;
        i--; format = Info->texture_formats[i];
        printf("\n    ");
        typ = SDL_PIXELTYPE(format);
        ordr = SDL_PIXELORDER(format);
        if (format==SDL_PIXELFORMAT_RGB444) printf("RGB444|");
        if (format==SDL_PIXELFORMAT_ARGB4444) printf("ARGB4444|");
        if (format==SDL_PIXELFORMAT_ABGR4444) printf("ABGR4444|");
        if (format==SDL_PIXELFORMAT_RGBA4444) printf("RGBA4444|");
        if (format==SDL_PIXELFORMAT_BGRA4444) printf("BGRA4444|");
        if (format==SDL_PIXELFORMAT_INDEX4LSB) printf("4LSB|");
        if (format==SDL_PIXELFORMAT_INDEX4MSB) printf("4MSB|");
        if (format==SDL_PIXELFORMAT_INDEX1LSB) printf("1LSB|");
        if (format==SDL_PIXELFORMAT_INDEX1MSB) printf("1MSB|");
        if (format==SDL_PIXELFORMAT_INDEX8) printf("IDX8|");
        if (format==SDL_PIXELFORMAT_ABGR8888) printf("ABGR8888|");
        if (format==SDL_PIXELFORMAT_ARGB8888) printf("ARGB8888|");
        if (format==SDL_PIXELFORMAT_BGRA8888) printf("BGRA8888|");
        if (format==SDL_PIXELFORMAT_RGBA8888) printf("RGBA8888|");
        if (format==SDL_PIXELFORMAT_RGBX8888) printf("RGBX8888|");
        if (format==SDL_PIXELFORMAT_BGRX8888) printf("BGRX8888|");
        if (format==SDL_PIXELFORMAT_RGB888) printf("RGB888|");
        if (format==SDL_PIXELFORMAT_BGR888) printf("BGR888|");
        if (format==SDL_PIXELFORMAT_RGBA32) printf("RGBA32|");
        if (format==SDL_PIXELFORMAT_BGRA32) printf("BGRA32|");
        if (format==SDL_PIXELFORMAT_ARGB32) printf("ARGB32|");
        if (format==SDL_PIXELFORMAT_ABGR32) printf("ABGR32|");

        switch(typ) {
            case SDL_PIXELTYPE_INDEX1: printf("INDEX1|"); break;
            case SDL_PIXELTYPE_INDEX4: printf("INDEX4|"); break;
            case SDL_PIXELTYPE_INDEX8: printf("INDEX8|"); break;
            case SDL_PIXELTYPE_PACKED8: printf("PACKED8|"); break;
            case SDL_PIXELTYPE_PACKED16: printf("PACKED16|"); break;
            case SDL_PIXELTYPE_PACKED32: printf("PACKED32|"); break;
            case SDL_PIXELTYPE_ARRAYU8: printf("ARRAYU8|"); break;
            case SDL_PIXELTYPE_ARRAYU16: printf("ARRAYU16|"); break;
            case SDL_PIXELTYPE_ARRAYU32: printf("ARRAYU32|"); break;
            case SDL_PIXELTYPE_ARRAYF16: printf("ARRAYF16|"); break;
            case SDL_PIXELTYPE_ARRAYF32: printf("ARRAYF32|"); break;
            default: printf("%x:UNKNOWN|",typ);
        }
        switch(ordr) {
            case SDL_BITMAPORDER_4321 : printf("4321|"); break;
            case SDL_BITMAPORDER_1234 : printf("1234|"); break;
        }
        switch(ordr) {
            case SDL_PACKEDORDER_XRGB : printf("XRGB|"); break;
            case SDL_PACKEDORDER_RGBX : printf("RGBX|"); break;
            case SDL_PACKEDORDER_ARGB : printf("ARGB|"); break;
            case SDL_PACKEDORDER_RGBA : printf("RGBA|"); break;
            case SDL_PACKEDORDER_XBGR : printf("XBGR|"); break;
            case SDL_PACKEDORDER_BGRX : printf("BGRX|"); break;
            case SDL_PACKEDORDER_ABGR : printf("ABGR|"); break;
            case SDL_PACKEDORDER_BGRA : printf("BGRA|"); break;
        }
        printf("%x,",format);
    }
    printf("\n");
}


static void SDL2_Renderer_Go(SDL_Window *Window,int width,int height,int (*TheMain)(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll)) {
    SDL_Renderer *Renderer;
    Renderer = SDL_CreateRenderer(
        Window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC /* SDL_RENDERER_TARGETTEXTURE*/
    );
    if (Renderer!=NULL) {
        SDL_RendererInfo Info;
        TlsGuiRenderer *Tgt;
        PollInputs *Poll;
        SDL_GetRendererInfo(Renderer,&Info);
        PrintRendererInfo(&Info);
        rOpen {
            Tgt = TlsGuiSDLRenderer(Window,Renderer,width,height);
            Poll = NewEventPoll();
            TheMain(Tgt,&Poll->TlsGuiEventsPoll);
            EventPollClose(Poll);
            TlsGuiSDLRendererClose(Tgt);
        } rClose
        SDL_DestroyRenderer(Renderer);
    } else {
        printf("Error SDL2 could not create renderer: %s\n",SDL_GetError());
    }
}

// extern int SDL2_Main_Go(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll);
static void SDL2_Window_Go(int width,int height,int (*TheMain)(TlsGuiRenderer *Tgt,TlsGuiEventsPoll *Poll)) {
    SDL_Window *Window;
    Window = SDL_CreateWindow(
        "Application",
        SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,width,height,
        SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_INPUT_GRABBED /* SDL_WINDOW_OPENGL SDL_WINDOW_VULKAN SDL_WINDOW_HIDDEN*/
    );
    if (Window!=NULL) {
        SDL2_Renderer_Go(Window,width,height,TheMain);
        SDL_DestroyWindow(Window);
    } else {
        printf("Error SDL2 could not open window: %s\n",SDL_GetError());
    }
}

static void SDL2_Vulkan_Go(void) {
    // SDL_Vulkan_LoadLibrary(NULL);
    /*  {
        static char **EnabledLayerNames = {NULL};
        static char **EnabledExtensionNames = {NULL};
        VkInstanceCreateInfo Info;
        VkAllocationCallbacks AllocCallbacks;
        VkInstance Instance;
        VkGetInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
        VkCreateInstance = (void *)(VkGetInstanceProcAddr(NULL,"VkCreateInstance"));
        Info.sType = ;
        Info.pNext = NULL;
        Info.flags = 0;
        Info.pApplicationInfo = NULL;
        Info.enabledLayerCount = 0;
        Info.ppEnabledLayerNames = EnabledLayerNames;
        Info.enabledExtensionCount = 0;
        Info.ppEnabledExtensionNames = EnabledExtensionNames;
        VkCreateInstance(&Info,NULL,&Instance);
        VkXXX = (void *)(VkGetInstanceProcAddr(&Instance,"XXX"));
    } */
    // SDL_Vulkan_UnloadLibrary();
}

void SDL2_Main(int width,int height,int (*TheMain)(TlsGuiRenderer *,TlsGuiEventsPoll *)) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK/*|SDL_INIT_GAMECONTROLLER*/)==0)  {
        SDL2_Window_Go(width,height,TheMain);
        SDL_Quit();
    } else {
        printf("Error Initialising SDL2 : %s \n",SDL_GetError());
    }
}


