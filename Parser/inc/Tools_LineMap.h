/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#ifndef _Tools_LineMap_h_
#define _Tools_LineMap_h_

#include <Tools_Core.h>
#include <Tools_Cpp0.h>

/*_____________________________________________________________________________
 |
 | LineMap: the Hook to give to a Cpp0 filter, that keeps its marks so that the
 | line of a token in the filtered text (TlsTok Line) is turned back into the
 | line of the original file, for the messages. A file with no continued line and
 | no multi line comment has no mark at all: the lines are the same.
 |
 |   TlsLineMapNew(Where)       the hook (Where: the heap that holds the marks, NULL
 |                              forks the current one). The marks are kept in a
 |                              list of chunks of the same size.
 |   TlsLineMapOrig(m,OutLine)  the original line of the line OutLine (from 1) of the
 |                              filtered text. The marks come as the filter reads
 |                              its source, a block before the reader: a line that
 |                              was read is always known.
 |   TlsLineMapWarns(m,&Line,&Msg)  how many warnings the filter gave; the first
 |                              one is in Line (original line) and Msg.
 |   Reset of the filter forgets all (it calls the hook's Reset).
 |_____________________________________________________________________________
*/

TlsCpp0Hook *TlsLineMapNew(TlsHeap *Where);
unsigned int TlsLineMapOrig(TlsCpp0Hook *m,unsigned int OutLine);
int TlsLineMapWarns(TlsCpp0Hook *m,unsigned int *Line,char **Msg);

#endif
