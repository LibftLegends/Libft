#include "aabb.hpp"

namespace vre
{
AABB::AABB() : _min(vec3(0.0f, 0.0f, 0.0f)), _max(vec3(0.0f, 0.0f, 0.0f))
{
}

AABB::AABB(const AABB &other) : _min(other._min), _max(other._max)
{
}

AABB &AABB::operator=(const AABB &other)
{
	if (this != &other)
	{
		_min = other._min;
		_max = other._max;
	}
	return (*this);
}

AABB::~AABB()
{
}

AABB::AABB(const vec3 &min, const vec3 &max) : _min(min), _max(max)
{
}

const vec3 &AABB::min() const
{
	return (_min);
}

const vec3 &AABB::max() const
{
	return (_max);
}

void AABB::encapsulate(const vec3 &point)
{
	_min = vec3(std::min(_min.x(), point.x()), std::min(_min.y(), point.y()),
			std::min(_min.z(), point.z()));
	_max = vec3(std::max(_max.x(), point.x()), std::max(_max.y(), point.y()),
			std::max(_max.z(), point.z()));
}

AABB AABB::transform(const AABB &local, const mat4 &model)
{
	const vec3 corners[8] = {
		vec3(local._min.x(), local._min.y(), local._min.z()),
		vec3(local._max.x(), local._min.y(), local._min.z()),
		vec3(local._min.x(), local._max.y(), local._min.z()),
		vec3(local._max.x(), local._max.y(), local._min.z()),
		vec3(local._min.x(), local._min.y(), local._max.z()),
		vec3(local._max.x(), local._min.y(), local._max.z()),
		vec3(local._min.x(), local._max.y(), local._max.z()),
		vec3(local._max.x(), local._max.y(), local._max.z()),
	};
	AABB result(mat4::transform_point(model, corners[0]),
		mat4::transform_point(model, corners[0]));
	for (int i = 1; i < 8; i++)
		result.encapsulate(mat4::transform_point(model, corners[i]));
	return (result);
}

} // namespace vre
