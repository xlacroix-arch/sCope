/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Buffer.h>

#ifndef FileBlock
#define FileBlock 1024          /* must match Tools_Buffer.c: compile both with the same -DFileBlock */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

/* Reads a whole reader the way a parser does: walk p, call Check0 on a 0.
 * Data 0 (Check0 gives pos back) are kept and skipped. */
static int Slurp(TlsBuffer0 *b,unsigned char *out,int *zeros) {
    unsigned char *p = b->p;
    int n = 0;
    *zeros = 0;
    for (;;) {
        if (*p) { out[n++] = *p++; continue; }
        {   unsigned char *r = x_(b,Check0,p);
            if (r==TlsBufferEob) break;
            if (r==p) { out[n++] = 0; (*zeros)++; p++; continue; }
            p = r;
        }
    }
    return n;
}

static int OpenFds(void) {
    int n = 0;
    DIR *d = opendir("/proc/self/fd");
    while (readdir(d)) n++;
    closedir(d);
    return n;
}

static void TestEnds(void) {
    CHECK(&TlsBufferEob[0]!=&TlsBufferEof[0]);
    CHECK(*TlsBufferEob==0 && *TlsBufferEof==0);
    CHECK(x_(TlsBuffer0Null,Check0,(unsigned char *)TlsBufferEob)==TlsBufferEob);
    CHECK(TlsBuffer0Null->p==TlsBufferEob && TlsEmptyBuffer0.p==TlsBufferEob);
    CHECK(x_(&TlsEmptyBuffer0,Reset)==TlsBufferEob);
    x_(&TlsEmptyBuffer0,Close);
    printf("ends: ok\n");
}

static void TestString(void) {
    static unsigned char out[64];
    int z;
    TlsBuffer0 *b = TlsStringBuffer((unsigned char *)"hello");
    CHECK(b->p && *b->p=='h');
    CHECK(Slurp(b,out,&z)==5 && !memcmp(out,"hello",5) && z==0);
    CHECK(x_(b,Check0,(unsigned char *)TlsBufferEob)==TlsBufferEob);    /* the end is stable */
    CHECK(Slurp(b,out,&z)==0);                                          /* p is at the end */
    CHECK(*x_(b,Reset)=='h');
    CHECK(Slurp(b,out,&z)==5);
    b = TlsStringBuffer((unsigned char *)"");
    CHECK(Slurp(b,out,&z)==0);
    CHECK(x_(b,Check0,b->p)==TlsBufferEob);
    b = TlsStringBuffer((unsigned char *)"abc");
    x_(b,Close);
    CHECK(b->p==TlsBufferEob);
    printf("string: ok\n");
}

static void TestBuffer(void) {
    static unsigned char out[64];
    char src[] = {'a','b',0,'c',0,0,'d'};
    int z;
    TlsBuffer0 *b = TlsBufferBuffer(src,src+7);
    CHECK(b->p!=(unsigned char *)src);                                  /* a copy */
    CHECK(Slurp(b,out,&z)==7 && z==3 && !memcmp(out,src,7));
    CHECK(*x_(b,Reset)=='a');
    CHECK(Slurp(b,out,&z)==7);
    x_(b,Close);
    CHECK(x_(b,Check0,b->p)==TlsBufferEob);
    b = TlsBufferBuffer(src,src);                                       /* empty range */
    CHECK(Slurp(b,out,&z)==0 && z==0);
    b = TlsBufferBuffer(src+3,src);                                     /* reversed: empty too */
    CHECK(Slurp(b,out,&z)==0);
    printf("buffer (data 0, empty): ok\n");
}

