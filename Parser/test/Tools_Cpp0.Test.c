/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Core.h>
#include <Tools_Buffer.h>
#include <Tools_Cpp0.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CppBlock
#define CppBlock 512
#endif
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static unsigned long long Seed = 88172645463325252ULL;
static unsigned Rnd(void) { Seed ^= Seed<<13; Seed ^= Seed>>7; Seed ^= Seed<<17; return (unsigned)(Seed>>11); }

/*_____________________________________________________________________________
 | The hook of the tests: records everything it is told.
 |_____________________________________________________________________________
*/
typedef struct { unsigned long pos; unsigned int outLine,orig; } Mk;
typedef struct {
    TlsCpp0Hook TlsCpp0Hook;
    Mk m[4000];
    int nm,nw,nr;
    unsigned int warn[8];
} THook;
static void HMark(TlsCpp0Hook *this,unsigned long OutPos,unsigned int OutLine,unsigned int OrigLine) {
    THook *h = (THook *)this;
    CHECK(h->nm<4000);
    h->m[h->nm].pos = OutPos; h->m[h->nm].outLine = OutLine; h->m[h->nm].orig = OrigLine; h->nm++;
}
static void HWarn(TlsCpp0Hook *this,unsigned int OrigLine,char *Msg) {
    THook *h = (THook *)this;
    CHECK(!strcmp(Msg,"unterminated comment") && h->nw<8);
    h->warn[h->nw++] = OrigLine;
}
static void HReset(TlsCpp0Hook *this) { THook *h = (THook *)this; h->nm = h->nw = 0; h->nr++; }
static struct TlsCpp0Hook HookStatic = {HMark,HWarn,HReset};
static THook *NewHook(void) {
    THook *h;
    rPush(h);
    h->TlsCpp0Hook.Static = &HookStatic;
    h->nm = h->nw = h->nr = 0;
    return h;
}

/* Reads a whole reader the way a parser does. *zeros counts the data 0. */
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

/* The same text cut in pieces at random places: the pieces may be empty. */
static TlsBuffer0 *Pieces(unsigned char *s,int len,int cuts) {
    TlsBuffer0 *a[40];
    int c[40],i,k,n = 0,at = 0;
    if (cuts>30) cuts = 30;
    for (i=0;i<cuts;i++) c[i] = len ? (int)(Rnd()%(unsigned)(len+1)) : 0;
    for (i=1;i<cuts;i++) for (k=i;k>0 && c[k-1]>c[k];k--) { int t = c[k]; c[k] = c[k-1]; c[k-1] = t; }
    for (i=0;i<cuts;i++) { a[n++] = TlsBufferBuffer((char *)s+at,(char *)s+c[i]); at = c[i]; }
    a[n++] = TlsBufferBuffer((char *)s+at,(char *)s+len);
    return TlsBufferCat(a,n);
}

/*_____________________________________________________________________________
 | The model: the whole buffer, one phase after the other, the lines of every byte
 | carried along. It shares nothing with the filter but the specification.
 |_____________________________________________________________________________
*/
#define MaxIn 40000
typedef struct { int kind; unsigned char ch; unsigned int line; } Item;      /* kind: 0 byte, 1 blank, 2 line end */

