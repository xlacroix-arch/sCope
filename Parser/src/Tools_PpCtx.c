/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <Tools_PpCtx.h>

typedef struct PNode PNode;
struct PNode {
    PNode *next;
    char s[];
};
typedef struct {
    TlsPpCtx TlsPpCtx;
    TlsHeap *H;
    TlsIdMap *words;
    PNode *head,**tail;
    PNode *once;
    int n;
} BCtx;

#define WordRound(n) ((((size_t)(n))+sizeof(void *)-1)&~(sizeof(void *)-1))

static void CtxAddPath(TlsPpCtx *this,char const *dir) {
    TlsGetThat(BCtx,TlsPpCtx);
    size_t len = strlen(dir),sz = WordRound(sizeof(PNode)+len+1);
    PNode *nd;
    while (len>1 && dir[len-1]=='/') len--;             /* "a/" and "a" are the same */
    TlsHeapIn(that->H)
        rnPush(nd,sz);
    TlsHeapOut
    nd->next = 0;
    memcpy(nd->s,dir,len);
    nd->s[len] = 0;
    *that->tail = nd;
    that->tail = &nd->next;
    that->n++;
}
static char const *CtxPath(TlsPpCtx *this,int i) {
    TlsGetThat(BCtx,TlsPpCtx);
    PNode *nd = that->head;
    for (;nd && i>0;i--) nd = nd->next;
    return nd && i==0 ? nd->s : 0;
}

static int Exists(char const *path) {
    FILE *f = fopen(path,"rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}
/* dir/name on the current heap */
static char *Join(char const *dir,size_t dl,char const *name) {
    size_t nl = strlen(name),sz = WordRound(dl+nl+2);
    char *r;
    rnPush(r,sz);
    memcpy(r,dir,dl);
    if (dl && !(dl==1 && dir[0]=='/')) r[dl++] = '/';
    memcpy(r+dl,name,nl+1);
    return r;
}
static char *CtxResolve(TlsPpCtx *this,char const *name,int quoted,char const *from) {
    TlsGetThat(BCtx,TlsPpCtx);
    PNode *nd;
    char *r;
    if (!*name) return 0;
    if (name[0]=='/') {
        size_t nl = strlen(name),sz = WordRound(nl+1);
        if (!Exists(name)) return 0;
        rnPush(r,sz);
        memcpy(r,name,nl+1);
        return r;
    }
    if (quoted && from) {
        char const *sl = strrchr(from,'/');
        size_t dl = sl ? (size_t)(sl-from) : 0;
        if (sl && dl==0) dl = 1;                        /* from is /x: the dir is / */
        r = Join(from,sl ? dl : 0,name);
        if (Exists(r)) return r;
    } else if (quoted) {
        /* no including file: the name itself, then the paths */
    }
    for (nd=that->head;nd;nd=nd->next) {
        r = Join(nd->s,strlen(nd->s),name);
        if (Exists(r)) return r;
    }
    return 0;
}
static void CtxOnceSet(TlsPpCtx *this,char const *path) {
    TlsGetThat(BCtx,TlsPpCtx);
    size_t len = strlen(path),sz = WordRound(sizeof(PNode)+len+1);
    PNode *nd;
    if (x_(this,OnceHas,path)) return;
    TlsHeapIn(that->H)
        rnPush(nd,sz);
    TlsHeapOut
    memcpy(nd->s,path,len+1);
    nd->next = that->once;
    that->once = nd;
}
static int CtxOnceHas(TlsPpCtx *this,char const *path) {
    TlsGetThat(BCtx,TlsPpCtx);
    PNode *nd;
    for (nd=that->once;nd;nd=nd->next) if (!strcmp(nd->s,path)) return 1;
    return 0;
}
static int CtxWord(TlsPpCtx *this,int id) {
    TlsGetThat(BCtx,TlsPpCtx);
    return (int)(intptr_t)x_(that->words,Get,id);
}

static void CtxReport(TlsPpCtx *this,int Kind,char const *File,unsigned int Line,char const *Text) {
    if (Kind==PpMsgError) this->Errors++;
    else if (Kind==PpMsgWarning) this->Warnings++;
    if (this->Hook && this->Hook->Static->Message) x_(this->Hook,Message,Kind,File,Line,Text);
}
static int CtxDirective(TlsPpCtx *this,int Word,int Id,TlsTok *t,char const *File,unsigned int Line) {
    if (this->Hook && this->Hook->Static->Directive) return x_(this->Hook,Directive,Word,Id,t,File,Line);
    return 0;
}
static void CtxOpened(TlsPpCtx *this,char const *File,unsigned int Line,char const *Path) {
    if (this->Hook && this->Hook->Static->Open) x_(this->Hook,Open,File,Line,Path);
}

TlsPpCtx *TlsPpCtxNew(TlsDict *D,TlsHeap *Where) {
    static struct TlsPpCtx Static = {CtxAddPath,CtxPath,CtxResolve,CtxWord,CtxOnceSet,CtxOnceHas,CtxReport,CtxDirective,CtxOpened};
    static struct { char const *name; int code; } W[] = {
        {"define",PpWord(Define)},{"undef",PpWord(Undef)},{"include",PpWord(Include)},{"if",PpWord(If)},
        {"ifdef",PpWord(Ifdef)},{"ifndef",PpWord(Ifndef)},{"elif",PpWord(Elif)},{"else",PpWord(Else)},
        {"endif",PpWord(Endif)},{"error",PpWord(Error)},{"warning",PpWord(Warning)},{"pragma",PpWord(Pragma)},
        {"line",PpWord(Line)},{"defined",PpWord(Defined)},{"__LINE__",PpWord(LineMac)},{"__FILE__",PpWord(FileMac)},{"once",PpWord(Once)},
    };
    BCtx *r;
    unsigned int i;
    TlsHeap *H = Where ? Where : TlsHeapFork();
    TlsHeapIn(H)
        rPush(r);
    TlsHeapOut
    r->TlsPpCtx.Static = &Static;
    r->H = H;
    r->head = 0; r->tail = &r->head; r->n = 0; r->once = 0;
    r->TlsPpCtx.Dict = D ? D : TlsDictNew(H);
    r->TlsPpCtx.Macros = 0;
    r->TlsPpCtx.Hook = 0;
    r->TlsPpCtx.Errors = r->TlsPpCtx.Warnings = 0;
    r->words = TlsIdMapNew(H);
    for (i=0;i<sizeof W/sizeof *W;i++)
        x_(r->words,Set,x_(r->TlsPpCtx.Dict,Id,(unsigned char *)W[i].name,(int)strlen(W[i].name)),(void *)(intptr_t)W[i].code);
    TlsHeapIn(H)
        r->TlsPpCtx.Macros = TlsMacroLibNew(r->TlsPpCtx.Dict,H);
    TlsHeapOut
    return &r->TlsPpCtx;
}
