#include "keyframe.hpp"

namespace vre
{

Keyframe::Keyframe() : _time(0.0f)
{
}

Keyframe::Keyframe(const Keyframe &other) : _time(other._time),
	_rotation(other._rotation)
{
}

Keyframe &Keyframe::operator=(const Keyframe &other)
{
	if (this != &other)
	{
		_time = other._time;
		_rotation = other._rotation;
	}
	return (*this);
}

Keyframe::~Keyframe()
{
}

float Keyframe::time() const
{
	return (_time);
}

void Keyframe::set_time(float value)
{
	_time = value;
}

const vec3 &Keyframe::rotation() const
{
	return (_rotation);
}

void Keyframe::set_rotation(const vec3 &value)
{
	_rotation = value;
}

} // namespace vre
