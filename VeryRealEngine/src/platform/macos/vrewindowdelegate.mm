#import <QuartzCore/CAMetalLayer.h>

#include "vrewindowdelegate.hpp"

@implementation VREWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
	(void)sender;
	self.owner->on_close_requested();
	// Returning NO keeps AppKit from tearing the window down immediately;
	// the demo's main loop notices should_close() and exits on its own
	// terms, mirroring WindowLinux's WM_DELETE_WINDOW handling (which also
	// just sets a flag rather than closing the X11 window itself).
	return (NO);
}

- (void)windowDidResize:(NSNotification *)notification
{
	NSWindow *window = static_cast<NSWindow *>(notification.object);
	NSView *view = window.contentView;
	CGFloat scale = window.backingScaleFactor;
	int32_t width = static_cast<int32_t>(view.frame.size.width * scale);
	int32_t height = static_cast<int32_t>(view.frame.size.height * scale);

	CAMetalLayer *layer = static_cast<CAMetalLayer *>(view.layer);
	layer.drawableSize = CGSizeMake(width, height);

	self.owner->on_resized(width, height);
}
@end
