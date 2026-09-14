/**
 * @file mesh_registry.hpp
 * @brief Owns every GPU-resident mesh: device-local vertex/index buffers,
 * per-material submesh ranges, and cached local-space bounds. Loads OBJ and
 * skinned assets, uploading their materials through a TextureRegistry.
 */
#pragma once

#include "../animation/animationclip.hpp"
#include "../animation/skeleton.hpp"
#include "../assets/meshdata.hpp"
#include "../math/aabb.hpp"
#include "../vre.hpp"
#include "material_handle.hpp"
#include "mesh_handle.hpp"
#include "texture_registry.hpp"
#include "vulkan_device.hpp"

namespace vre
{

class MeshRegistry
{
  public:
	MeshRegistry();
	~MeshRegistry();

	/// @param device Used for buffer uploads; must outlive this object.
	/// @param texture_registry Used to create per-submesh materials during upload; must outlive this object.
	void create(const VulkanDevice &device, TextureRegistry *texture_registry);
	void destroy();

	/// Loads a mesh from an OBJ file, caching by path. Falls back to a
	/// placeholder cube (logging to stderr) if the file is missing or malformed.
	MeshHandle load_mesh_from_obj(const char *path);
	/// Loads a skinned mesh/skeleton/animation from a custom asset file.
	/// Falls back to a placeholder cube (with an empty skeleton/clip) on failure.
	MeshHandle load_skinned_mesh(const char *path, Skeleton *out_skeleton,
		AnimationClip *out_clip);

	VkBuffer vertex_buffer(MeshHandle handle) const;
	VkBuffer index_buffer(MeshHandle handle) const;
	size_t submesh_count(MeshHandle handle) const;
	uint32_t submesh_index_offset(MeshHandle handle,
		size_t submesh_index) const;
	uint32_t submesh_index_count(MeshHandle handle, size_t submesh_index) const;
	MaterialHandle submesh_material(MeshHandle handle,
		size_t submesh_index) const;
	const AABB &local_bounds(MeshHandle handle) const;

  private:
	// Owns VkBuffer/VkDeviceMemory arrays — the pre-C++11 idiom of a
	// private, never-defined copy constructor/assignment operator
	// (this project avoids `= delete`).
	MeshRegistry(const MeshRegistry &other);
	MeshRegistry &operator=(const MeshRegistry &other);

	static void build_fallback_cube_mesh(MeshData *out_mesh);

	/// Shared tail end of load_mesh_from_obj()/load_skinned_mesh():
	/// uploads already-parsed CPU-side mesh/material data to GPU-resident
	/// buffers and registers the materials.
	MeshHandle upload_mesh_data(const MeshData &mesh_data,
		const std::vector<MaterialData> &material_data);

	/// One contiguous index range of a GpuMesh sharing a single material.
	/// Pure GPU-layout data, manipulated only by MeshRegistry itself —
	/// same treatment as Renderer::GlobalUbo, not a fully encapsulated class.
	struct				GpuSubMesh
	{
		uint32_t		index_offset;
		uint32_t		index_count;
		MaterialHandle	material;
	};

	/// A GPU-resident mesh: device-local vertex/index buffers, its
	/// per-material submesh ranges, and a cached object-local bounding box.
	struct				GpuMesh
	{
		VkBuffer		vertex_buffer = VK_NULL_HANDLE;
		VkDeviceMemory	vertex_buffer_memory = VK_NULL_HANDLE;
		VkBuffer		index_buffer = VK_NULL_HANDLE;
		VkDeviceMemory	index_buffer_memory = VK_NULL_HANDLE;
		std::vector<GpuSubMesh> submeshes;
		AABB			local_bounds;
	};

	const VulkanDevice *_device;        ///< Non-owning; set by create().
	TextureRegistry *_texture_registry; ///< Non-owning; set by create().

	std::vector<GpuMesh> _meshes;
	std::map<std::string, MeshHandle> _mesh_cache;
};

} // namespace vre
