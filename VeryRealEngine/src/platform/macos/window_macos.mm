// macOS windowing backend implementation (Objective-C++, ARC-enabled — see
// the Makefile's dedicated `.mm` compile rule). Native Cocoa/Metal objects
// (NSWindow, the delegate, CAMetalLayer) are bridged to the plain `void *`
// members declared in window_macos.hpp so that header stays includable from
// non-Objective-C translation units without pulling in Objective-C — see
// the comment there.
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

// A plain NSView whose backing CALayer is a CAMetalLayer, so Vulkan (via
// MoltenVK) has a Metal layer to build a swapchain-backed drawable from —
// the macOS/Metal equivalent of the Xlib backend handing over a raw
// X11 Window handle.
@interface VREMetalView : NSView
@end

@implementation VREMetalView
+ (Class)layerClass
{
    return ([CAMetalLayer class]);
}
- (CALayer *)makeBackingLayer
{
    return ([CAMetalLayer layer]);
}
@end

// Bridges AppKit window events (close button, resize) back into the plain
// C++ WindowMacOS instance. Holds a raw, non-owning pointer: WindowMacOS
// owns this delegate's lifetime (via CFBridgingRetain/Release in
// initialize()/destroy()), not the other way around.
@interface VREWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) vre::WindowMacOS *owner;
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

namespace vre
{

WindowMacOS::WindowMacOS()
    : _ns_window(nullptr), _metal_layer(nullptr), _delegate(nullptr), _width(0),
	_height(0),
      _close_requested(false), _resized(false)
{
    for (bool &pressed : _key_pressed_this_poll)
        pressed = false;
    for (bool &held : _key_held)
        held = false;
}

WindowMacOS::~WindowMacOS()
{
    destroy();
}

bool WindowMacOS::should_close() const
{
    return (_close_requested);
}

int32_t WindowMacOS::get_width() const
{
    return (_width);
}

int32_t WindowMacOS::get_height() const
{
    return (_height);
}

bool WindowMacOS::was_resized() const
{
    return (_resized);
}

void WindowMacOS::clear_resized_flag()
{
    _resized = false;
}

bool WindowMacOS::was_key_pressed(Key key) const
{
    return (_key_pressed_this_poll[static_cast<size_t>(key)]);
}

bool WindowMacOS::is_key_held(Key key) const
{
    return (_key_held[static_cast<size_t>(key)]);
}

void WindowMacOS::on_close_requested()
{
    _close_requested = true;
}

bool WindowMacOS::virtual_keycode_to_key(unsigned short keycode, Key *out_key)
{
    switch (keycode)
    {
    case kVkAnsiH:
        *out_key = Key::H;
        return (true);
    case kVkEscape:
        *out_key = Key::Escape;
        return (true);
    case kVkAnsiW:
        *out_key = Key::W;
        return (true);
    case kVkAnsiA:
        *out_key = Key::A;
        return (true);
    case kVkAnsiS:
        *out_key = Key::S;
        return (true);
    case kVkAnsiD:
        *out_key = Key::D;
        return (true);
    case kVkLeftArrow:
        *out_key = Key::Left;
        return (true);
    case kVkRightArrow:
        *out_key = Key::Right;
        return (true);
    case kVkUpArrow:
        *out_key = Key::Up;
        return (true);
    case kVkDownArrow:
        *out_key = Key::Down;
        return (true);
    case kVkAnsiE:
        *out_key = Key::E;
        return (true);
    case kVkAnsiF:
        *out_key = Key::F;
        return (true);
    default:
        return (false);
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

void WindowMacOS::get_required_instance_extensions(const char **out_extensions,
                                                   uint32_t *out_count) const
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

VkResult WindowMacOS::create_vulkan_surface(VkInstance instance,
	VkSurfaceKHR *out_surface) const
{
    VkMetalSurfaceCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    create_info.pLayer = (__bridge const CAMetalLayer *)_metal_layer;
    return vkCreateMetalSurfaceEXT(instance, &create_info, nullptr,
	out_surface);
}

} // namespace vre

vre::Window *vre::Window::create()
{
    return new vre::WindowMacOS();
}
