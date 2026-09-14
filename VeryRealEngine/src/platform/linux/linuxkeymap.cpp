#include "linuxkeymap.hpp"

namespace vre
{
LinuxKeyMap::LinuxKeyMap()
{
}

LinuxKeyMap::LinuxKeyMap(const LinuxKeyMap &)
{
}

LinuxKeyMap &LinuxKeyMap::operator=(const LinuxKeyMap &)
{
	return (*this);
}

LinuxKeyMap::~LinuxKeyMap()
{
}

bool LinuxKeyMap::from_keysym(KeySym sym, Window::Key *out_key)
{
	switch (sym)
	{
	case XK_h:
	case XK_H:
		*out_key = Window::Key::H;
		return (true);
	case XK_Escape:
		*out_key = Window::Key::Escape;
		return (true);
	case XK_w:
	case XK_W:
		*out_key = Window::Key::W;
		return (true);
	case XK_a:
	case XK_A:
		*out_key = Window::Key::A;
		return (true);
	case XK_s:
	case XK_S:
		*out_key = Window::Key::S;
		return (true);
	case XK_d:
	case XK_D:
		*out_key = Window::Key::D;
		return (true);
	case XK_Left:
		*out_key = Window::Key::Left;
		return (true);
	case XK_Right:
		*out_key = Window::Key::Right;
		return (true);
	case XK_Up:
		*out_key = Window::Key::Up;
		return (true);
	case XK_Down:
		*out_key = Window::Key::Down;
		return (true);
	case XK_e:
	case XK_E:
		*out_key = Window::Key::E;
		return (true);
	case XK_f:
	case XK_F:
		*out_key = Window::Key::F;
		return (true);
	default:
		return (false);
	}
}

} // namespace vre
