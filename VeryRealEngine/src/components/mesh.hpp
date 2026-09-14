/**
 * @file mesh.hpp
 * @brief Marks an entity as having a renderable mesh.
 */
#pragma once

#include "../rendererresources/meshhandle.hpp"
#include "../vre.hpp"

namespace vre
{
class MeshComponent
{
  public:
	MeshComponent();
	MeshComponent(const MeshComponent &other);
	MeshComponent &operator=(const MeshComponent &other);
	~MeshComponent();

	explicit MeshComponent(MeshHandle mesh);

	MeshHandle mesh() const;
	void set_mesh(MeshHandle value);

	/// @return The sentinel MeshHandle meaning "no mesh" — an
	/// organizational or physics-only entity.
	static MeshHandle no_mesh();

  private:
	MeshHandle _mesh;
};

} // namespace vre
