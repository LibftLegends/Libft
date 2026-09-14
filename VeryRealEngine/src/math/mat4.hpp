/**
 * @file mat4.hpp
 * @brief Column-major 4x4 matrix, laid out the way Vulkan/GLSL expect it
 * (matches the memory layout of `mat4` in std140/push-constant blocks).
 */
#pragma once

#include "../vre.hpp"
#include "mat4inverse.hpp"
#include "vec3.hpp"

namespace vre
{
class mat4
{
  public:
	mat4();
	mat4(const mat4 &other);
	mat4 &operator=(const mat4 &other);
	~mat4();

	/// @return Element `index` (0-15) of the column-major storage.
	float m(size_t index) const;

	/// @return The 4x4 identity matrix.
	static mat4 identity();
	/// @return The matrix product `a * b`.
	static mat4 multiply(const mat4 &a, const mat4 &b);
	/// @return A translation matrix by `t`.
	static mat4 translate(const vec3 &t);
	/// @return A rotation matrix of `radians` around the Y axis.
	static mat4 rotate_y(float radians);
	/// @return A rotation matrix of `radians` around the X axis.
	static mat4 rotate_x(float radians);
	/// @return A rotation matrix of `radians` around the Z axis.
	static mat4 rotate_z(float radians);
	/// @return A scale matrix by `s`.
	static mat4 scale(const vec3 &s);

	/**
		* @brief Composes a local transform the way the scene graph's JSON
		* fields describe it: scale first, then rotate (Z * Y * X, i.e. roll
		* then yaw then pitch applied to the object), then translate.
		* @param position Translation component.
		* @param euler_radians Rotation, applied Z then Y then X, in radians.
		* @param scale_factor Per-axis scale, applied before rotation.
		* @return The composed transform matrix.
		*/
	static mat4 compose(const vec3 &position, const vec3 &euler_radians,
		const vec3 &scale_factor);

	/**
		* @brief A right-handed look-at view matrix, matching a Y-up world.
		* @param eye Camera position.
		* @param center Point the camera looks at.
		* @param up World up vector.
		*/
	static mat4 look_at(const vec3 &eye, const vec3 &center, const vec3 &up);

	/**
		* @brief Right-handed perspective projection with Vulkan's [0,1] depth
		* range and flipped Y (Vulkan's NDC Y points down, unlike OpenGL's).
		* @param fov_y_radians Vertical field of view, in radians.
		* @param aspect Viewport width / height.
		* @param z_near Near clip distance.
		* @param z_far Far clip distance.
		*/
	static mat4 perspective(float fov_y_radians, float aspect, float z_near,
		float z_far);

	/**
		* @brief Orthographic projection, same Vulkan [0,1] depth range and
		* flipped Y as perspective() above.
		*
		* Used for the directional-light shadow map's projection, where a
		* light "camera" has no perspective falloff.
		*/
	static mat4 orthographic(float left, float right, float bottom, float top,
		float z_near, float z_far);

	/**
		* @brief General 4x4 inverse (see Mat4Inverse for the algorithm).
		* @param m Matrix to invert.
		* @return The inverse of `m`, or the identity matrix if `m` is
		* (numerically) singular.
		*/
	static mat4 inverse(const mat4 &m);

	/// @return `p` transformed by affine matrix `m` (implicit w=1, no
	/// perspective divide).
	static vec3 transform_point(const mat4 &m, const vec3 &p);

  private:
	float _m[16];
};

} // namespace vre