static void MakeFile(const char *name,const unsigned char *d,size_t n) {
    FILE *f = fopen(name,"wb");
    CHECK(f);
    CHECK(fwrite(d,1,n,f)==n);
    fclose(f);
}
static void TestFile(void) {
    static unsigned char data[40000],out[40001];
    static const size_t sizes[] = {0,1,FileBlock-1,FileBlock,FileBlock+1,2*FileBlock,20000,40000};
    unsigned int i,s = 12345;
    int z,fd0,k;
    TlsBuffer0 *b;
    for (i=0;i<sizeof(data);i++) { s = s*1103515245u+12345u; data[i] = (unsigned char)(1+(s>>16)%250); }
    for (k=0;k<(int)(sizeof(sizes)/sizeof(sizes[0]));k++) {
        size_t n = sizes[k];
        MakeFile("buf_test.tmp",data,n);
        fd0 = OpenFds();
        b = TlsFileBuffer("buf_test.tmp");
        CHECK(b);
        CHECK(Slurp(b,out,&z)==(int)n && z==0 && !memcmp(out,data,n));
        CHECK(OpenFds()==fd0);                                          /* closed at the end of the stream */
        CHECK(x_(b,Check0,(unsigned char *)TlsBufferEob)==TlsBufferEob);
        CHECK(*x_(b,Reset)!=0 || n==0);                                 /* reopened */
        CHECK(Slurp(b,out,&z)==(int)n && !memcmp(out,data,n));
        CHECK(OpenFds()==fd0);
    }
    /* data 0 inside, across a block limit */
    data[100] = 0; data[FileBlock-1] = 0; data[FileBlock] = 0; data[39999] = 0;
    MakeFile("buf_test.tmp",data,40000);
    b = TlsFileBuffer("buf_test.tmp");
    CHECK(Slurp(b,out,&z)==40000 && z==4 && !memcmp(out,data,40000));
    /* a file that is not read to its end is closed by Close ... */
    fd0 = OpenFds();
    b = TlsFileBuffer("buf_test.tmp");
    CHECK(OpenFds()==fd0+1);
    x_(b,Close);
    CHECK(OpenFds()==fd0 && b->p==TlsBufferEob);
    CHECK(x_(b,Check0,b->p)==TlsBufferEob);
    /* ... or by the layer pop */
    {   TlsStackNod *l = TlsHeapLayer();
        b = TlsFileBuffer("buf_test.tmp");
        CHECK(OpenFds()==fd0+1);
        TlsHeapLayerPop(l);
        CHECK(OpenFds()==fd0);
    }
    CHECK(TlsFileBuffer("no/such/file")==0);
    remove("buf_test.tmp");
    printf("file (block limits, data 0, close, hook): ok\n");
}

static void TestCat(void) {
    static unsigned char out[50000],data[20000];
    int z,fd0;
    unsigned int i,s = 99;
    char e[1] = {0},xy[3] = {'x',0,'y'};
    TlsBuffer0 *a[7],*c;
    for (i=0;i<sizeof(data);i++) { s = s*1103515245u+12345u; data[i] = (unsigned char)(1+(s>>16)%250); }
    MakeFile("buf_test.tmp",data,sizeof(data));
    fd0 = OpenFds();
    a[0] = TlsStringBuffer((unsigned char *)"");
    a[1] = TlsStringBuffer((unsigned char *)"abc");
    a[2] = TlsBufferBuffer(e,e);                                        /* empty */
    a[3] = TlsFileBuffer("buf_test.tmp");
    a[4] = TlsBufferBuffer(xy,xy+3);                                           /* a data 0 */
    a[5] = TlsBuffer0Null;
    a[6] = TlsStringBuffer((unsigned char *)"de");
    c = TlsBufferCat(a,7);
    CHECK(Slurp(c,out,&z)==3+20000+3+2 && z==1);
    CHECK(!memcmp(out,"abc",3) && !memcmp(out+3,data,20000) && !memcmp(out+20003,"x\0y",3) && !memcmp(out+20006,"de",2));
    CHECK(x_(c,Check0,(unsigned char *)TlsBufferEob)==TlsBufferEob);
    CHECK(OpenFds()==fd0);
    CHECK(*x_(c,Reset)=='a');                                           /* the empty first piece is skipped */
    CHECK(Slurp(c,out,&z)==3+20000+3+2 && !memcmp(out+3,data,20000));
    x_(c,Close);
    CHECK(c->p==TlsBufferEob && x_(c,Check0,c->p)==TlsBufferEob);
    /* varargs, leading empties, all empty, none */
    c = TlsBufferCat0(TlsStringBuffer((unsigned char *)""),TlsStringBuffer((unsigned char *)""),TlsStringBuffer((unsigned char *)"z"),(TlsBuffer0 *)0);
    CHECK(*c->p=='z');
    CHECK(Slurp(c,out,&z)==1);
    c = TlsBufferCat0(TlsStringBuffer((unsigned char *)""),TlsBuffer0Null,(TlsBuffer0 *)0);
    CHECK(c->p==TlsBufferEob && Slurp(c,out,&z)==0);
    c = TlsBufferCat0((TlsBuffer0 *)0);
    CHECK(c->p==TlsBufferEob);
    c = TlsBufferCat(a,0);
    CHECK(c->p==TlsBufferEob && x_(c,Reset)==TlsBufferEob);
    /* a cat of cats */
    c = TlsBufferCat0(TlsBufferCat0(TlsStringBuffer((unsigned char *)"ab"),TlsStringBuffer((unsigned char *)""),(TlsBuffer0 *)0),
                      TlsBufferCat0(TlsStringBuffer((unsigned char *)""),TlsStringBuffer((unsigned char *)"cd"),(TlsBuffer0 *)0),(TlsBuffer0 *)0);
    CHECK(Slurp(c,out,&z)==4 && !memcmp(out,"abcd",4));
    remove("buf_test.tmp");
    printf("cat (empty pieces, file, data 0, nested, varargs): ok\n");
}


