/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_PpCtx_h_
#define _Tools_PpCtx_h_

#include <Tools_Macros.h>

/*_____________________________________________________________________________
 |
 | PpCtx: the context of a preprocessing: what the files that are read together
 | share. It holds
 |
 |   Dict      the labels (to share with other contexts, or not)
 |   Macros    the macros of this context only (a new context starts without
 |             any, whatever the Dict: -D things are defined with
 |             x_(ctx->Macros,DefineStr,"NAME 1"))
 |   the include paths, in the order they are tried
 |   the words that mean something to the directives (define, if ...), found
 |   back from the Dict number of a label with Word(c,id)
 |
 |   TlsPpCtxNew(D,Where)   D: the Dict to use, NULL for a new one. Where: the heap
 |                          of everything the context keeps, NULL forks the current.
 |   AddPath(c,dir)         a directory to search, after those already there (the
 |                          string is copied)
 |   Path(c,i)              the i-th one (from 0), or NULL
 |   Resolve(c,name,quoted,from)
 |                          the file a #include names, as a path (0 terminated, on the
 |                          current heap), or NULL if there is none. A name that begins
 |                          with / is taken as it is. Else, if quoted ("name"), the
 |                          directory of the file from (NULL: none) is tried first,
 |                          then every path in order. A file exists if it opens.
 |   Word(c,id)            the PpWord of a label number, or 0 (PpWord.nil) if it is
 |                          not one of them
 |   OnceSet(c,path)       a file that is read only once (#pragma once): written down
 |                          (the string is copied)
 |   OnceHas(c,path)       1 if it was: the names are compared as they are, so it is
 |                          the name a file was found under that counts
 |
 | Hook (may be NULL, and any of its methods too): how the preprocessor talks to
 | whatever it feeds, and what that language may add to it. It is set in the
 | field Hook of the context, before the first file is read.
 |
 |   Message(h,Kind,File,Line,Text)
 |       something to tell: PpMsgError (#error, a macro that is not closed ...),
 |       PpMsgWarning (#warning, an unterminated comment ...) or PpMsgInfo. File is
 |       the file's path as it was resolved (NULL: the text had none), Line the
 |       line of the ORIGINAL file, Text is only valid during the call. Nothing
 |       stops: the preprocessor goes on.
 |   Directive(h,Word,Id,t,File,Line)
 |       a # line the preprocessor does not know: Word is PpWord(Pragma) for a
 |       #pragma, else 0; Id is the Dict number of its name (the label after the #;
 |       for a pragma, of the first word after pragma, or 0). t is the tokenizer,
 |       on the line: read what you need from it (Next, Rest), but stop before its
 |       end (look with Peek(t,1): '\n' is the end: the Pp needs to read it). Return 1
 |       if it was yours; else the line is skipped (an unknown one is also an error,
 |       a pragma is just ignored). Whatever is left of the line is skipped.
 |   Open(h,File,Line,Path)
 |       an #include found its file and opens it (the first file too, with Line 0):
 |       what a dependency list wants.
 |
 | The context calls them through Report, Directive and Opened (that check for the
 | NULLs) and counts Errors and Warnings, hook or not.
 |_____________________________________________________________________________
*/

#define PpMsgError 1
#define PpMsgWarning 2
#define PpMsgInfo 3

TlsClassDef(TlsPpHook) {
    void (*Message)(TlsPpHook *this,int Kind,char const *File,unsigned int Line,char const *Text);
    int (*Directive)(TlsPpHook *this,int Word,int Id,TlsTok *t,char const *File,unsigned int Line);
    void (*Open)(TlsPpHook *this,char const *File,unsigned int Line,char const *Path);
};

/* the words of the directives, as c_(TlsPpCtx,C.w,Define) ... (case labels) */
struct c_TlsPpCtx {
    struct {
        struct {
            char nil;
            char Define,Undef,Include,If,Ifdef,Ifndef,Elif,Else,Endif,Error,Warning,Pragma,Line;
            char Defined,LineMac,FileMac;               /* defined, __LINE__, __FILE__ */
            char Once;                                  /* the word after #pragma */
        } w;
    } C;
};
#define PpWord(name) c_(TlsPpCtx,C.w,name)

typedef struct {
    struct TlsPpCtx *Static;
    TlsDict *Dict;
    TlsMacroLib *Macros;
    TlsPpHook *Hook;
    int Errors,Warnings;
} TlsPpCtx;
struct TlsPpCtx {
    void (*AddPath)(TlsPpCtx *this,char const *dir);
    char const *(*Path)(TlsPpCtx *this,int i);
    char *(*Resolve)(TlsPpCtx *this,char const *name,int quoted,char const *from);
    int (*Word)(TlsPpCtx *this,int id);
    void (*OnceSet)(TlsPpCtx *this,char const *path);
    int (*OnceHas)(TlsPpCtx *this,char const *path);
    void (*Report)(TlsPpCtx *this,int Kind,char const *File,unsigned int Line,char const *Text);
    int (*Directive)(TlsPpCtx *this,int Word,int Id,TlsTok *t,char const *File,unsigned int Line);
    void (*Opened)(TlsPpCtx *this,char const *File,unsigned int Line,char const *Path);
};

TlsPpCtx *TlsPpCtxNew(TlsDict *D,TlsHeap *Where);

#endif
