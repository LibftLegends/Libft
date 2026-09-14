/**
 * @file macoskeymap.hpp
 * @brief Maps an Apple virtual keycode to this engine's platform-independent
 * Window::Key.
 */
#pragma once

#include "../../vre.hpp"
#include "../window.hpp"

namespace vre
{
class MacOSKeyMap
{
  public:
	MacOSKeyMap();
	MacOSKeyMap(const MacOSKeyMap &other);
	MacOSKeyMap &operator=(const MacOSKeyMap &other);
	~MacOSKeyMap();

	/// @return false (leaving `out_key` untouched) if `keycode` isn't
	/// one of the keys the engine's demo cares about (see Window::Key).
	static bool from_keycode(unsigned short keycode, Window::Key *out_key);
};

} // namespace vre
