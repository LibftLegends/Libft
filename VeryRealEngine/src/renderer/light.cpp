#include "light.hpp"

namespace vre
{
Light::Light() : _type(Type::Directional), _color(1.0f, 1.0f, 1.0f),
	_intensity(1.0f)
{
}

Light::Light(const Light &other) : _type(other._type),
	_direction_or_position(other._direction_or_position), _color(other._color),
	_intensity(other._intensity)
{
}

Light &Light::operator=(const Light &other)
{
	if (this != &other)
	{
		_type = other._type;
		_direction_or_position = other._direction_or_position;
		_color = other._color;
		_intensity = other._intensity;
	}
	return (*this);
}

Light::~Light()
{
}

Light::Type Light::type() const
{
	return (_type);
}

void Light::set_type(Type value)
{
	_type = value;
}

const vec3 &Light::direction_or_position() const
{
	return (_direction_or_position);
}

void Light::set_direction_or_position(const vec3 &value)
{
	_direction_or_position = value;
}

const vec3 &Light::color() const
{
	return (_color);
}

void Light::set_color(const vec3 &value)
{
	_color = value;
}

float Light::intensity() const
{
	return (_intensity);
}

void Light::set_intensity(float value)
{
	_intensity = value;
}

} // namespace vre