/*_____________________________________________________________________________
 | Take, Skip, Replace, macros.
 |_____________________________________________________________________________*/

static unsigned int Seed = 4242;
static unsigned int Rnd(void) { Seed = Seed*1103515245u+12345u; return (Seed>>16)&0x7fff; }

static int Same(TlsBuffer0 *b,const char *exp,int n) {
    static unsigned char out[200000];
    int z,k = Slurp(b,out,&z);
    return k==n && !memcmp(out,exp,(size_t)n);
}
static TlsBuffer0 *Str(const char *s) { return TlsStringBuffer((unsigned char *)s); }
static TlsBuffer0 *Raw(const char *s,int n) { return TlsBufferBuffer((char *)s,(char *)s+n); }

static void TestTakeSkip(void) {
    static unsigned char data[3000];
    unsigned int i;
    TlsBuffer0 *b,*c;
    CHECK(Same(TlsBufferTake(Str("hello world"),5),"hello",5));
    CHECK(Same(TlsBufferTake(Str("hello"),50),"hello",5));              /* more than there is */
    CHECK(Same(TlsBufferTake(Str("hello"),0),"",0));
    CHECK(Same(TlsBufferTake(Str("hello"),-3),"",0));
    CHECK(Same(TlsBufferTake(Str(""),4),"",0));
    CHECK(Same(TlsBufferSkip(Str("hello world"),6),"world",5));
    CHECK(Same(TlsBufferSkip(Str("hello"),50),"",0));
    CHECK(Same(TlsBufferSkip(Str("hello"),5),"",0));
    CHECK(Same(TlsBufferSkip(Str("hello"),0),"hello",5));
    CHECK(Same(TlsBufferSkip(Str("hello"),-1),"hello",5));
    /* one Src: Take then Skip follow each other */
    b = Str("0123456789");
    CHECK(Same(TlsBufferTake(b,3),"012",3));
    CHECK(Same(TlsBufferSkip(b,2),"56789",5));
    /* the same on a long, data 0 rich stream spread on several pieces and a file */
    for (i=0;i<sizeof(data);i++) data[i] = (unsigned char)(Rnd()%7 ? 1+Rnd()%250 : 0);
    MakeFile("buf_test.tmp",data,sizeof(data));
    for (i=0;i<200;i++) {
        int a = (int)(Rnd()%3200),k = (int)(Rnd()%3200),x;
        TlsStackNod *l = TlsHeapLayer();
        int fd0 = OpenFds();
        c = TlsBufferCat0(Raw((char *)data,1000),TlsFileBuffer("buf_test.tmp") ? Str("") : 0,Raw((char *)data+1000,2000),(TlsBuffer0 *)0);
        /* data = [0,1000) + [1000,3000) = 3000 bytes; the file piece is not read: closed with the layer */
        x = a>3000 ? 3000 : a;
        {   TlsBuffer0 *t = TlsBufferTake(c,a);
            CHECK(Same(t,(char *)data,x));
        }
        {   int y = k>3000-x ? 3000-x : k;
            TlsBuffer0 *s = TlsBufferSkip(c,k);
            CHECK(Same(s,(char *)data+x+y,3000-x-y));
        }
        TlsHeapLayerPop(l);
        CHECK(OpenFds()==fd0);
    }
    /* a file as Src, with Take then Skip over several blocks */
    b = TlsFileBuffer("buf_test.tmp");
    CHECK(Same(TlsBufferTake(b,1500),(char *)data,1500));
    CHECK(Same(TlsBufferSkip(b,100),(char *)data+1600,1400));
    /* Reset restarts from the start of Src */
    b = Str("abcdef");
    c = TlsBufferTake(b,3);
    CHECK(Same(c,"abc",3));
    x_(c,Reset);
    CHECK(Same(c,"abc",3));
    remove("buf_test.tmp");
    printf("take/skip: ok\n");
}

