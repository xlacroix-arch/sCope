/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Tok_h_
#define _Tools_Tok_h_

#include <Tools_Core.h>
#include <Tools_Buffer.h>
#include <Tools_Dict.h>
#include <Tools_OutBuf.h>

/*_____________________________________________________________________________
 |
 | Token codes (what a tokenizer's Next() returns):
 |
 |   0            TokNil: nothing, the end of the text
 |   1..31        the operators made of several characters (below), and TokNl
 |   32..127      a single character punctuator is its own ASCII value ('(', '+'...)
 |   a letter     a class of token, its value comes with it:
 |                  TokLabel 'l'   a name (value: its Dict number)
 |                  TokInt   'i'   an integer     TokFloat 'f'  a number with . or e
 |                  TokStr   's'   a string        TokChar  'c'  a character
 |                  TokStray 'x'   a byte that is nothing else
 |
 | The operators are offsets in struct c_TlsParser, as the framework does it:
 |     c_(TlsParser,C.op,Arrow)
 | works as a case label, and sizeof(((struct c_TlsParser *)0)->C.op) counts them.
 | TokNl is 10, '\n' itself: the end of a line is a token for the directives.
 |_____________________________________________________________________________
*/

struct c_TlsParser {
    struct {
        struct {
            char nil;                                   /*  0 */
            char Arrow,Inc,Dec,Shl,Shr,Le,Ge,Eq,Ne;     /*  1..9   -> ++ -- << >> <= >= == != */
            char Nl;                                    /* 10 */
            char AndAnd,OrOr,Scope,Ellipsis,Paste;      /* 11..15  && || :: ... ## */
            char AddEq,SubEq,MulEq,DivEq,ModEq;         /* 16..20 */
            char AndEq,OrEq,XorEq,ShlEq,ShrEq;          /* 21..25 */
        } op;
    } C;
};

#define TokNil 0
#define TokNl '\n'
#define TokLabel 'l'
#define TokInt 'i'
#define TokFloat 'f'
#define TokStr 's'
#define TokChar 'c'
#define TokStray 'x'
#define TokOp(name) c_(TlsParser,C.op,name)

/*_____________________________________________________________________________
 |
 | Tok: the tokenizer. It reads a Buffer0 (meant to be a Cpp0 filter: comments
 | gone, blanks collapsed, lines ended by '\n', no 0 in the data) and gives the
 | tokens one at a time, nothing is read ahead but the byte that follows.
 |
 |   Next(t)     moves to the next token and returns its code (also in Code). At
 |               the end it returns TokNil, again and again. The fields tell:
 |       Code    TokNil, an operator, a single character, or a class letter
 |       Txt,Len the characters of the token, as written (Txt is 0 terminated
 |               for the operators, the classes are cut at Len). A string keeps
 |               its quotes. It is valid until the next call (it can be in the
 |               source buffer itself): copy it if you keep it
 |       Val     'l': the Dict number    'i': the value (the ' and the u l suffixes
 |               are ignored)    'c': the character (an escape is decoded, a
 |               'ab' makes a*256+b)    'x': the byte
 |       Spc     1 if blanks came before the token (that is what tells
 |               "#define F(x)" from "#define F (x)")
 |       Pos,End  where the token starts and ends, in bytes from the start of the
 |               source (End excluded): what AddSubstitution wants. The blanks
 |               and the end of lines count as bytes
 |       Line    the line of the token, 1 for the first one; the \n belongs to the
 |               line it ends (the lines of the filtered text: TlsLineMapOrig maps
 |               them to the original ones)
 |   Rest(t,&n)  the raw rest of the line, from the next non blank byte to the
 |               end of line (that is left for Next): #error, #include <x.h>,
 |               #pragma. n may be NULL: it skips the line and copies nothing.
 |               Right after the end of line token there is no rest: it gives an
 |               empty one and reads nothing (so that a reader that met the end of
 |               its line too soon can still end it with Rest).
 |   Peek(t,skip) the next byte, not read: right after the last token, or (skip)
 |               after the blanks that follow. 0 at the end. '\n' is a byte:
 |               a macro call is not looked for on the next line.
 |   Reset(t)    Resets the source and starts again.
 |
 | An unclosed string or character stops at the end of the line. A token that
 | spans two pieces of the source has to be put together: it is built in the OutBuf
 | and Txt points there if it fits in its first chunk (it is lost at the next
 | token that spans pieces, in any tokenizer that shares this OutBuf: copy it
 | before), else it is copied (Take, on the current heap). The other tokens are not
 | copied. Give an OutBuf to share it between tokenizers, or NULL for a new one.
 |_____________________________________________________________________________
*/
typedef struct {
    struct TlsTok *Static;
    int Code,Spc,Len;
    unsigned char *Txt;
    unsigned long Val,Pos,End;
    unsigned int Line;
} TlsTok;
struct TlsTok {
    int (*Next)(TlsTok *this);
    unsigned char *(*Rest)(TlsTok *this,int *n);
    int (*Peek)(TlsTok *this,int skip);
    void (*Reset)(TlsTok *this);
};

TlsTok *TlsTokNew(TlsBuffer0 *Src,TlsDict *D,TlsOutBuf *Ob);

#endif
