//=========================================================================
// Name:            MacStaticBox.h
// Purpose:         Reach through wxStaticBox to the underlying NSBox so we
//                  can opt out of macOS 26 (Tahoe) grouped-form recessed
//                  rendering while keeping the box title visible.
//
// Authors:         J.J. Boyd
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#ifndef MAC_STATIC_BOX_H
#define MAC_STATIC_BOX_H

class wxStaticBox;

namespace tci {

// On macOS 26 the default wxStaticBox renders as a Tahoe grouped-form
// recessed darker card.  This call isa-swizzles the underlying NSBox to a
// subclass whose drawRect: skips the recessed background and border, then
// draws only the title cell.  The result: parent chrome shows through,
// title text stays visible, group label still reads as a labeled group.
//
// No-op on non-Apple platforms.
void MakeStaticBoxFlat(wxStaticBox* box);

} // namespace tci

#endif // MAC_STATIC_BOX_H
