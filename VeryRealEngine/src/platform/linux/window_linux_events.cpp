/**
 * @file window_linux_events.cpp
 * @brief WindowLinux::poll_events()'s body, split out of window_linux.cpp
 * purely to keep each file under this project's 250-line cap — both files
 * define methods of the same WindowLinux class declared in
 * window_linux.hpp.
 */
#include "window_linux.hpp"

namespace vre
{

void WindowLinux::poll_events()
{
    for (bool &pressed : _key_pressed_this_poll)
        pressed = false;

    while (XPending(_display) > 0)
    {
        XEvent event;
        XNextEvent(_display, &event);

        if (event.type == KeyPress)
        {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            Key key;
            if (keysym_to_key(sym, &key))
            {
                _key_pressed_this_poll[static_cast<size_t>(key)] = true;
                _key_held[static_cast<size_t>(key)] = true;
            }
        }
        else if (event.type == KeyRelease)
        {
            // X11 auto-repeat sends a release immediately followed by a
            // press (same keycode, same timestamp) while a key is held
            // down — without filtering that out, is_key_held() would
            // flicker false between repeats instead of staying true.
            bool is_repeat = false;
            if (XEventsQueued(_display, QueuedAfterReading) > 0)
            {
                XEvent next_event;
                XPeekEvent(_display, &next_event);
                if (next_event.type == KeyPress
                    && next_event.xkey.keycode == event.xkey.keycode
                    && next_event.xkey.time == event.xkey.time)
                    is_repeat = true;
            }
            if (!is_repeat)
            {
                KeySym sym = XLookupKeysym(&event.xkey, 0);
                Key key;
                if (keysym_to_key(sym, &key))
                    _key_held[static_cast<size_t>(key)] = false;
            }
        }
        else if (event.type == ClientMessage)
        {
            if (static_cast<Atom>(event.xclient.data.l[0]) == _wm_delete_window)
                _close_requested = true;
        }
        else if (event.type == ConfigureNotify)
        {
            int32_t new_width = event.xconfigure.width;
            int32_t new_height = event.xconfigure.height;
            if (new_width != _width || new_height != _height)
            {
                _width = new_width;
                _height = new_height;
                _resized = true;
            }
        }
    }
}

} // namespace vre
