#include "mat4.hpp"

namespace vre
{

mat4::mat4()
{
	std::memset(_m, 0, sizeof(_m));
}

mat4::mat4(const mat4 &other)
{
	std::memcpy(_m, other._m, sizeof(_m));
}

mat4 &mat4::operator=(const mat4 &other)
{
	if (this != &other)
		std::memcpy(_m, other._m, sizeof(_m));
	return (*this);
}

mat4::~mat4()
{
}

float mat4::m(size_t index) const
{
	return (_m[index]);
}

mat4 mat4::identity()
{
	mat4	result;

	result._m[0] = 1.0f;
	result._m[5] = 1.0f;
	result._m[10] = 1.0f;
	result._m[15] = 1.0f;
	return (result);
}

mat4 mat4::multiply(const mat4 &a, const mat4 &b)
{
	mat4	result;
	float	sum;

	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			sum = 0.0f;
			for (int k = 0; k < 4; k++)
				sum += a._m[k * 4 + row] * b._m[col * 4 + k];
			result._m[col * 4 + row] = sum;
		}
	}
	return (result);
}

mat4 mat4::translate(const vec3 &t)
{
	mat4	result;

	result = identity();
	result._m[12] = t.x();
	result._m[13] = t.y();
	result._m[14] = t.z();
	return (result);
}

mat4 mat4::rotate_y(float radians)
{
	mat4	result;
	float	c;
	float	s;

	result = identity();
	c = std::cos(radians);
	s = std::sin(radians);
	result._m[0] = c;
	result._m[2] = -s;
	result._m[8] = s;
	result._m[10] = c;
	return (result);
}

mat4 mat4::rotate_x(float radians)
{
	mat4	result;
	float	c;
	float	s;

	result = identity();
	c = std::cos(radians);
	s = std::sin(radians);
	result._m[5] = c;
	result._m[6] = s;
	result._m[9] = -s;
	result._m[10] = c;
	return (result);
}

mat4 mat4::rotate_z(float radians)
{
	mat4	result;
	float	c;
	float	s;

	result = identity();
	c = std::cos(radians);
	s = std::sin(radians);
	result._m[0] = c;
	result._m[1] = s;
	result._m[4] = -s;
	result._m[5] = c;
	return (result);
}

mat4 mat4::scale(const vec3 &s)
{
	mat4	result;

	result = identity();
	result._m[0] = s.x();
	result._m[5] = s.y();
	result._m[10] = s.z();
	return (result);
}

} // namespace vre