static void TestReplace(void) {
    static unsigned char base[600],edit[100],exp[800];
    int it;
    CHECK(Same(TlsBufferReplace(Str("hello world"),Str("there"),6,11),"hello there",11));
    CHECK(Same(TlsBufferReplace(Str("hello world"),Str("big "),6,6),"hello big world",15));       /* insert */
    CHECK(Same(TlsBufferReplace(Str("hello"),Str(">> "),0,0),">> hello",8));
    CHECK(Same(TlsBufferReplace(Str("hello"),Str("!"),5,5),"hello!",6));
    CHECK(Same(TlsBufferReplace(Str("hello"),Str("!"),9,12),"hello!",6));                         /* past the end */
    CHECK(Same(TlsBufferReplace(Str("hello"),Str(""),1,4),"ho",2));                               /* a cut */
    CHECK(Same(TlsBufferReplace(Str("hello"),0,0,5),"",0));
    CHECK(Same(TlsBufferReplace(Str("hello"),Str("x"),3,1),"helxlo",6));                          /* e<b is an insert at b */
    {   TlsBuffer0 *r = TlsBufferReplace(Str("hello world"),Str("there"),6,11);
        CHECK(Same(r,"hello there",11));
        x_(r,Reset);
        CHECK(Same(r,"hello there",11));                                                          /* it can start over */
    }
    for (it=0;it<1500;it++) {
        int lb = (int)(Rnd()%600),le = (int)(Rnd()%100),b,e,i,n = 0;
        TlsStackNod *l = TlsHeapLayer();
        TlsBuffer0 *B;
        Seed += (unsigned)it;
        for (i=0;i<lb;i++) base[i] = (unsigned char)(Rnd()%9 ? 1+Rnd()%250 : 0);
        for (i=0;i<le;i++) edit[i] = (unsigned char)(Rnd()%9 ? 1+Rnd()%250 : 0);
        b = (int)(Rnd()%(unsigned)(lb+3)); e = b+(int)(Rnd()%40);
        if (b>lb) b = lb;
        if (e>lb) e = lb;
        if (e<b) e = b;
        memcpy(exp,base,(size_t)b); n = b;
        memcpy(exp+n,edit,(size_t)le); n += le;
        memcpy(exp+n,base+e,(size_t)(lb-e)); n += lb-e;
        /* Base cut in pieces of random sizes, as a cat */
        {   TlsBuffer0 *pc[40]; int np = 0,at = 0;
            while (at<lb && np<39) { int k = 1+(int)(Rnd()%90); if (at+k>lb || np==38) k = lb-at; pc[np++] = Raw((char *)base+at,k); at += k; if (Rnd()%4==0) pc[np++] = Str(""); }
            B = TlsBufferCat(pc,np);
        }
        CHECK(Same(TlsBufferReplace(B,Raw((char *)edit,le),b,e),(char *)exp,n));
        TlsHeapLayerPop(l);
    }
    printf("replace (fuzz against a plain model): ok\n");
}

