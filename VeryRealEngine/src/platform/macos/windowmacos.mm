// macOS windowing backend implementation (Objective-C++, ARC-enabled — see
// the Makefile's dedicated `.mm` compile rule). Native Cocoa/Metal objects
// are bridged to the plain `void *` members declared in windowmacos.hpp so
// that header stays includable from non-Objective-C translation units
// without pulling in Objective-C — see the comment there.
// VREMetalView/VREWindowDelegate (the two small Objective-C helper classes
// this backend needs) live in their own files.
//
// The window and its delegate are given an owning reference via
// CFBridgingRetain/CFBridgingRelease: AppKit's NSWindow.delegate property
// is weak, and nothing else here keeps a strong Objective-C reference
// alive past initialize() returning, so without that explicit retain both
// objects would be deallocated. The CAMetalLayer needs no such treatment:
// it's reachable via window -> contentView -> layer, all strong
// properties, so it stays alive exactly as long as the window does.
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include "vremetalview.hpp"
#include "vrewindowdelegate.hpp"
#include "windowmacos.hpp"

namespace vre
{
WindowMacOS::WindowMacOS() : _ns_window(nullptr), _metal_layer(nullptr),
	_delegate(nullptr), _width(0), _height(0), _close_requested(false),
	_resized(false)
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

void WindowMacOS::on_resized(int32_t width, int32_t height)
{
	if (width != _width || height != _height)
	{
		_width = width;
		_height = height;
		_resized = true;
	}
}

bool WindowMacOS::initialize(const char *title, int32_t width,
	int32_t height)
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
		NSWindowStyleMask style = NSWindowStyleMaskTitled
			| NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
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

void WindowMacOS::poll_events()
{
	NSEvent	*event;
	Key		key;
	bool	down;

	for (bool &pressed : _key_pressed_this_poll)
		pressed = false;
	@autoreleasepool
	{
		while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
										   untilDate:[NSDate distantPast]
											  inMode:NSDefaultRunLoopMode
											 dequeue:YES]) != nil)
		{
			if (event.type == NSEventTypeKeyDown
				|| event.type == NSEventTypeKeyUp)
			{
				if (MacOSKeyMap::from_keycode(event.keyCode, &key))
				{
					down = (event.type == NSEventTypeKeyDown);
					_key_held[static_cast<size_t>(key)] = down;
					// NSEvent.isARepeat marks OS-generated auto-repeat
					// keyDowns explicitly, unlike X11 (see the peek-ahead
					// workaround in WindowLinux::poll_events) — so an
					// edge-triggered "was pressed" just needs to ignore
					// those, no repeat-vs-release disambiguation needed.
					if (down && !event.isARepeat)
						_key_pressed_this_poll[static_cast<size_t>(key)] = true;
				}
				// Deliberately not forwarding to -[NSApp sendEvent:]: there
				// is no text-input first responder to consume these, and
				// sending an unhandled keyDown up the responder chain makes
				// AppKit beep on every keystroke.
				continue ;
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
