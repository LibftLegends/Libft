#include "vec3.hpp"

namespace vre
{
vec3::vec3() : _x(0.0f), _y(0.0f), _z(0.0f)
{
}

vec3::vec3(const vec3 &other) : _x(other._x), _y(other._y), _z(other._z)
{
}

vec3 &vec3::operator=(const vec3 &other)
{
	if (this != &other)
	{
		_x = other._x;
		_y = other._y;
		_z = other._z;
	}
	return (*this);
}

vec3::~vec3()
{
}

vec3::vec3(float x, float y, float z) : _x(x), _y(y), _z(z)
{
}

float vec3::x() const
{
	return (_x);
}

float vec3::y() const
{
	return (_y);
}

float vec3::z() const
{
	return (_z);
}

void vec3::set_x(float value)
{
	_x = value;
}

void vec3::set_y(float value)
{
	_y = value;
}

void vec3::set_z(float value)
{
	_z = value;
}

vec3 vec3::operator+(const vec3 &other) const
{
	return (vec3(_x + other._x, _y + other._y, _z + other._z));
}

vec3 vec3::operator-(const vec3 &other) const
{
	return (vec3(_x - other._x, _y - other._y, _z - other._z));
}

vec3 vec3::operator*(float scalar) const
{
	return (vec3(_x * scalar, _y * scalar, _z * scalar));
}

float vec3::dot(const vec3 &a, const vec3 &b)
{
	return (a._x * b._x + a._y * b._y + a._z * b._z);
}

vec3 vec3::cross(const vec3 &a, const vec3 &b)
{
	return (vec3(a._y * b._z - a._z * b._y, a._z * b._x - a._x * b._z, a._x
			* b._y - a._y * b._x));
}

vec3 vec3::normalize(const vec3 &v)
{
	float	length;

	length = std::sqrt(dot(v, v));
	if (length <= 0.0f)
		return (vec3(0.0f, 0.0f, 0.0f));
	return (vec3(v._x / length, v._y / length, v._z / length));
}

} // namespace vre
