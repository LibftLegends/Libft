#include "texture_handle.hpp"

namespace vre
{

TextureHandle::TextureHandle() : _value(invalid()._value)
{
}

TextureHandle::TextureHandle(const TextureHandle &other) : _value(other._value)
{
}

TextureHandle &TextureHandle::operator=(const TextureHandle &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

TextureHandle::~TextureHandle()
{
}

TextureHandle::TextureHandle(size_t value) : _value(value)
{
}

size_t TextureHandle::value() const
{
	return (_value);
}

bool TextureHandle::is_valid() const
{
	return (_value != invalid()._value);
}

bool TextureHandle::operator==(const TextureHandle &other) const
{
	return (_value == other._value);
}

bool TextureHandle::operator!=(const TextureHandle &other) const
{
	return (_value != other._value);
}

TextureHandle TextureHandle::invalid()
{
	return (TextureHandle(static_cast<size_t>(-1)));
}

} // namespace vre
