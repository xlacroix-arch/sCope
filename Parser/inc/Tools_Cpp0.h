/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Cpp0_h_
#define _Tools_Cpp0_h_

#include <Tools_Buffer.h>

/*_____________________________________________________________________________
 |
 | Cpp0: the first cleaning of a C source (.c, .h), as a Buffer0 filter. It reads
 | Src (any Buffer0, used in place) lazily and yields, by blocks:
 |
 |  1. line ends: CR LF, CR and LF all become one \n. (LF CR is two.)
 |  2. continued lines: a backslash immediately followed by a line end is removed
 |     with it, wherever it is (inside a comment or a literal too, as in C).
 |  3. comments: the block ones (slash star ... star slash) and the line ones (two
 |     slashes, up to but not including the line end) each become ONE blank, so
 |     that a, an empty block comment, b stays two words.
 |  4. blanks: space, tab, vertical tab, form feed and the NUL byte all count as
 |     blank, and a run of blanks (comments included) becomes a single space.
 |     (A NUL inside a literal becomes a space: the output holds no 0.)
 |     With Cpp0Trim the blanks at the start and the end of a line are dropped.
 |  5. literals: "..." and '...' are copied as they are (their blanks, tabs and
 |     comment look-alikes are kept, \x escapes skip one byte). One that is not
 |     closed ends at the line end, as in  #error don't  : no error, but what
 |     follows it on that line (a comment too) stays as it is.
 |     With Cpp0DigitSep the ' inside a number (1'000, 0xff'ff) is a separator,
 |     not the start of a character literal.
 |  6. the output always ends with a \n (unless it is empty), a BOM at the start
 |     is dropped, other bytes (UTF-8 ...) are copied untouched.
 |
 | The output holds no 0. Not done: trigraphs, a space between the backslash and
 | the line end (it is not a continuation), header names (#include <a//b.h>),
 | and anything about # lines: they are left to the next stage. The other
 | readers' rules apply: Src is read from its start (assumed fresh), nobody
 | else may read it meanwhile, Reset resets Src and starts over, Close closes it.
 |
 | Hook (may be NULL, and any of its methods too). Line numbers start at 1 and
 | count the lines of the ORIGINAL file; the output lines are numbered the same
 | way, from 1, by its \n. The output line that starts on an original line other
 | than the one after the previous output line's start is marked, so that the
 | marks are few: a file without continued lines or multi line comments has none.
 |
 |   Mark(this,OutPos,OutLine,OrigLine)
 |       the output line OutLine starts at the byte OutPos of the output (counted
 |       from 0 since the start or the last Reset), and its first byte comes from
 |       the original line OrigLine. Until the next mark, output line k comes from
 |       original line k+OrigLine-OutLine. (A line that is only a \n has the
 |       line of that line end.) The marks are called as the block holding the
 |       byte is produced: up to one block before the reader gets there, so keep
 |       the OutPos, not the time.
 |   Warn(this,OrigLine,Msg)
 |       something was odd: for now only "unterminated comment", reported at the
 |       line where the comment began.
 |   Reset(this)
 |       the filter was Reset: forget the marks, they come again.
 |
 | TlsCpp0OutPos tells the byte number in the output of a pointer p into the
 | current block of the filter (what the reader has in Check0 / p), so that a
 | parser can find the original line of a position without counting. It returns
 | (unsigned long)-1 if Cpp0 is not a Cpp0 filter.
 |_____________________________________________________________________________
*/

#define Cpp0Trim      1
#define Cpp0DigitSep  2
#define Cpp0Default   (Cpp0Trim|Cpp0DigitSep)

TlsClassDef(TlsCpp0Hook) {
    void (*Mark)(TlsCpp0Hook *this,unsigned long OutPos,unsigned int OutLine,unsigned int OrigLine);
    void (*Warn)(TlsCpp0Hook *this,unsigned int OrigLine,char *Msg);
    void (*Reset)(TlsCpp0Hook *this);
};

TlsBuffer0 *TlsCpp0Filter(TlsBuffer0 *Src,int Flags,TlsCpp0Hook *Hook);
unsigned long TlsCpp0OutPos(TlsBuffer0 *Cpp0,unsigned char *p);

#endif
