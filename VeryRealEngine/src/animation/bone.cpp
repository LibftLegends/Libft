#include "bone.hpp"

namespace vre
{
Bone::Bone() : _parent_index(no_parent_index())
{
}

Bone::Bone(const Bone &other) : _name(other._name),
	_parent_index(other._parent_index),
	_bind_local_position(other._bind_local_position),
	_bind_local_rotation(other._bind_local_rotation)
{
}

Bone &Bone::operator=(const Bone &other)
{
	if (this != &other)
	{
		_name = other._name;
		_parent_index = other._parent_index;
		_bind_local_position = other._bind_local_position;
		_bind_local_rotation = other._bind_local_rotation;
	}
	return (*this);
}

Bone::~Bone()
{
}

const std::string &Bone::name() const
{
	return (_name);
}

void Bone::set_name(const std::string &value)
{
	_name = value;
}

int32_t Bone::parent_index() const
{
	return (_parent_index);
}

void Bone::set_parent_index(int32_t value)
{
	_parent_index = value;
}

const vec3 &Bone::bind_local_position() const
{
	return (_bind_local_position);
}

void Bone::set_bind_local_position(const vec3 &value)
{
	_bind_local_position = value;
}

const vec3 &Bone::bind_local_rotation() const
{
	return (_bind_local_rotation);
}

void Bone::set_bind_local_rotation(const vec3 &value)
{
	_bind_local_rotation = value;
}

int32_t Bone::no_parent_index()
{
	return (-1);
}

} // namespace vre
