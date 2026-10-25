/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Macros_h_
#define _Tools_Macros_h_

#include <Tools_Tok.h>

/*_____________________________________________________________________________
 |
 | MacroLib: the macros of one context, found by the Dict number of their name.
 | The body of a macro is a TlsBuffMacro (Tools_Buffer.h): its text, with the
 | places of the parameters marked, so that an expansion copies no text.
 |
 |   Define(l,t)     defines a macro from the text the tokenizer t is on, just after
 |                   the word define:   NAME body   or   NAME(a,b,...) body
 |                   (the ( must touch the name). The body is what is left of the
 |                   line (the end of line itself is not read). Returns the number of the name, or 0 if it is not a
 |                   definition (the line is skipped). A macro defined again
 |                   replaces the old one.
 |   DefineStr(l,s)  the same from a string:  "NAME(a) a+1"  (-DNAME=1 and such)
 |   Get(l,id)       the macro of a name, or NULL
 |   Undef(l,id)     forgets it; 1 if it was there
 |
 | The arguments are cut on the text as written, then, as in C, a parameter is
 | replaced by its argument with the macros in it expanded (the macro itself is not
 | busy yet), except an operand of # or ##, which is the text as written. In the body
 |   #x     is the text of the argument x in a string ("" and \ escaped)
 |   a##b   glues the two: nothing is put between them, they are read again
 |          together (so x##y with foo and bar gives the label foobar)
 |   ...    the last parameter takes the rest of the arguments, commas
 |          included; its name in the body is __VA_ARGS__
 | At most 31 parameters, besides the ... one.
 |_____________________________________________________________________________
*/

typedef struct {
    int Params;                 /* -1: no parentheses (an object like macro), else how many */
    int Variadic;               /* the last one is ... */
    int Busy;                   /* being expanded: it does not expand again inside itself */
    unsigned int Str;           /* bit k: parameter k is used with # */
    unsigned int Raw;           /* bit k: parameter k is an operand of ## (it is not expanded first) */
    int Np;                     /* the parameters of the body */
    TlsBuffMacro *Body;
} TlsMacro;

TlsClassDef(TlsMacroLib) {
    int (*Define)(TlsMacroLib *this,TlsTok *t);
    int (*DefineStr)(TlsMacroLib *this,char const *s);
    TlsMacro *(*Get)(TlsMacroLib *this,int id);
    int (*Undef)(TlsMacroLib *this,int id);
};

TlsMacroLib *TlsMacroLibNew(TlsDict *D,TlsHeap *Where);

#endif
