#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <typeindex>
#include <unordered_map>
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
