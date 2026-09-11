// macOS windowing backend implementation (Objective-C++, ARC-enabled — see
// the Makefile's dedicated `.mm` compile rule). Native Cocoa/Metal objects
// (NSWindow, the delegate, CAMetalLayer) are bridged to the plain `void *`
// members declared in window_macos.hpp so that header stays includable from
// non-Objective-C translation units (renderer.cpp) — see the comment there.
//
// The window and its delegate are given an owning reference via
// CFBridgingRetain/CFBridgingRelease: AppKit's NSWindow.delegate property is
// weak, and nothing else in this file keeps a strong Objective-C reference
// alive past initialize() returning, so without that explicit retain both
// objects would be deallocated (or, for the window, only kept alive by
// AppKit's own bookkeeping, which isn't guaranteed the moment before it's
// shown). The CAMetalLayer needs no such treatment: it's reachable via
// window -> contentView -> layer, all strong properties, so it stays alive
// exactly as long as the window does.
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include "window_macos.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <cstdio>

// A plain NSView whose backing CALayer is a CAMetalLayer, so Vulkan (via
// MoltenVK) has a Metal layer to build a swapchain-backed drawable from —
// the macOS/Metal equivalent of the Xlib backend handing over a raw
// X11 Window handle.
@interface VREMetalView : NSView
@end

@implementation VREMetalView
+ (Class)layerClass
{
    return [CAMetalLayer class];
}
- (CALayer *)makeBackingLayer
{
    return [CAMetalLayer layer];
}
@end

// Bridges AppKit window events (close button, resize) back into the plain
// C++ WindowMacOS instance. Holds a raw, non-owning pointer: WindowMacOS
// owns this delegate's lifetime (via CFBridgingRetain/Release in
// initialize()/destroy()), not the other way around.
@interface VREWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) vre::WindowMacOS *owner;
@end

@implementation VREWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
    (void)sender;
    self.owner->on_close_requested();
    // Returning NO keeps AppKit from tearing the window down immediately;
    // the demo's main loop notices should_close() and exits on its own
    // terms, mirroring WindowLinux's WM_DELETE_WINDOW handling (which also
    // just sets a flag rather than closing the X11 window itself).
    return NO;
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

namespace vre
{

// Apple virtual keycodes: layout-independent hardware scan codes, same
// numeric IDs as Carbon's HIToolbox <Events.h> kVK_* constants. Hardcoded
// here (rather than linking the Carbon framework just for these names) —
// they're a stable, documented part of the platform ABI, not an
// implementation detail likely to change.
namespace
{
constexpr unsigned short kVkAnsiA = 0x00;
constexpr unsigned short kVkAnsiS = 0x01;
constexpr unsigned short kVkAnsiD = 0x02;
constexpr unsigned short kVkAnsiF = 0x03;
constexpr unsigned short kVkAnsiH = 0x04;
constexpr unsigned short kVkAnsiW = 0x0D;
constexpr unsigned short kVkAnsiE = 0x0E;
constexpr unsigned short kVkEscape = 0x35;
constexpr unsigned short kVkLeftArrow = 0x7B;
constexpr unsigned short kVkRightArrow = 0x7C;
constexpr unsigned short kVkDownArrow = 0x7D;
constexpr unsigned short kVkUpArrow = 0x7E;

bool virtual_keycode_to_keycode(unsigned short keycode, KeyCode *out_code)
{
    switch (keycode)
    {
        case kVkAnsiH: *out_code = KeyCode::H; return true;
        case kVkEscape: *out_code = KeyCode::Escape; return true;
        case kVkAnsiW: *out_code = KeyCode::W; return true;
        case kVkAnsiA: *out_code = KeyCode::A; return true;
        case kVkAnsiS: *out_code = KeyCode::S; return true;
        case kVkAnsiD: *out_code = KeyCode::D; return true;
        case kVkLeftArrow: *out_code = KeyCode::Left; return true;
        case kVkRightArrow: *out_code = KeyCode::Right; return true;
        case kVkUpArrow: *out_code = KeyCode::Up; return true;
        case kVkDownArrow: *out_code = KeyCode::Down; return true;
        case kVkAnsiE: *out_code = KeyCode::E; return true;
        case kVkAnsiF: *out_code = KeyCode::F; return true;
        default: return false;
    }
}
} // namespace

WindowMacOS::WindowMacOS()
    : _ns_window(nullptr), _metal_layer(nullptr), _delegate(nullptr),
      _width(0), _height(0), _close_requested(false), _resized(false)
{
}

WindowMacOS::~WindowMacOS()
{
    destroy();
}

bool WindowMacOS::initialize(const char *title, int32_t width, int32_t height)
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect frame = NSMakeRect(0, 0, width, height);
        NSWindowStyleMask style = NSWindowStyleMaskTitled
            | NSWindowStyleMaskClosable
            | NSWindowStyleMaskResizable
            | NSWindowStyleMaskMiniaturizable;

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

