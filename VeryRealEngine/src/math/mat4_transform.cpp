/**
 * @file mat4_transform.cpp
 * @brief View/projection/inverse method bodies for mat4, split out of
 * mat4.cpp purely to keep each file under this project's 250-line cap —
 * both files define methods of the same `mat4` class declared in mat4.hpp.
 */
#include "mat4.hpp"

namespace vre
{

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
	const float	*a = m._m;
	float		inv[16];
	float		determinant;
	float		inverse_determinant;
	mat4		result;

	inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15]
		+ a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
	inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15]
		- a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
	inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15]
		+ a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
	inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14]
		- a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
	inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15]
		- a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
	inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15]
		+ a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
	inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15]
		- a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
	inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14]
		+ a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
	inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15]
		+ a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
	inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15]
		- a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
	inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15]
		+ a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
	inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14]
		- a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
	inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11]
		- a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
	inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11]
		+ a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
	inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11]
		- a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
	inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10]
		+ a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
	determinant = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3]
		* inv[12];
	if (std::fabs(determinant) < 1e-12f)
		return (identity());
	inverse_determinant = 1.0f / determinant;
	for (int i = 0; i < 16; i++)
		result._m[i] = inv[i] * inverse_determinant;
	return (result);
}

vec3 mat4::transform_point(const mat4 &m, const vec3 &p)
{
	return (vec3(m._m[0] * p.x() + m._m[4] * p.y() + m._m[8] * p.z() + m._m[12],
			m._m[1] * p.x() + m._m[5] * p.y() + m._m[9] * p.z() + m._m[13],
			m._m[2] * p.x() + m._m[6] * p.y() + m._m[10] * p.z() + m._m[14]));
}

} // namespace vre
