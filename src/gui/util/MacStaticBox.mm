//=========================================================================
// Name:            MacStaticBox.mm
// Purpose:         (see header)
//
// Authors:         J.J. Boyd
// License:         (see header: LGPL 2.1+ matching freedv-gui house style)
//=========================================================================

#include "MacStaticBox.h"

#if defined(__APPLE__)

#include <wx/wx.h>
#include <wx/statbox.h>

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

// ---------------------------------------------------------------------------
// FreeDVStaticBox: NSBox subclass whose drawRect: skips the default Tahoe
// grouped-form background and border drawing.  We draw only the title cell.
//
// Why subclass instead of switching boxType?
//   - NSBoxPrimary (the default on macOS 26) draws title + recessed grouped-
//     form background.  No public toggle separates the two.
//   - NSBoxCustom does NOT draw the title automatically; the title disappears.
//   - NSBoxOldStyle was deprecated in 10.15 and is rejected by -Werror.
//   - Setting transparent=YES hides the title as well as the background.
//
// Overriding drawRect: directly lets us paint only the title using the
// existing titleCell, preserving accessibility metadata, and skipping the
// Tahoe grouped-form rendering entirely.
// ---------------------------------------------------------------------------
@interface FreeDVStaticBox : NSBox
@end

@implementation FreeDVStaticBox
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;

    NSRect bounds    = [self bounds];
    NSRect titleRect = [self titleRect];
    NSCell* cell     = [self titleCell];

    // Draw a thin border around the bounds with a gap at the top edge
    // where the title sits.  Matches the pre-Tahoe NSBoxOldStyle look:
    // labeled-group rectangle with the title text breaking through the
    // top border on the left.
    //
    // Border is inset half a pixel so the 1px stroke lands on whole pixels.
    const CGFloat kInset       = 0.5;
    const CGFloat kTitleGapPad = 4.0;   // horizontal padding on each side of the title gap
    CGFloat top    = NSMaxY(titleRect) - 3.0;      // border top runs through the title's midline
    CGFloat left   = NSMinX(bounds)    + kInset;
    CGFloat right  = NSMaxX(bounds)    - kInset;
    CGFloat bottom = NSMinY(bounds)    + kInset;

    CGFloat titleLeft  = NSMinX(titleRect) - kTitleGapPad;
    CGFloat titleRight = NSMaxX(titleRect) + kTitleGapPad;

    // Clamp the title gap inside the border bounds.
    if (titleLeft  < left)  titleLeft  = left;
    if (titleRight > right) titleRight = right;

    NSBezierPath* path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(titleLeft,  top)];
    [path lineToPoint:NSMakePoint(left,       top)];
    [path lineToPoint:NSMakePoint(left,       bottom)];
    [path lineToPoint:NSMakePoint(right,      bottom)];
    [path lineToPoint:NSMakePoint(right,      top)];
    [path lineToPoint:NSMakePoint(titleRight, top)];
    [path setLineWidth:1.0];
    [[NSColor separatorColor] setStroke];
    [path stroke];

    // Now draw the title text on top so it visually breaks through the
    // top border gap.
    if (cell)
    {
        [cell drawWithFrame:titleRect inView:self];
    }
}
@end

namespace tci {

void MakeStaticBoxFlat(wxStaticBox* box)
{
    if (!box) return;

    // wxStaticBox::GetHandle() returns the underlying NSView*.  On Cocoa
    // wxStaticBox wraps an NSBox.  Defensive guard in case wxWidgets
    // changes the native widget type in a future release.
    NSView* view = static_cast<NSView*>(box->GetHandle());
    if (!view || ![view isKindOfClass:[NSBox class]]) return;

    // Swap the runtime class of the existing NSBox instance to our
    // subclass.  drawRect: now routes to FreeDVStaticBox's override.
    object_setClass(view, [FreeDVStaticBox class]);
    [view setNeedsDisplay:YES];
}

} // namespace tci

#endif  // __APPLE__
// Non-Apple builds use the inline no-op declared in MacStaticBox.h; this
// translation unit is only compiled when APPLE (see gui/util/CMakeLists.txt).