static void TestMacro(void) {
    TlsBuffMacroDef *d;
    TlsBuffMacro *m;
    TlsBuffer0 *V[3],*r1,*r2;
    char t[] = "Hello NAME, you are AGE! NAME";
    /* t: NAME at [6,10), AGE at [20,23), NAME at [25,29) */
    d = TlsBuffMacroDefNew(Str(t));
    CHECK(x_(d,AddSubstitution,25,29,0)==0);                    /* out of order on purpose */
    CHECK(x_(d,AddSubstitution,6,10,0)==0);
    CHECK(x_(d,AddSubstitution,20,23,1)==0);
    CHECK(x_(d,AddSubstitution,8,12,2)==-1);                    /* overlaps [6,10) */
    CHECK(x_(d,AddSubstitution,7,7,2)==-1);                     /* an insert inside [6,10) */
    CHECK(x_(d,AddSubstitution,6,6,2)==0);                      /* an insert at its edge is fine */
    CHECK(x_(d,AddSubstitution,10,10,2)==0);
    CHECK(x_(d,AddSubstitution,-1,3,0)==-1 && x_(d,AddSubstitution,3,2,0)==-1);
    CHECK(x_(d,AddSubstitution,0,99,0)==-1 && x_(d,AddSubstitution,0,1,-1)==-1);
    CHECK(x_(d,AddSubstitution,10,20,9)==0);                    /* touches both: ", you are " replaced */
    m = x_(d,Generate);
    CHECK(x_(m,ParamNumber)==10);
    d = TlsBuffMacroDefNew(Str(t));
    x_(d,AddSubstitution,25,29,0); x_(d,AddSubstitution,6,10,0); x_(d,AddSubstitution,20,23,1);
    m = x_(d,Generate);
    CHECK(x_(m,ParamNumber)==2);
    V[0] = Str("Bob"); V[1] = Str("42");
    r1 = x_(m,Apply,V);
    CHECK(Same(r1,"Hello Bob, you are 42! Bob",26));            /* NAME used twice: read again */
    /* independent results, Apply as often as wanted */
    V[0] = Str("Alice"); V[1] = Str("");
    r2 = x_(m,Apply,V);
    V[0] = Str("X"); V[1] = Str("Y");
    r1 = x_(m,Apply,V);
    CHECK(Same(r2,"Hello Alice, you are ! Alice",28));
    CHECK(Same(r1,"Hello X, you are Y! X",21));
    x_(r1,Reset);
    CHECK(Same(r1,"Hello X, you are Y! X",21));                 /* Reset starts it over */
    x_(m,Apply,0);                                              /* no Var at all: empty holes */
    CHECK(Same(x_(m,Apply,0),"Hello , you are ! ",18));
    V[0] = 0; V[1] = Str("7");
    CHECK(Same(x_(m,Apply,V),"Hello , you are 7! ",19));
    /* nesting: a Var that is a macro result, itself with a repeated Var */
    {   TlsBuffMacroDef *d2 = TlsBuffMacroDefNew(Str("<>"));
        TlsBuffMacro *m2;
        TlsBuffer0 *W[1];
        x_(d2,AddSubstitution,1,1,0);
        x_(d2,AddSubstitution,2,2,0);
        m2 = x_(d2,Generate);                                    /* "<" P ">" P */
        W[0] = Str("ab");
        V[0] = x_(m2,Apply,W);
        CHECK(Same(V[0],"<ab>ab",6));
        W[0] = Str("ab");
        V[0] = x_(m2,Apply,W);                                  /* "<ab>ab" */
        V[1] = Str("!");
        CHECK(Same(x_(m,Apply,V),"Hello <ab>ab, you are !! <ab>ab",31));
    }
    /* a Var that is a file, used twice: reopened for the second use, closed at the end */
    {   int fd0,z;
        static unsigned char buf[20000],out[60000];
        unsigned int i;
        TlsBuffMacroDef *d3 = TlsBuffMacroDefNew(Str("[]"));
        TlsBuffer0 *W[1],*r;
        for (i=0;i<sizeof(buf);i++) buf[i] = (unsigned char)(1+Rnd()%250);
        MakeFile("buf_test.tmp",buf,sizeof(buf));
        x_(d3,AddSubstitution,1,1,0); x_(d3,AddSubstitution,1,1,0);
        fd0 = OpenFds();
        W[0] = TlsFileBuffer("buf_test.tmp");
        r = x_(x_(d3,Generate),Apply,W);
        CHECK(Slurp(r,out,&z)==2+2*20000 && out[0]=='[' && !memcmp(out+1,buf,20000) && !memcmp(out+1+20000,buf,20000) && out[40001]==']');
        CHECK(OpenFds()==fd0);
        remove("buf_test.tmp");
    }
    /* data 0 in the text and in a Var */
    {   char tx[5] = {'a',0,'b','c',0};
        TlsBuffMacroDef *d4 = TlsBuffMacroDefNew(Raw(tx,5));
        char vv[3] = {0,'z',0};
        TlsBuffer0 *W[1];
        x_(d4,AddSubstitution,2,3,0);
        W[0] = Raw(vv,3);
        CHECK(Same(x_(x_(d4,Generate),Apply,W),"a\0\0z\0c\0",7));
    }
    /* an empty text, and no hole at all */
    CHECK(Same(x_(x_(TlsBuffMacroDefNew(Str("")),Generate),Apply,0),"",0));
    {   TlsBuffMacroDef *d5 = TlsBuffMacroDefNew(Str("plain"));
        TlsBuffMacro *m5 = x_(d5,Generate);
        CHECK(x_(m5,ParamNumber)==0 && Same(x_(m5,Apply,0),"plain",5));
        x_(d5,AddSubstitution,0,5,0);                           /* the definition goes on, and generates again */
        V[0] = Str("all");
        CHECK(Same(x_(x_(d5,Generate),Apply,V),"all",3) && Same(x_(m5,Apply,0),"plain",5));
    }
    /* the text can be any stream, and is Reset: a half read one is read whole */
    {   TlsBuffer0 *b = Str("0123456789");
        TlsBuffMacroDef *d6;
        x_(b,Check0,b->p);
        d6 = TlsBuffMacroDefNew(TlsBufferCat0(Str("0123"),Str("456789"),(TlsBuffer0 *)0));
        x_(d6,AddSubstitution,4,6,0);
        V[0] = Str("-");
        CHECK(Same(x_(x_(d6,Generate),Apply,V),"0123-6789",9));
    }
    printf("macro (holes, reuse, nesting, file, data 0): ok\n");
}

