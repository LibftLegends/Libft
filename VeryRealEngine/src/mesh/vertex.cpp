#include "vertex.hpp"

namespace vre
{
MeshVertex::MeshVertex()
{
	for (size_t i = 0; i < 3; i++)
	{
		_position[i] = 0.0f;
		_normal[i] = 0.0f;
	}
	_uv[0] = 0.0f;
	_uv[1] = 0.0f;
	for (size_t i = 0; i < 4; i++)
	{
		_bone_indices[i] = 0.0f;
		_bone_weights[i] = 0.0f;
	}
	_bone_weights[0] = 1.0f;
}

MeshVertex::MeshVertex(const MeshVertex &other)
{
	std::memcpy(_position, other._position, sizeof(_position));
	std::memcpy(_normal, other._normal, sizeof(_normal));
	std::memcpy(_uv, other._uv, sizeof(_uv));
	std::memcpy(_bone_indices, other._bone_indices, sizeof(_bone_indices));
	std::memcpy(_bone_weights, other._bone_weights, sizeof(_bone_weights));
}

MeshVertex &MeshVertex::operator=(const MeshVertex &other)
{
	if (this != &other)
	{
		std::memcpy(_position, other._position, sizeof(_position));
		std::memcpy(_normal, other._normal, sizeof(_normal));
		std::memcpy(_uv, other._uv, sizeof(_uv));
		std::memcpy(_bone_indices, other._bone_indices, sizeof(_bone_indices));
		std::memcpy(_bone_weights, other._bone_weights, sizeof(_bone_weights));
	}
	return (*this);
}

MeshVertex::~MeshVertex()
{
}

float MeshVertex::position(size_t index) const
{
	return (_position[index]);
}

void MeshVertex::set_position(size_t index, float value)
{
	_position[index] = value;
}

void MeshVertex::set_position(const float value[3])
{
	std::memcpy(_position, value, sizeof(_position));
}

float MeshVertex::normal(size_t index) const
{
	return (_normal[index]);
}

void MeshVertex::set_normal(size_t index, float value)
{
	_normal[index] = value;
}

void MeshVertex::set_normal(const float value[3])
{
	std::memcpy(_normal, value, sizeof(_normal));
}

float MeshVertex::uv(size_t index) const
{
	return (_uv[index]);
}

void MeshVertex::set_uv(size_t index, float value)
{
	_uv[index] = value;
}

void MeshVertex::set_uv(const float value[2])
{
	std::memcpy(_uv, value, sizeof(_uv));
}

float MeshVertex::bone_index(size_t index) const
{
	return (_bone_indices[index]);
}

void MeshVertex::set_bone_index(size_t index, float value)
{
	_bone_indices[index] = value;
}

float MeshVertex::bone_weight(size_t index) const
{
	return (_bone_weights[index]);
}

void MeshVertex::set_bone_weight(size_t index, float value)
{
	_bone_weights[index] = value;
}

size_t MeshVertex::position_offset()
{
	return (offsetof(MeshVertex, _position));
}

size_t MeshVertex::normal_offset()
{
	return (offsetof(MeshVertex, _normal));
}

size_t MeshVertex::uv_offset()
{
	return (offsetof(MeshVertex, _uv));
}

size_t MeshVertex::bone_indices_offset()
{
	return (offsetof(MeshVertex, _bone_indices));
}

size_t MeshVertex::bone_weights_offset()
{
	return (offsetof(MeshVertex, _bone_weights));
}

} // namespace vre
