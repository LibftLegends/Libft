#include "skeleton.hpp"

namespace vre
{
Skeleton::Skeleton()
{
}

Skeleton::Skeleton(const Skeleton &other) : _bones(other._bones),
	_bind_pose_inverse(other._bind_pose_inverse)
{
}

Skeleton &Skeleton::operator=(const Skeleton &other)
{
	if (this != &other)
	{
		_bones = other._bones;
		_bind_pose_inverse = other._bind_pose_inverse;
	}
	return (*this);
}

Skeleton::~Skeleton()
{
}

std::vector<Bone> &Skeleton::bones()
{
	return (_bones);
}

const std::vector<Bone> &Skeleton::bones() const
{
	return (_bones);
}

void Skeleton::compute_bind_pose()
{
	std::vector<mat4> bind_world(_bones.size());
	for (size_t i = 0; i < _bones.size(); i++)
	{
		// rigid only — see this file's header comment
		mat4 local = mat4::compose(_bones[i].bind_local_position(),
				_bones[i].bind_local_rotation(), vec3(1.0f, 1.0f, 1.0f));
		if (_bones[i].parent_index() == Bone::no_parent_index())
			bind_world[i] = local;
		else
			bind_world[i] = mat4::multiply(
					bind_world[_bones[i].parent_index()], local);
	}
	_bind_pose_inverse.resize(_bones.size());
	for (size_t i = 0; i < _bones.size(); i++)
		_bind_pose_inverse[i] = mat4::inverse(bind_world[i]);
}

const mat4 &Skeleton::bind_pose_inverse(size_t index) const
{
	return (_bind_pose_inverse[index]);
}

} // namespace vre
