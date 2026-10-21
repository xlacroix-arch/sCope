/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_Buffer_h_
#define _Tools_Buffer_h_

#include <Tools_Class.h>

/*_____________________________________________________________________________
 |
 | Buffer0: a READING HEAD over a 0 terminated stream of bytes.
 |
 | The caller reads sequentially thru p and never tests for the end: the data
 | the head points to always ends with a 0 (the sentinel). Only when the
 | caller reads a 0 at address pos does it call
 |
 |        p = x_(b,Check0,pos);
 |
 | pos is the address of that 0. b->p may be stale at that time (the caller
 | walks its own pointer): Check0 updates b->p and returns it too. The result
 | tells where to go on:
 |    - r == TlsBufferEob   the stream is over (a pointer to a static "").
 |                          Check0 on it gives it again: the end is stable.
 |    - r == pos            that 0 is data, not the end (a buffer made from a
 |                          range, a binary file ...). p stays on it; it is
 |                          the caller's to skip, if it accepts such a byte.
 |                          Text parsers that never meet one can ignore this
 |                          case, and treat r==pos as they treat any 0 of data.
 |    - else                the head was refilled or moved to another piece:
 |                          go on at r. Its first byte may be a 0 as well (an
 |                          empty piece, a data 0): the caller just reads it
 |                          and calls Check0 again, as always.
 | Whatever was read before Check0 may be overwritten by it (a file reuses its
 | block): a reader that wants a token keeps it by copying it out BEFORE the
 | next Check0 (typically in an output buffer, then at its final size on the
 | heap). A Buffer0 is only the reading head, it owns no token.
 |
 | Empty buffers are legal, and frequent inside composites: they are skipped by
 | the same rules (their first byte is a 0, Check0 moves on).
 |
 | TlsBufferEof is a second end marker, with no use for now.
 |
 |   - Reset rewinds the stream to its start and returns the new p. (A file is
 |     reopened.)
 |   - Close ends the stream at once (a file is closed); Check0 answers Eob
 |     from then on. It frees nothing: everything is on the heap. It is not
 |     needed: a file is also closed when its stream ends, and, if it has not,
 |     by a hook when the heap layer is popped.
 |
 | All the items created here are pushed on the current heap (rPush/rnPush):
 | no malloc. They live until the layer that holds them is popped; the heap is
 | the one current when the constructor is called.
 |_____________________________________________________________________________
*/

extern unsigned char const TlsBufferEob[], TlsBufferEof[];

typedef struct {
    struct TlsBuffer0 *Static;
    unsigned char *p;
} TlsBuffer0;
struct TlsBuffer0 {
    unsigned char *(*Check0)(TlsBuffer0 *this,unsigned char *pos);
    unsigned char *(*Reset)(TlsBuffer0 *this);
    void (*Close)(TlsBuffer0 *this);
};

/* Two readers that are over from the start (p is TlsBufferEob). They are static:
 * nothing is pushed. */
extern TlsBuffer0 TlsBuffer0Null_;
#define TlsBuffer0Null  (&TlsBuffer0Null_)
extern TlsBuffer0 TlsEmptyBuffer0;

/* Reads str in place (no copy): str must outlive the reader, and being a C
 * string it has no data 0. */
TlsBuffer0 *TlsStringBuffer(unsigned char *str);
/* Reads a copy of [b,e), made on the heap: it may hold data 0 (Check0 gives
 * pos back for them) and need not be terminated. */
TlsBuffer0 *TlsBufferBuffer(char *b,char *e);
/* Reads the file by blocks (one block, pushed once, is refilled when the head
 * reaches its end). The name is copied. Returns NULL if it cannot be opened.
 * The file is closed when the stream ends, or by Close, or when the layer is
 * popped; a data 0 is reported as above. */
TlsBuffer0 *TlsFileBuffer(char *FileName);

/* The concatenation of n buffers (the array is copied; the buffers are used in
 * place, so they must not be read by anybody else meanwhile). Empty ones are
 * skipped. TlsBufferCat0 takes them as arguments, ended by a NULL. */
TlsBuffer0 *TlsBufferCat(TlsBuffer0 **a,int n);
TlsBuffer0 *TlsBufferCat0(TlsBuffer0 *a,...);

