#include "data.hpp"

namespace vre
{
MeshData::MeshData()
{
}

MeshData::MeshData(const MeshData &other) : _vertices(other._vertices),
	_indices(other._indices), _submeshes(other._submeshes)
{
}

MeshData &MeshData::operator=(const MeshData &other)
{
	if (this != &other)
	{
		_vertices = other._vertices;
		_indices = other._indices;
		_submeshes = other._submeshes;
	}
	return (*this);
}

MeshData::~MeshData()
{
}

std::vector<MeshVertex> &MeshData::vertices()
{
	return (_vertices);
}

const std::vector<MeshVertex> &MeshData::vertices() const
{
	return (_vertices);
}

std::vector<uint32_t> &MeshData::indices()
{
	return (_indices);
}

const std::vector<uint32_t> &MeshData::indices() const
{
	return (_indices);
}

std::vector<SubMesh> &MeshData::submeshes()
{
	return (_submeshes);
}

const std::vector<SubMesh> &MeshData::submeshes() const
{
	return (_submeshes);
}

} // namespace vre