static int Model(unsigned char *in,int len,int flags,unsigned char *out,Mk *mk,int *nmk,unsigned int *warn,int *nwarn) {
    static unsigned char b[MaxIn],b2[MaxIn];
    static unsigned int l[MaxIn],l2[MaxIn];
    static Item it[3*MaxIn],t2[3*MaxIn],ln[3*MaxIn];
    int i,m = 0,m2 = 0,ni = 0,no = 0,nt,nl,k,outLine = 1;
    unsigned int line = 1,prev = 0;
    unsigned long pos = 0;
    *nmk = 0; *nwarn = 0;
    i = (len>=3 && in[0]==0xEF && in[1]==0xBB && in[2]==0xBF) ? 3 : 0;
    for (;i<len;i++) {                                  /* line ends */
        unsigned char c = in[i]==0 ? ' ' : in[i];       /* a NUL is a space, a literal's too */
        if (c=='\r' || c=='\n') {
            if (c=='\r' && i+1<len && in[i+1]=='\n') i++;
            b[m] = '\n'; l[m] = line++; m++;
        } else { b[m] = c; l[m] = line; m++; }
    }
    for (i=0;i<m;i++) {                                 /* continued lines */
        if (b[i]=='\\' && i+1<m && b[i+1]=='\n') { i++; continue; }
        b2[m2] = b[i]; l2[m2] = l[i]; m2++;
    }
    for (i=0;i<m2;) {                                   /* comments, literals, blanks */
        unsigned char c = b2[i];
        if (c=='/' && i+1<m2 && b2[i+1]=='/') {
            it[ni].kind = 1; it[ni].line = l2[i]; ni++;
            i += 2;
            while (i<m2 && b2[i]!='\n') i++;
        } else if (c=='/' && i+1<m2 && b2[i+1]=='*') {
            int j = i+2;
            it[ni].kind = 1; it[ni].line = l2[i]; ni++;
            while (j+1<m2 && !(b2[j]=='*' && b2[j+1]=='/')) j++;
            if (j+1<m2) i = j+2;
            else { warn[(*nwarn)++] = l2[i]; i = m2; }
        } else if (c=='"' || c=='\'') {
            unsigned char q = c;
            it[ni].kind = 0; it[ni].ch = c; it[ni].line = l2[i]; ni++;
            i++;
            while (i<m2 && b2[i]!='\n') {
                unsigned char d = b2[i];
                it[ni].kind = 0; it[ni].ch = d; it[ni].line = l2[i]; ni++;
                i++;
                if (d==q) break;
                if (d=='\\' && i<m2 && b2[i]!='\n') { it[ni].kind = 0; it[ni].ch = b2[i]; it[ni].line = l2[i]; ni++; i++; }
            }
        } else {
            if (c=='\n') it[ni].kind = 2;
            else if (c==' ' || c=='\t' || c=='\v' || c=='\f' || c==0) it[ni].kind = 1;
            else it[ni].kind = 0;
            it[ni].ch = c; it[ni].line = l2[i]; ni++;
            i++;
        }
    }
    /* output lines */
    for (i=0;i<=ni;) {
        int j = i,end;
        while (j<ni && it[j].kind!=2) j++;
        end = j;                                        /* it[end] is the line end, or the end of the input */
        nt = 0;
        for (k=i;k<end;k++) { if (it[k].kind==1 && nt && t2[nt-1].kind==1) continue; t2[nt++] = it[k]; }
        if (flags&Cpp0Trim) {
            nl = 0;
            if (nt && t2[0].kind==1) { memmove(t2,t2+1,(size_t)(nt-1)*sizeof(Item)); nt--; }
            if (nt && t2[nt-1].kind==1) nt--;
            (void)nl;
        }
        if (end<ni || nt) {                             /* a line, closed by its \n (the last gets one) */
            unsigned int first = nt ? t2[0].line : (end<ni ? it[end].line : line);
            if (first!=prev+1) { mk[*nmk].pos = pos; mk[*nmk].outLine = (unsigned)outLine; mk[*nmk].orig = first; (*nmk)++; }
            prev = first;
            for (k=0;k<nt;k++) out[no++] = t2[k].kind==1 ? ' ' : t2[k].ch;
            out[no++] = '\n';
            pos = (unsigned long)no;
            outLine++;
        }
        i = end+1;
    }
    (void)ln;
    return no;
}

static int Same(TlsBuffer0 *r,const char *exp,int n) {
    static unsigned char out[8*MaxIn];
    int z,got = Slurp(r,out,&z);
    return got==n && !z && !memcmp(out,exp,(size_t)n);
}
static TlsBuffer0 *Str(const char *s) { return TlsStringBuffer((unsigned char *)s); }
#define RUN(src,flags,exp) CHECK(Same(TlsCpp0Filter(Str(src),flags,0),exp,(int)strlen(exp)))

