#include "submesh.hpp"

namespace vre
{
SubMesh::SubMesh() : _index_offset(0), _index_count(0), _material_index(-1)
{
}

SubMesh::SubMesh(const SubMesh &other) : _index_offset(other._index_offset),
	_index_count(other._index_count), _material_index(other._material_index)
{
}

SubMesh &SubMesh::operator=(const SubMesh &other)
{
	if (this != &other)
	{
		_index_offset = other._index_offset;
		_index_count = other._index_count;
		_material_index = other._material_index;
	}
	return (*this);
}

SubMesh::~SubMesh()
{
}

uint32_t SubMesh::index_offset() const
{
	return (_index_offset);
}

void SubMesh::set_index_offset(uint32_t value)
{
	_index_offset = value;
}

uint32_t SubMesh::index_count() const
{
	return (_index_count);
}

void SubMesh::set_index_count(uint32_t value)
{
	_index_count = value;
}

int32_t SubMesh::material_index() const
{
	return (_material_index);
}

void SubMesh::set_material_index(int32_t value)
{
	_material_index = value;
}

} // namespace vre
