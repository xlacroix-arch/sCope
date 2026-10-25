/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_PpCtx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static void Touch(char const *path) { FILE *f = fopen(path,"w"); CHECK(f); fputs("x\n",f); fclose(f); }

static void TestWords(void) {
    TlsPpCtx *c = TlsPpCtxNew(NULL,NULL);
    TlsDict *d = c->Dict;
    int id;
    CHECK(c->Dict && c->Macros);
    CHECK(x_(c,Word,x_(d,Id,(unsigned char *)"define",6))==PpWord(Define));
    CHECK(x_(c,Word,x_(d,Id,(unsigned char *)"ifndef",6))==PpWord(Ifndef));
    CHECK(x_(c,Word,x_(d,Id,(unsigned char *)"__FILE__",8))==PpWord(FileMac));
    CHECK(x_(c,Word,x_(d,Id,(unsigned char *)"defined",7))==PpWord(Defined));
    id = x_(d,Id,(unsigned char *)"Define",6);
    CHECK(x_(c,Word,id)==0 && x_(c,Word,0)==0 && x_(c,Word,99999)==0 && PpWord(nil)==0);
    switch (x_(c,Word,x_(d,Id,(unsigned char *)"endif",5))) { case PpWord(Endif): break; default: CHECK(0); }
    CHECK(sizeof(((struct c_TlsPpCtx *)0)->C.w)<256);
}
static void TestShared(void) {
    TlsDict *d = TlsDictNew(NULL);
    TlsPpCtx *a = TlsPpCtxNew(d,NULL),*b = TlsPpCtxNew(d,NULL);
    int n;
    CHECK(a->Dict==d && b->Dict==d && a->Macros!=b->Macros);
    n = x_(a->Macros,DefineStr,"ONE 1");
    CHECK(n && x_(a->Macros,Get,n) && !x_(b->Macros,Get,n));             /* the macros are not shared */
    CHECK(x_(b,Word,x_(d,Id,(unsigned char *)"include",7))==PpWord(Include));
    CHECK(x_(a,Word,x_(d,Id,(unsigned char *)"include",7))==PpWord(Include));
}
static void TestPaths(void) {
    char root[] = "/tmp/ppctxXXXXXX",p1[300],p2[300],p3[300],f[400];
    TlsPpCtx *c = TlsPpCtxNew(NULL,NULL);
    char *r;
    CHECK(mkdtemp(root));
    snprintf(p1,sizeof p1,"%s/inc1",root); snprintf(p2,sizeof p2,"%s/inc2/",root); snprintf(p3,sizeof p3,"%s/src",root);
    CHECK(!mkdir(p1,0755)); { char q[300]; snprintf(q,sizeof q,"%s/inc2",root); CHECK(!mkdir(q,0755)); } CHECK(!mkdir(p3,0755));
    snprintf(f,sizeof f,"%s/a.h",p1); Touch(f);
    snprintf(f,sizeof f,"%s/b.h",p1); Touch(f);
    snprintf(f,sizeof f,"%s/b.h",p2); Touch(f);                         /* b.h is in both */
    snprintf(f,sizeof f,"%s/c.h",p2); Touch(f);
    snprintf(f,sizeof f,"%s/local.h",p3); Touch(f);
    snprintf(f,sizeof f,"%s/main.c",p3); Touch(f);
    CHECK(x_(c,Path,0)==NULL && x_(c,Resolve,"a.h",0,NULL)==NULL);
    x_(c,AddPath,p1); x_(c,AddPath,p2);
    CHECK(!strcmp(x_(c,Path,0),p1) && !strncmp(x_(c,Path,1),p2,strlen(p2)-1) && strlen(x_(c,Path,1))==strlen(p2)-1 && x_(c,Path,2)==NULL && x_(c,Path,-1)==NULL);
    CHECK(x_(c,Path,1)[strlen(x_(c,Path,1))-1]=='2');                     /* the final / is gone */
    r = x_(c,Resolve,"a.h",0,NULL); snprintf(f,sizeof f,"%s/a.h",p1); CHECK(r && !strcmp(r,f));
    r = x_(c,Resolve,"b.h",0,NULL); snprintf(f,sizeof f,"%s/b.h",p1); CHECK(r && !strcmp(r,f));        /* the first one */
    r = x_(c,Resolve,"c.h",1,NULL); snprintf(f,sizeof f,"%s/inc2/c.h",root); CHECK(r && !strcmp(r,f));
    CHECK(x_(c,Resolve,"nope.h",0,NULL)==NULL && x_(c,Resolve,"",0,NULL)==NULL);
    CHECK(x_(c,Resolve,"local.h",0,NULL)==NULL);                          /* <local.h> is not on the paths */
    snprintf(f,sizeof f,"%s/main.c",p3);
    r = x_(c,Resolve,"local.h",1,f); snprintf(f,sizeof f,"%s/local.h",p3); CHECK(r && !strcmp(r,f));  /* "local.h": next to the includer */
    snprintf(f,sizeof f,"%s/main.c",p3);
    CHECK(x_(c,Resolve,"local.h",0,f)==NULL);
    r = x_(c,Resolve,"a.h",1,f); snprintf(f,sizeof f,"%s/a.h",p1); CHECK(r && !strcmp(r,f));            /* then the paths */
    snprintf(f,sizeof f,"%s/inc2/c.h",root);
    r = x_(c,Resolve,f,0,NULL); CHECK(r && !strcmp(r,f));                 /* absolute */
    snprintf(f,sizeof f,"%s/inc2/none.h",root);
    CHECK(x_(c,Resolve,f,0,NULL)==NULL);
    {   char cmd[400];
        snprintf(cmd,sizeof cmd,"rm -rf %s",root);
        CHECK(!system(cmd));
    }
}
/* a hook that writes down what it is told */
typedef struct { TlsPpHook TlsPpHook; int n,dirs,opens; int kinds[8]; unsigned int lines[8]; char texts[8][60],files[8][40]; int ret,lastWord,lastId; } Rec;
static void RMessage(TlsPpHook *this,int Kind,char const *File,unsigned int Line,char const *Text) {
    Rec *r = (Rec *)this;
    if (r->n<8) { r->kinds[r->n] = Kind; r->lines[r->n] = Line; strncpy(r->texts[r->n],Text,59); strncpy(r->files[r->n],File ? File : "(none)",39); }
    r->n++;
}
static int RDirective(TlsPpHook *this,int Word,int Id,TlsTok *t,char const *File,unsigned int Line) {
    Rec *r = (Rec *)this;
    (void)t; (void)File; (void)Line;
    r->dirs++; r->lastWord = Word; r->lastId = Id;
    return r->ret;
}
static void ROpen(TlsPpHook *this,char const *File,unsigned int Line,char const *Path) {
    Rec *r = (Rec *)this;
    (void)File; (void)Line; (void)Path;
    r->opens++;
}
static void TestHook(void) {
    static struct TlsPpHook full = {RMessage,RDirective,ROpen},msgonly = {RMessage,0,0};
    Rec r;
    TlsPpCtx *c = TlsPpCtxNew(NULL,NULL);
    memset(&r,0,sizeof r);
    /* no hook: nothing happens but the counts */
    x_(c,Report,PpMsgError,"f.c",3,"boom");
    x_(c,Report,PpMsgWarning,"f.c",4,"hmm");
    x_(c,Report,PpMsgInfo,NULL,5,"fyi");
    CHECK(c->Errors==1 && c->Warnings==1 && x_(c,Directive,0,0,NULL,"f.c",1)==0);
    x_(c,Opened,"f.c",1,"/x/y.h");
    c->Hook = &r.TlsPpHook;
    r.TlsPpHook.Static = &full;
    x_(c,Report,PpMsgError,"f.c",7,"#error stop");
    x_(c,Report,PpMsgInfo,NULL,9,"note");
    CHECK(r.n==2 && r.kinds[0]==PpMsgError && r.lines[0]==7 && !strcmp(r.texts[0],"#error stop") && !strcmp(r.files[0],"f.c"));
    CHECK(r.kinds[1]==PpMsgInfo && r.lines[1]==9 && !strcmp(r.files[1],"(none)") && c->Errors==2 && c->Warnings==1);
    r.ret = 1;
    CHECK(x_(c,Directive,PpWord(Pragma),42,NULL,"f.c",2)==1 && r.dirs==1 && r.lastWord==PpWord(Pragma) && r.lastId==42);
    r.ret = 0;
    CHECK(x_(c,Directive,0,43,NULL,"f.c",2)==0 && r.dirs==2 && r.lastWord==0);
    x_(c,Opened,"f.c",0,"f.c");
    CHECK(r.opens==1);
    /* a hook with only some of its methods */
    r.TlsPpHook.Static = &msgonly;
    CHECK(x_(c,Directive,0,1,NULL,"f.c",1)==0 && r.dirs==2);
    x_(c,Opened,"f.c",1,"p"); CHECK(r.opens==1);
    x_(c,Report,PpMsgWarning,"f.c",8,"w"); CHECK(r.n==3 && c->Warnings==2);
}
int main(void) {
    TlsStackNod *l;
    TlsMainHeapOpen(1<<20);
    l = TlsHeapLayer(); TestWords(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestShared(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestPaths(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestHook(); TlsHeapLayerPop(l);
    printf("all passed\n");
    return 0;
}