static void TestSpec(void) {
    /* blanks */
    RUN("a  b\t\tc\n",Cpp0Default,"a b c\n");
    RUN("  \t a \v\f b  \n",Cpp0Trim,"a b\n");
    RUN("  \t a \v\f b  \n",0," a b \n");
    /* line ends */
    RUN("a\r\nb\rc\nd\n\re",Cpp0Default,"a\nb\nc\nd\n\ne\n");
    /* continued lines, also in the middle of a word, a literal and a comment */
    RUN("#define X 1 \\\n + 2\n",Cpp0Default,"#define X 1 + 2\n");
    RUN("ab\\\ncd\\\r\nef\n",Cpp0Default,"abcdef\n");
    RUN("\"a\\\nb\" /* c\\\nd */ e\n",Cpp0Default,"\"ab\" e\n");
    RUN("a // c \\\n still a comment\nb\n",Cpp0Default,"a\nb\n");
    RUN("a\\\\\nb\n",Cpp0Default,"a\\b\n");               /* \\ then a line end: the last \ continues */
    RUN("a \\ \nb\n",Cpp0Default,"a \\\nb\n");             /* a blank after the backslash: no continuation */
    /* comments: one blank each, and never a glue */
    RUN("a/**/b\n",Cpp0Default,"a b\n");
    RUN("x/*c*/y // z\nw\n",Cpp0Default,"x y\nw\n");
    RUN("a /* \n \n */ b\nc\n",Cpp0Default,"a b\nc\n");
    RUN("/*/ not closed yet */x\n",Cpp0Default,"x\n");
    RUN("/**/\n",Cpp0Default,"\n");
    RUN("/**/\n",0," \n");
    RUN("a / b /= c\n",Cpp0Default,"a / b /= c\n");       /* a slash is a slash */
    RUN("a/",Cpp0Default,"a/\n");
    /* literals are left alone */
    RUN("s = \"a  /* b */ // \t\" ; \n",Cpp0Default,"s = \"a  /* b */ // \t\" ;\n");
    RUN("c = '\"' ; d = \"\\\"//\" ; e = '\\'' /*x*/;\n",Cpp0Default,"c = '\"' ; d = \"\\\"//\" ; e = '\\'' ;\n");
    RUN("#error don't do it // here\nint x;\n",Cpp0Default,"#error don't do it // here\nint x;\n");  /* the ' ends with the line, and what follows it stays */
    RUN("#error don't /* a\nb */ c\n",Cpp0Default,"#error don't /* a\nb */ c\n");
    RUN("\"abc\nx /* c */ y\n",Cpp0Default,"\"abc\nx y\n");
    /* digit separators */
    RUN("n = 1'000'000 + 0xff'ff + 1.5'3 ; c = 'a' ; w = L'b' ; u = u8'c'\n",Cpp0Default,
        "n = 1'000'000 + 0xff'ff + 1.5'3 ; c = 'a' ; w = L'b' ; u = u8'c'\n");
    RUN("x = 1'/*c*/ '\n",Cpp0Default,"x = 1' '\n");                       /* a separator, then a blank, then a ' that is alone */
    RUN("a = 1 '\"' /* c */\n",Cpp0Default,"a = 1 '\"'\n");               /* not stuck to the number: a literal */
    RUN("a = 1'\"' /* c */\n",Cpp0Default,"a = 1'\"' /* c */\n");          /* stuck: a separator, then a string */
    RUN("a = 1'\"' /* c */\n",Cpp0Default&~Cpp0DigitSep,"a = 1'\"'\n");
    /* the ends */
    RUN("",Cpp0Default,"");
    RUN("   ",Cpp0Default,"");
    RUN("   ",0," \n");
    RUN("a",Cpp0Default,"a\n");
    RUN("a\n\n\nb",Cpp0Default,"a\n\n\nb\n");
    RUN("a\\\n",Cpp0Default,"a\n");
    RUN("a\\",Cpp0Default,"a\\\n");
    RUN("\xEF\xBB\xBF" "a\n",Cpp0Default,"a\n");                            /* the BOM goes */
    RUN("\xEF\xBB" "a\n",Cpp0Default,"\xEF\xBB" "a\n");                     /* a part of it stays */
    RUN("x\xEF\xBB\xBF" "a\n",Cpp0Default,"x\xEF\xBB\xBF" "a\n");          /* not at the start */
    RUN("\xC3\xA9 = \"\xC3\xA9\"\n",Cpp0Default,"\xC3\xA9 = \"\xC3\xA9\"\n");
    {   char z[] = {'a',0,0,'b','\n'};                                      /* a NUL is a blank */
        CHECK(Same(TlsCpp0Filter(TlsBufferBuffer(z,z+5),Cpp0Default,0),"a b\n",4));
    }
    {   char z[] = {'"','a',0,'b','"','\n'};                               /* ... in a literal too */
        CHECK(Same(TlsCpp0Filter(TlsBufferBuffer(z,z+6),Cpp0Default,0),"\"a b\"\n",6));
    }
    printf("cpp0 spec: ok\n");
}

