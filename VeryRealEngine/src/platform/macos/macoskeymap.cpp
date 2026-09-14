#include "macoskeymap.hpp"

namespace vre
{
namespace
{

// Apple virtual keycodes: layout-independent hardware scan codes, same
// numeric IDs as Carbon's HIToolbox <Events.h> kVK_* constants. Hardcoded
// here (rather than linking the Carbon framework just for these names) —
// they're a stable, documented part of the platform ABI, not an
// implementation detail likely to change.
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

} // namespace

MacOSKeyMap::MacOSKeyMap()
{
}

MacOSKeyMap::MacOSKeyMap(const MacOSKeyMap &)
{
}

MacOSKeyMap &MacOSKeyMap::operator=(const MacOSKeyMap &)
{
	return (*this);
}

MacOSKeyMap::~MacOSKeyMap()
{
}

bool MacOSKeyMap::from_keycode(unsigned short keycode, Window::Key *out_key)
{
	switch (keycode)
	{
	case kVkAnsiH:
		*out_key = Window::Key::H;
		return (true);
	case kVkEscape:
		*out_key = Window::Key::Escape;
		return (true);
	case kVkAnsiW:
		*out_key = Window::Key::W;
		return (true);
	case kVkAnsiA:
		*out_key = Window::Key::A;
		return (true);
	case kVkAnsiS:
		*out_key = Window::Key::S;
		return (true);
	case kVkAnsiD:
		*out_key = Window::Key::D;
		return (true);
	case kVkLeftArrow:
		*out_key = Window::Key::Left;
		return (true);
	case kVkRightArrow:
		*out_key = Window::Key::Right;
		return (true);
	case kVkUpArrow:
		*out_key = Window::Key::Up;
		return (true);
	case kVkDownArrow:
		*out_key = Window::Key::Down;
		return (true);
	case kVkAnsiE:
		*out_key = Window::Key::E;
		return (true);
	case kVkAnsiF:
		*out_key = Window::Key::F;
		return (true);
	default:
		return (false);
	}
}

} // namespace vre
