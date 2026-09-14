/**
 * @file window_macos_events.mm
 * @brief WindowMacOS::poll_events()'s body, split out of window_macos.mm
 * purely to keep each file under this project's 250-line cap — both files
 * define methods of the same WindowMacOS class declared in
 * window_macos.hpp.
 */
#import <Cocoa/Cocoa.h>

#include "window_macos.hpp"

namespace vre
{

void WindowMacOS::poll_events()
{
        NSEvent *event;
                Key key;
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
                if (virtual_keycode_to_key(event.keyCode, &key))
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

} // namespace vre
