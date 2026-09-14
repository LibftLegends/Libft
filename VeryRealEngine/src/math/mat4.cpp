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

mat4 mat4::compose(const vec3 &position, const vec3 &euler_radians,
	const vec3 &scale_factor)
{
	mat4	rotation;

	rotation = multiply(rotate_z(euler_radians.z()),
			multiply(rotate_y(euler_radians.y()), rotate_x(euler_radians.x())));
	return (multiply(translate(position), multiply(rotation,
				scale(scale_factor))));
}

mat4 mat4::look_at(const vec3 &eye, const vec3 &center, const vec3 &up)
{
	vec3	f;
	vec3	s;
	vec3	u;
	mat4	result;

	f = vec3::normalize(center - eye);
	s = vec3::normalize(vec3::cross(f, up));
	u = vec3::cross(s, f);
	result = identity();
	result._m[0] = s.x();
	result._m[4] = s.y();
	result._m[8] = s.z();
	result._m[1] = u.x();
	result._m[5] = u.y();
	result._m[9] = u.z();
	result._m[2] = -f.x();
	result._m[6] = -f.y();
	result._m[10] = -f.z();
	result._m[12] = -vec3::dot(s, eye);
	result._m[13] = -vec3::dot(u, eye);
	result._m[14] = vec3::dot(f, eye);
	return (result);
}

mat4 mat4::perspective(float fov_y_radians, float aspect, float z_near,
	float z_far)
{
	mat4	result;
	float	tan_half_fov;

	tan_half_fov = std::tan(fov_y_radians * 0.5f);
	result._m[0] = 1.0f / (aspect * tan_half_fov);
	result._m[5] = -1.0f / tan_half_fov;
	result._m[10] = z_far / (z_near - z_far);
	result._m[11] = -1.0f;
	result._m[14] = -(z_far * z_near) / (z_far - z_near);
	return (result);
}

mat4 mat4::orthographic(float left, float right, float bottom, float top,
	float z_near, float z_far)
{
	mat4	result;

	result = identity();
	result._m[0] = 2.0f / (right - left);
	result._m[5] = -2.0f / (top - bottom);
	result._m[10] = 1.0f / (z_near - z_far);
	result._m[12] = -(right + left) / (right - left);
	result._m[13] = -(top + bottom) / (top - bottom);
	result._m[14] = z_near / (z_near - z_far);
	return (result);
}

mat4 mat4::inverse(const mat4 &m)
{
	mat4	result;

	if (!Mat4Inverse::compute(m._m, result._m))
		return (identity());
	return (result);
}

vec3 mat4::transform_point(const mat4 &m, const vec3 &p)
{
	return (vec3(m._m[0] * p.x() + m._m[4] * p.y() + m._m[8] * p.z() + m._m[12],
			m._m[1] * p.x() + m._m[5] * p.y() + m._m[9] * p.z() + m._m[13],
			m._m[2] * p.x() + m._m[6] * p.y() + m._m[10] * p.z() + m._m[14]));
}

} // namespace vre
