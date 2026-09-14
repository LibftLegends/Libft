/**
 * @file vec3.hpp
 * @brief A 3-component vector: positions, directions, and colors throughout
 * the engine.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
/// A 3-component vector, used throughout the engine for positions,
/// directions, and colors.
class vec3
{
  public:
	vec3();
	vec3(const vec3 &other);
	vec3 &operator=(const vec3 &other);
	~vec3();

	vec3(float x, float y, float z);

	float x() const;
	float y() const;
	float z() const;
	void set_x(float value);
	void set_y(float value);
	void set_z(float value);

	vec3 operator+(const vec3 &other) const;
	vec3 operator-(const vec3 &other) const;
	vec3 operator*(float scalar) const;

	/// @return The dot product of `a` and `b`.
	static float dot(const vec3 &a, const vec3 &b);
	/// @return The cross product of `a` and `b`.
	static vec3 cross(const vec3 &a, const vec3 &b);
	/// @return `v` scaled to unit length, or the zero vector if `v` is
	/// (numerically) zero-length.
	static vec3 normalize(const vec3 &v);

  private:
	float _x;
	float _y;
	float _z;
};

} // namespace vre
