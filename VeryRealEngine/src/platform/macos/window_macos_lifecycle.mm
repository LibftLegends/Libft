/**
 * @file window_macos_lifecycle.mm
 * @brief WindowMacOS::initialize()/destroy()'s bodies, split out of
 * window_macos.mm purely to keep each file under this project's 250-line
 * cap — both files define methods of the same WindowMacOS class declared
 * in window_macos.hpp.
 */
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include "window_macos.hpp"

// Declared in window_macos.mm; both files instantiate these Objective-C
// classes, so their @interface declarations live in window_macos.hpp's
// translation-unit-local scope via this forward declaration instead.
@interface VREMetalView : NSView
@end

@interface VREWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) vre::WindowMacOS *owner;
@end

namespace vre
{

bool WindowMacOS::initialize(const char *title, int32_t width, int32_t height)
{
    NSRect frame;
    VREMetalView *view;
    CAMetalLayer *layer;
    CGFloat scale;
    VREWindowDelegate *delegate;

    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        frame = NSMakeRect(0, 0, width, height);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

        NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                        styleMask:style
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO];
        if (window == nil)
        {
            std::fprintf(stderr, "WindowMacOS: NSWindow creation failed\n");
            return false;
        }

        [window setTitle:[NSString stringWithUTF8String:title]];
        [window center];

        view = [[VREMetalView alloc] initWithFrame:frame];
        view.wantsLayer = YES;
        window.contentView = view;
        [window makeFirstResponder:view];

        layer = static_cast<CAMetalLayer *>(view.layer);
        scale = window.backingScaleFactor;
        layer.contentsScale = scale;
        _width = static_cast<int32_t>(frame.size.width * scale);
        _height = static_cast<int32_t>(frame.size.height * scale);
        layer.drawableSize = CGSizeMake(_width, _height);

        delegate = [[VREWindowDelegate alloc] init];
        delegate.owner = this;
        window.delegate = delegate;

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp finishLaunching];

        _ns_window = (void *)CFBridgingRetain(window);
        _delegate = (void *)CFBridgingRetain(delegate);
        _metal_layer = (__bridge void *)layer;

        return true;
    }
}

void WindowMacOS::destroy()
{
    NSWindow *window;

    @autoreleasepool
    {
        if (_ns_window != nullptr)
        {
            window = (__bridge NSWindow *)_ns_window;
            window.delegate = nil;
            [window close];
        }
        if (_delegate != nullptr)
        {
            CFBridgingRelease(_delegate);
            _delegate = nullptr;
        }
        if (_ns_window != nullptr)
        {
            CFBridgingRelease(_ns_window);
            _ns_window = nullptr;
        }
        _metal_layer = nullptr;
    }
}

} // namespace vre