static void TestHook(void) {
    THook *h = NewHook();
    TlsBuffer0 *f;
    /* nothing lost: no mark */
    f = TlsCpp0Filter(Str("a\nb\n\nc\n"),Cpp0Default,&h->TlsCpp0Hook);
    CHECK(Same(f,"a\nb\n\nc\n",7) && h->nm==0 && h->nw==0);
    /* a multi line comment, a continued line */
    h->nm = 0;
    f = TlsCpp0Filter(Str("a\n/* x\ny */ b\nc\\\nd\ne\n"),Cpp0Default,&h->TlsCpp0Hook);
    CHECK(Same(f,"a\nb\ncd\ne\n",9));
    /* original lines: a=1, b=3, cd=4 (continued on 5), e=6 */
    CHECK(h->nm==2);
    CHECK(h->m[0].pos==2 && h->m[0].outLine==2 && h->m[0].orig==3);
    CHECK(h->m[1].pos==7 && h->m[1].outLine==4 && h->m[1].orig==6);
    /* a comment first: the first line is not line 1 */
    h->nm = 0;
    f = TlsCpp0Filter(Str("/*\n\n*/ a\nb\n"),Cpp0Default,&h->TlsCpp0Hook);
    CHECK(Same(f,"a\nb\n",4) && h->nm==1 && h->m[0].pos==0 && h->m[0].outLine==1 && h->m[0].orig==3);
    /* a warning, at the line where the comment began */
    h->nm = h->nw = 0;
    f = TlsCpp0Filter(Str("a\nb /* never\nclosed\n"),Cpp0Default,&h->TlsCpp0Hook);
    CHECK(Same(f,"a\nb\n",4) && h->nw==1 && h->warn[0]==2);
    /* Reset: the hook is told, the marks come again, the output is the same */
    h->nm = h->nw = 0;
    f = TlsCpp0Filter(Str("a\n/* x\ny */ b\n"),Cpp0Default,&h->TlsCpp0Hook);
    CHECK(Same(f,"a\nb\n",4) && h->nm==1);
    x_(f,Reset);
    CHECK(h->nm==0 && h->nr==1);
    CHECK(Same(f,"a\nb\n",4) && h->nm==1);
    /* TlsCpp0OutPos: from a pointer in the current block */
    {   TlsBuffer0 *g = TlsCpp0Filter(Str("ab\ncd\n"),Cpp0Default,0);
        unsigned char *p = x_(g,Check0,g->p);
        CHECK(TlsCpp0OutPos(g,p)==0);
        if (CppBlock>=6) CHECK(TlsCpp0OutPos(g,p+4)==4);
        CHECK(TlsCpp0OutPos(TlsBuffer0Null,p)==(unsigned long)-1);
    }
    /* Close */
    f = TlsCpp0Filter(Str("a\n"),Cpp0Default,0);
    x_(f,Close);
    CHECK(f->p==TlsBufferEob && x_(f,Check0,f->p)==TlsBufferEob);
    printf("cpp0 hook: ok\n");
}

