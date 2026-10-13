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

TlsClassDef(TlsGateGuest) {
    int (*Switch)(TlsGateGuest *this);
    void (*SendOf)(TlsGateGuest *this);
}
TlsClassDef(TlsGate) {
    void (*Shutdown)(TlsGate *this);
    TlsGateQuest *(*Knock)(TlsGate *this,unsigned int date);
    int (*Check)(TlsGate *this);
    TlsGateGuest *(*Invite)(TlsGate *this);
};
extern TlsGate TlsGateClosed;
TlsClassDef(TlsGateEntry *Entry) {
    void (*Entry)(TlsGateEntry *this,TlsGate *Parent);
};

#endif

