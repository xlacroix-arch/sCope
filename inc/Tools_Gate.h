/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _Tools_Gate_h_
#define _Tools_Gate_h_
#include <Tools_Class.h>

/*_____________________________________________________________________________
 |
 | Threads and Gates:
 | A thread is spawned by TlsGateSet and can be accessed by the returned TlsGate.
 | The thread heap is forked from the current heap, and its stack is the first
 | element pushed on it. However, it is the responsibility of the parent to 
 | shut it down before its layer is poped.
 |
 | Communication: 
 |   * The client side issues a request by calling the Knock method. It is then
 | suspended until the end of the operation.
 |   * The server side can use  periodically the Check method to see if a 
 | request is pending. It can then use the Invite method to merge the threads.
 |   * Once merged, the server side can use the resulting GateGuest to move synchronously 
 | between the two thread environments, using TlsGateGuestSwitch().
 |   * Once the request finished, the server side can use the TlsGateGuestSendOf()
 | primitive to reactivate the client side.
 |_____________________________________________________________________________
*/

typedef struct TlsGateNod TlsGateNod;
typedef struct {TlsGateNod *Nod;} TlsGateGuest;
TlsGateGuest *TlsGateGuestSwitch(TlsGateGuest *Target,TlsGateGuest *ret);
void TlsGateGuestSendOf(TlsGateGuest *Target,TlsGateGuest *ret);

TlsClassDef(TlsGate) {
    void (*Shutdown)(TlsGate *this);
    TlsGateGuest *(*Knock)(TlsGate *this,TlsGateGuest *request);
    int (*Check)(TlsGate *this);
    TlsGateGuest *(*Invite)(TlsGate *this);
};
TlsClassDef(TlsGateEntry) {
    void (*Entry)(TlsGateEntry *this,TlsGate *Parent);
};
TlsGate *TlsGateSet(int StackSize,TlsGateEntry *Entry);

#endif

