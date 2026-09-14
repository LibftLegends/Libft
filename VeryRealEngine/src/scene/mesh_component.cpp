#include "mesh_component.hpp"

namespace vre
{

MeshComponent::MeshComponent() : _mesh(no_mesh())
{
}

MeshComponent::MeshComponent(const MeshComponent &other) : _mesh(other._mesh)
{
}

MeshComponent &MeshComponent::operator=(const MeshComponent &other)
{
	if (this != &other)
		_mesh = other._mesh;
	return (*this);
}

MeshComponent::~MeshComponent()
{
}

MeshComponent::MeshComponent(MeshHandle mesh) : _mesh(mesh)
{
}

MeshHandle MeshComponent::mesh() const
{
	return (_mesh);
}

void MeshComponent::set_mesh(MeshHandle value)
{
	_mesh = value;
}

MeshHandle MeshComponent::no_mesh()
{
	return (static_cast<MeshHandle>(-1));
}

} // namespace vre
