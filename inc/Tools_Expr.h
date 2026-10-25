/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Expr_h_
#define _Tools_Expr_h_

#include <Tools_Tok.h>

/*_____________________________________________________________________________
 |
 | Expr: the value of an integer expression written with the C operators, read
 | from a tokenizer. It is made for the #if of the pre-processor, but it knows
 | nothing about it: any text the tokenizer reads will do, and the names it
 | meets are given a value by the caller.
 |
 |   TlsExprEval(t,env,&v)   reads the tokens of t up to the end of the text (or of
 |       the line: TokNil, TokNl) and gives the value in v. It returns NULL, or a
 |       message if the text is not an expression (the tokenizer is left where it
 |       stopped). env gives the value of the names (may be NULL: a name is an error).
 |
 |   Label(env,id,&v)   the value of the name whose Dict number is id; returns 0 if
 |       it has none (that is an error).  It is not called for a name that is not
 |       evaluated (the side of && || ?: that is not taken).
 |
 | The operators, from the loosest to the tightest:
 |       ?:   ||   &&   |   ^   &   == !=   < > <= >=   << >>   + -   * / %
 |       and, in front: + - ! ~     ( ) to group.
 | The numbers are 64 bits, signed (the u and l suffixes mean nothing, a
 | character is its value). Overflows wrap. A shift by 64 or more, or by a
 | negative number, gives 0 (or -1 for a negative number >> 64); a division by
 | 0 is an error unless it is not evaluated. No comma operator, no float.
 | Everything is on the stack: nothing is pushed on the heap.
 |_____________________________________________________________________________
*/

typedef struct {
    struct TlsExprEnv *Static;
} TlsExprEnv;
struct TlsExprEnv {
    int (*Label)(TlsExprEnv *this,int id,long long *v);
};

char const *TlsExprEval(TlsTok *t,TlsExprEnv *env,long long *v);

#endif