/*_____________________________________________________________________________
 |
 | Take and Skip: two filters over a stream that is read from its current
 | position (Src->p), so that they can follow each other on the SAME Src:
 |
 |   TlsBufferTake(Src,n)  yields the next n bytes of Src (fewer if Src ends).
 |                         It copies them thru a small block (TakeBlock bytes,
 |                         pushed once): the end of the range cannot be marked
 |                         inside Src without writing in it.
 |   TlsBufferSkip(Src,n)  yields Src once its next n bytes are dropped. It
 |                         copies nothing.
 |
 | Both are lazy: Src is not touched before their first Check0, so they can be
 | placed in a Cat in any order. A walker that stops before the end of a stream
 | hands over by storing its position in Src->p (Take does it when its n bytes
 | are done): the next filter on the same Src starts there. Reset resets Src
 | itself and starts the filter over; Close closes Src. A data 0 counts as a
 | byte, as any other.
 |
 | TlsBufferReplace(Base,Edit,b,e) reads Base with its bytes [b,e) replaced by
 | Edit: that is Cat(Take(Base,b),Edit,Skip(Base,e-b)). Offsets are in bytes,
 | from 0, e excluded; [i,i[ inserts Edit at i. A position past the end of Base
 | is the end. Base is used in place: nobody else may read it meanwhile.
 |_____________________________________________________________________________
*/
TlsBuffer0 *TlsBufferTake(TlsBuffer0 *Src,int n);
TlsBuffer0 *TlsBufferSkip(TlsBuffer0 *Src,int n);
TlsBuffer0 *TlsBufferReplace(TlsBuffer0 *Base,TlsBuffer0 *Edit,int b,int e);

/*_____________________________________________________________________________
 |
 | Macros: a text with holes, where each hole is a range [b,e) of the text
 | (bytes from 0, e excluded, length e-b; [i,i[ is a plain insert) and the
 | number of a parameter (from 0).
 |
 |   TlsBuffMacroDef *d = TlsBuffMacroDefNew(Text);
 |     Text is read whole, from its start (it is Reset), and copied on the heap
 |     in two passes (one to measure, one to copy: no temporary buffer, no
 |     bound on its size). Text is not used after that.
 |   d->AddSubstitution(d,b,e,Param)
 |     returns 0, or -1 (and changes nothing) if the range is not inside the
 |     text, if Param<0, or if it overlaps one already there. Two ranges only
 |     touching are fine, and inserts at the same place come out in the order
 |     they were added. The order of the calls is otherwise free.
 |   TlsBuffMacro *m = d->Generate(d)
 |     cuts a copy of the text into its pieces, one block. The definition can go
 |     on being edited, and generate again.
 |   m->ParamNumber(m)
 |     the number of parameters: the highest Param used + 1 (0 if none).
 |   m->Apply(m,Var)
 |     returns a new reader of the text with Param i replaced by the stream
 |     Var[i] (a NULL one is empty). Var needs ParamNumber items, and is copied.
 |     Apply can be called as often as wanted: every result is independent,
 |     and only the pieces are shared. A result costs one small structure and
 |     the copy of Var, it copies no text. The streams Var[i] are read in
 |     place, lazily, so they may be anything: files, macro results ...
 |     A parameter used several times is read again each time: its stream is
 |     Reset before every use but the first, and so must not be shared with
 |     another reader meanwhile. Reset on a result starts it over (the Var are
 |     Reset as they are met).
 |_____________________________________________________________________________
*/
TlsClassDef(TlsBuffMacro) {
    int (*ParamNumber)(TlsBuffMacro *this);
    TlsBuffer0 *(*Apply)(TlsBuffMacro *this,TlsBuffer0 **Var);
};
TlsClassDef(TlsBuffMacroDef) {
    int (*AddSubstitution)(TlsBuffMacroDef *this,int b,int e,int ParamNum);
    TlsBuffMacro *(*Generate)(TlsBuffMacroDef *this);
};
TlsBuffMacroDef *TlsBuffMacroDefNew(TlsBuffer0 *BaseText);

#endif
