/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Pp_h_
#define _Tools_Pp_h_

#include <Tools_PpCtx.h>
#include <Tools_LineMap.h>

/*_____________________________________________________________________________
 |
 | Pp: the tokens of a text with its macros expanded and its # lines done. It is
 | the input stack (the text, and one more level for each macro being expanded),
 | the expansion, and the directives:
 |
 |   #define #undef             the macros of the context
 |   #ifdef #ifndef #else #endif  groups of text that are read or skipped (they nest;
 |                              a skipped line is not tokenized, but for its # )
 |   #error #warning           reported (PpMsgError / PpMsgWarning)
 |   #pragma, and what is not known    to the hook of the context (Directive); an
 |                              unknown one that the hook does not take is an error
 |   #include "x" <x>          the file is looked for with Resolve of the context (the
 |                              directory of the including file first for "x", then the
 |                              paths); the name is written as it is, no macro for it. The
 |                              file is read at once through Cpp0, with a line map of its
 |                              own, and the text goes on after the line. 16 files deep at most.
 |                              A file that cannot be found or read is an error
 |   #pragma once              the file is not read again by an #include (the name it
 |                              was found under)
 |   #if #elif                 the rest of the line: `defined X` and `defined(X)` become 1 or 0,
 |                              its macros are expanded, and Tools_Expr gives the value. A
 |                              name that is left has the value 0. An #elif is not evaluated
 |                              if a group was taken, nor a #if in a group that is off. A line
 |                              that is not an expression is reported, and the group is off
 |   #line N  #line N "file"  the next line is number N (N is a number, or macros that give
 |                              it), in the file the name says (the last one if there is none).
 |                              It is for __LINE__, __FILE__, the messages, Where and File of
 |                              what follows, in this file only (an #include'd file has
 |                              its own and gives it back)
 |   __LINE__ __FILE__         a number and a string. __LINE__ is the line of the macro call
 |                              that is expanded (the first line of it), also in an argument
 |                              that is expanded first. #ifdef and defined take them as macros;
 |                              a macro the program defines with their name comes first
 |
 | A # is a directive if it is the first token of its line, in the text itself
 | (not in an expansion). The end of line of a directive, and of a line that is skipped,
 | is still there, for PpNl: the lines of the output stay where those of the text are.
 | A hook that reads a directive line stops before its end of line.
 |
 |   TlsPpNew(Ctx,Src,Name,Map,Flags)
 |       Ctx: the context (its Dict, its Macros, its Hook: what goes wrong is
 |       reported there). Src: a Buffer0, usually a Cpp0 filter (what the tokenizer
 |       says applies). Name: the path of the file, for the messages (may be NULL).
 |       Map: the LineMap that was the hook of that Cpp0 filter, to give the lines of
 |       the original file in the messages (may be NULL: the lines are those of Src).
 |       Flags: PpNl to get the end of line tokens too (without, they are dropped).
 |       The first file is told to the hook (Open) if it has a Name.
 |   TlsPpNewFile(Ctx,Path,Flags)  the same for a file: it opens it (NULL if it cannot),
 |       with its Cpp0 filter and line map, on the current heap, so the layer that holds
 |       the call must outlive the use. It is closed when it is read to its end.
 |   Next(p)  the next token, and its code (TokNil at the end). The fields are
 |       those of TlsTok, with
 |         Code,Spc,Len,Txt,Val   as the tokenizer's
 |         Pos,End,Line            the place in the text: for a token that comes
 |                                 from an expansion, the one of the whole call
 |         Exp                     0, or how many macros deep the token is
 |         File                    the file the token is in (a token from an expansion:
 |                                 the file of the call); valid until the next call
 |       Txt is valid until the next call.
 |   Where(p,&File,&Line)  the file and the line IN THE ORIGINAL FILE of the last token
 |       (Line, in the fields, is the line of the filtered text, cheap to give).
 |
 | A function like macro is a call only if its name is followed by ( on the same
 | line. The arguments are cut, on the text as written, at the commas that are not
 | in parentheses (so F(G(2)) is one argument even if G gives a comma), each one kept
 | as text with the blanks between its tokens made one. As in C, a parameter is
 | replaced by its argument with its macros expanded first (by a Pp of its own, on
 | the same context), but for the operands of # and ##, which stay as written; the
 | result is read again. A macro does not expand
 | inside its own expansion (nor does one it called). What goes wrong (an argument
 | list that is not closed, too many or too few arguments) is reported to the context. All the room an expansion
 | takes is given back when the outermost one is read.
 |_____________________________________________________________________________
*/

#define PpNl 1

typedef struct {
    struct TlsPp *Static;
    int Code,Spc,Len,Exp;
    unsigned char *Txt;
    unsigned long Val,Pos,End;
    unsigned int Line;
    char const *File;
} TlsPp;
struct TlsPp {
    int (*Next)(TlsPp *this);
    void (*Where)(TlsPp *this,char const **File,unsigned int *Line);
};

TlsPp *TlsPpNew(TlsPpCtx *Ctx,TlsBuffer0 *Src,char const *Name,TlsCpp0Hook *Map,int Flags);
TlsPp *TlsPpNewFile(TlsPpCtx *Ctx,char const *Path,int Flags);

#endif