        VREMetalView *view = [[VREMetalView alloc] initWithFrame:frame];
        view.wantsLayer = YES;
        window.contentView = view;
        [window makeFirstResponder:view];

        CAMetalLayer *layer = static_cast<CAMetalLayer *>(view.layer);
        CGFloat scale = window.backingScaleFactor;
        layer.contentsScale = scale;
        _width = static_cast<int32_t>(frame.size.width * scale);
        _height = static_cast<int32_t>(frame.size.height * scale);
        layer.drawableSize = CGSizeMake(_width, _height);

        VREWindowDelegate *delegate = [[VREWindowDelegate alloc] init];
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
    @autoreleasepool
    {
        if (_ns_window != nullptr)
        {
            NSWindow *window = (__bridge NSWindow *)_ns_window;
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

void WindowMacOS::on_resized(int32_t width, int32_t height)
{
    if (width != _width || height != _height)
    {
        _width = width;
        _height = height;
        _resized = true;
    }
}

void WindowMacOS::poll_events()
{
    for (bool &pressed : _key_pressed_this_poll)
        pressed = false;

    @autoreleasepool
    {
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES]) != nil)
        {
            if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)
            {
                KeyCode code;
                if (virtual_keycode_to_keycode(event.keyCode, &code))
                {
                    bool down = (event.type == NSEventTypeKeyDown);
                    _key_held[static_cast<size_t>(code)] = down;
                    // NSEvent.isARepeat marks OS-generated auto-repeat
                    // keyDowns explicitly, unlike X11 (see the peek-ahead
                    // workaround in WindowLinux::poll_events) — so an
                    // edge-triggered "was pressed" just needs to ignore
                    // those, no repeat-vs-release disambiguation needed.
                    if (down && !event.isARepeat)
                        _key_pressed_this_poll[static_cast<size_t>(code)] = true;
                }
                // Deliberately not forwarding to -[NSApp sendEvent:]: there
                // is no text-input first responder to consume these, and
                // sending an unhandled keyDown up the responder chain makes
                // AppKit beep on every keystroke.
                continue;
            }
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
}

void WindowMacOS::get_required_instance_extensions(
    const char **out_extensions, uint32_t *out_count) const
{
    static const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
    };
    if (out_extensions != nullptr)
    {
        out_extensions[0] = extensions[0];
        out_extensions[1] = extensions[1];
    }
    if (out_count != nullptr)
        *out_count = 2;
}

VkResult WindowMacOS::create_vulkan_surface(
    VkInstance instance, VkSurfaceKHR *out_surface) const
{
    VkMetalSurfaceCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    create_info.pLayer = (__bridge const CAMetalLayer *)_metal_layer;
    return vkCreateMetalSurfaceEXT(instance, &create_info, nullptr, out_surface);
}

} // namespace vre

vre::Window *vre::Window::create()
{
    return new vre::WindowMacOS();
}