static void TestMacroFuzz(void) {
    int it;
    for (it=0;it<800;it++) {
        static unsigned char txt[300],exp[20000],vv[4][60];
        int vl[4],len = (int)(Rnd()%250),nh = (int)(Rnd()%7),i,n = 0,pos = 0,np = 0;
        int hb[8],he[8],hp[8];
        TlsStackNod *l = TlsHeapLayer();
        TlsBuffMacroDef *d;
        TlsBuffMacro *m;
        TlsBuffer0 *V[4],*r;
        for (i=0;i<len;i++) txt[i] = (unsigned char)(Rnd()%11 ? 1+Rnd()%250 : 0);
        for (i=0;i<4;i++) { int k; vl[i] = (int)(Rnd()%50); for (k=0;k<vl[i];k++) vv[i][k] = (unsigned char)(Rnd()%11 ? 1+Rnd()%250 : 0); }
        /* holes: sorted, non overlapping, some inserts, some touching */
        for (i=0;i<nh && pos<=len;i++) {
            int b = pos+(int)(Rnd()%40),e;
            if (b>len) break;
            e = (Rnd()%3==0) ? b : b+(int)(Rnd()%30);
            if (e>len) e = len;
            hb[i] = b; he[i] = e; hp[i] = (int)(Rnd()%4);
            pos = e;
        }
        nh = i;
        d = TlsBuffMacroDefNew(Raw((char *)txt,len));
        {   int ord[8],srt[8],k,j;                              /* add them in a shuffled order */
            for (k=0;k<nh;k++) ord[k] = k;
            for (k=nh-1;k>0;k--) { int j2 = (int)(Rnd()%(unsigned)(k+1)),tmp = ord[k]; ord[k] = ord[j2]; ord[j2] = tmp; }
            for (k=0;k<nh;k++) CHECK(x_(d,AddSubstitution,hb[ord[k]],he[ord[k]],hp[ord[k]])==0);
            /* the model: sorted by (b,e), and for equal ones in the order they were added
             * (a stable insertion sort of the order of the calls) */
            for (k=0;k<nh;k++) {
                for (j=k;j>0 && (hb[srt[j-1]]>hb[ord[k]] || (hb[srt[j-1]]==hb[ord[k]] && he[srt[j-1]]>he[ord[k]]));j--) srt[j] = srt[j-1];
                srt[j] = ord[k];
            }
            for (pos=0,k=0;k<nh;k++) {
                int h = srt[k];
                memcpy(exp+n,txt+pos,(size_t)(hb[h]-pos)); n += hb[h]-pos;
                memcpy(exp+n,vv[hp[h]],(size_t)vl[hp[h]]); n += vl[hp[h]];
                pos = he[h];
                if (hp[h]+1>np) np = hp[h]+1;
            }
        }
        memcpy(exp+n,txt+pos,(size_t)(len-pos)); n += len-pos;
        m = x_(d,Generate);
        CHECK(x_(m,ParamNumber)==np);
        for (i=0;i<4;i++) V[i] = Raw((char *)vv[i],vl[i]);
        r = x_(m,Apply,V);
        CHECK(Same(r,(char *)exp,n));
        x_(r,Reset);
        CHECK(Same(r,(char *)exp,n));
        TlsHeapLayerPop(l);
    }
    printf("macro (fuzz against a plain model): ok\n");
}

int main(void) {
    TlsStackNod *l;
    TlsMainHeapOpen(65536);
    l = TlsHeapLayer(); TestEnds(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestString(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestBuffer(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestFile(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestCat(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestTakeSkip(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestReplace(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestMacro(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestMacroFuzz(); TlsHeapLayerPop(l);
    printf("all passed\n");
    return 0;
}
