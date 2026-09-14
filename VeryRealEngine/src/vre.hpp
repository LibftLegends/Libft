/**
 * @file vre.hpp
 * @brief Single point of entry for every *external* (non-project) header
 * VeryRealEngine needs — the Vulkan API, the C++ standard library, and the
 * handful of Linux system headers (Xlib, ALSA) that count as "system
 * libraries" the same way the subject's own XCB example does.
 *
 * Every project header includes this one instead of reaching for
 * `<vector>`/`<vulkan/vulkan.h>`/etc. directly, and every `.cpp` file
 * includes only its own matching `.hpp` — so this file is the one place
 * that ever needs to change if a new external dependency is introduced.
 *
 * One documented exception: Apple's Cocoa/QuartzCore headers are
 * Objective-C and only parse inside a `.mm` translation unit compiled in
 * Objective-C++ mode. This header is included by plain `.cpp` files too, so
 * it cannot pull those in — `platform/macos/window_macos.mm` includes them
 * directly, in addition to this file, which is the one place "a .cpp
 * includes only its own .hpp" cannot hold, because the language leaves no
 * other option. Nothing else in the project ever sees those headers;
 * everything funnels through the platform-independent `Window` interface.
 */
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <vulkan/vulkan.h>

#ifdef __linux__
# include <X11/Xatom.h>
# include <X11/Xlib.h>
# include <X11/keysym.h>
# include <alsa/asoundlib.h>
# include <vulkan/vulkan_xlib.h>
#endif

#ifdef __APPLE__
# include <vulkan/vulkan_metal.h>
#endif