static void TestFuzz(void) {
    static unsigned char in[MaxIn],out[8*MaxIn],exp[8*MaxIn];
    static const unsigned char alpha[] = {'a','b','1','.','_',' ',' ','\t','\v','\f','\r','\n','\n','\\','\\','/','/','*','*','"','\'',0,'x','/','*'};
    static Mk em[4000];
    int it,fails = 0;
    for (it=0;it<6000;it++) {
        int len = (int)(Rnd()%130),i,n,z,nem,ew,got,flags = (Rnd()&1) ? Cpp0Trim : 0;
        unsigned int ewarn[4];
        TlsStackNod *l = TlsHeapLayer();
        THook *h = NewHook();
        TlsBuffer0 *f,*src;
        for (i=0;i<len;i++) in[i] = alpha[Rnd()%sizeof(alpha)];
        if (len>3 && Rnd()%8==0) { in[0] = 0xEF; in[1] = 0xBB; in[2] = (Rnd()&1) ? 0xBF : 0xBE; }
        n = Model(in,len,flags,exp,em,&nem,ewarn,&ew);
        src = (Rnd()%3) ? Pieces(in,len,(int)(Rnd()%6)) : TlsBufferBuffer((char *)in,(char *)in+len);
        f = TlsCpp0Filter(src,flags,&h->TlsCpp0Hook);
        got = Slurp(f,out,&z);
        if (got!=n || z || memcmp(out,exp,(size_t)n)) {
            printf("MISMATCH it=%d flags=%d len=%d\n in : ",it,flags,len);
            for (i=0;i<len;i++) printf("%02x ",in[i]);
            printf("\n exp: ");
            for (i=0;i<n;i++) printf("%02x ",exp[i]);
            printf("\n got: ");
            for (i=0;i<got;i++) printf("%02x ",out[i]);
            printf("\n");
            if (++fails>3) exit(1);
            TlsHeapLayerPop(l);
            continue;
        }
        CHECK(h->nm==nem);
        for (i=0;i<nem;i++) CHECK(h->m[i].pos==em[i].pos && h->m[i].outLine==em[i].outLine && h->m[i].orig==em[i].orig);
        CHECK(h->nw==ew);
        for (i=0;i<ew;i++) CHECK(h->warn[i]==ewarn[i]);
        /* and again after a Reset */
        x_(f,Reset);
        got = Slurp(f,out,&z);
        CHECK(got==n && !z && !memcmp(out,exp,(size_t)n) && h->nm==nem);
        TlsHeapLayerPop(l);
    }
    CHECK(!fails);
    printf("cpp0 fuzz (against the model, random pieces, Trim or not, Reset): ok\n");
}

static void TestFile(void) {
    static unsigned char txt[40000],out[80000];
    static const char *line[] = {"int  x ;\t// a comment\n","#define F(a,b) \\\n   ((a)+(b)) /* sum */\n","  /* multi\n line */ y = \"s  t\" ;\r\n","\n"};
    int n = 0,z,i;
    FILE *f;
    TlsBuffer0 *c;
    THook *h = NewHook();
    for (i=0;n<30000;i++) { const char *s = line[Rnd()%4]; size_t k = strlen(s); memcpy(txt+n,s,k); n += (int)k; }
    f = fopen("cpp0_test.tmp","wb");
    CHECK(f && fwrite(txt,1,(size_t)n,f)==(size_t)n);
    fclose(f);
    c = TlsCpp0Filter(TlsFileBuffer("cpp0_test.tmp"),Cpp0Default,&h->TlsCpp0Hook);
    {   static unsigned char exp[80000];
        static Mk em[4000];
        unsigned int w[4];
        int nem,nw,ne = Model(txt,n,Cpp0Default&~Cpp0DigitSep,exp,em,&nem,w,&nw);
        int got = Slurp(c,out,&z);
        CHECK(got==ne && !z && !memcmp(out,exp,(size_t)ne));
    }
    remove("cpp0_test.tmp");
    printf("cpp0 file (long, many blocks): ok\n");
}

int main(void) {
    TlsStackNod *l;
    TlsMainHeapOpen(65536);
    l = TlsHeapLayer(); TestSpec(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestHook(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestFuzz(); TlsHeapLayerPop(l);
    l = TlsHeapLayer(); TestFile(); TlsHeapLayerPop(l);
    printf("all passed\n");
    return 0;
}
