/**
 * @file linuxkeymap.hpp
 * @brief Maps an X11 KeySym to this engine's platform-independent Window::Key.
 */
#pragma once

#include "../../vre.hpp"
#include "../window.hpp"

namespace vre
{
class LinuxKeyMap
{
  public:
	LinuxKeyMap();
	LinuxKeyMap(const LinuxKeyMap &other);
	LinuxKeyMap &operator=(const LinuxKeyMap &other);
	~LinuxKeyMap();

	/// @return false (leaving `out_key` untouched) if `sym` isn't one of
	/// the keys the engine's demo cares about (see Window::Key).
	static bool from_keysym(KeySym sym, Window::Key *out_key);
};

} // namespace vre
