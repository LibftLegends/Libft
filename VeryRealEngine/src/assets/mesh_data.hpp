/**
 * @file mesh_data.hpp
 * @brief Parsed geometry for one mesh: vertices, indices, and per-material
 * submesh ranges — shared by the OBJ loader and the renderer's GPU upload
 * path.
 *
 * Kept separate from renderer.hpp so asset loading has no Vulkan
 * dependency at all.
 */
#pragma once

#include "../vre.hpp"
#include "mesh_vertex.hpp"
#include "sub_mesh.hpp"

namespace vre
{

class MeshData
{
  public:
	MeshData();
	MeshData(const MeshData &other);
	MeshData &operator=(const MeshData &other);
	~MeshData();

	std::vector<MeshVertex> &vertices();
	const std::vector<MeshVertex> &vertices() const;

	std::vector<uint32_t> &indices();
	const std::vector<uint32_t> &indices() const;

	std::vector<SubMesh> &submeshes();
	const std::vector<SubMesh> &submeshes() const;

  private:
	std::vector<MeshVertex> _vertices;
	std::vector<uint32_t> _indices;
	std::vector<SubMesh> _submeshes;
};

} // namespace vre
