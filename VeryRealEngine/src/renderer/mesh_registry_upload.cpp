#include "../assets/meshvertex.hpp"
#include "mesh_registry.hpp"
#include "vk_check.hpp"

namespace vre
{

MeshHandle MeshRegistry::upload_mesh_data(const MeshData &mesh_data,
	const std::vector<MaterialData> &material_data)
{
	GpuMesh	mesh;
		GpuSubMesh gpu_submesh;

	std::vector<MaterialHandle> local_to_global_material(material_data.size());
	for (size_t i = 0; i < material_data.size(); i++)
		local_to_global_material[i] = _texture_registry->create_material(material_data[i]);
	_device->upload_to_device_local_buffer(mesh_data.vertices().data(),
		sizeof(MeshVertex) * mesh_data.vertices().size(),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &mesh.vertex_buffer,
		&mesh.vertex_buffer_memory);
	_device->upload_to_device_local_buffer(mesh_data.indices().data(),
		sizeof(uint32_t) * mesh_data.indices().size(),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &mesh.index_buffer,
		&mesh.index_buffer_memory);
	if (!mesh_data.vertices().empty())
	{
		const MeshVertex &first = mesh_data.vertices()[0];
		vec3 first_position(first.position(0), first.position(1),
			first.position(2));
		mesh.local_bounds = AABB(first_position, first_position);
		for (const MeshVertex &vertex : mesh_data.vertices())
		{
			vec3 position(vertex.position(0), vertex.position(1),
				vertex.position(2));
			mesh.local_bounds.encapsulate(position);
		}
	}
	for (const auto &submesh : mesh_data.submeshes())
	{
		gpu_submesh.index_offset = submesh.index_offset();
		gpu_submesh.index_count = submesh.index_count();
		gpu_submesh.material = (submesh.material_index() >= 0) ? local_to_global_material[submesh.material_index()] : _texture_registry->default_material();
		mesh.submeshes.push_back(gpu_submesh);
	}
	MeshHandle handle(_meshes.size());
	_meshes.push_back(mesh);
	return (handle);
}

VkBuffer MeshRegistry::vertex_buffer(MeshHandle handle) const
{
	return (_meshes[handle.value()].vertex_buffer);
}

VkBuffer MeshRegistry::index_buffer(MeshHandle handle) const
{
	return (_meshes[handle.value()].index_buffer);
}

size_t MeshRegistry::submesh_count(MeshHandle handle) const
{
	return (_meshes[handle.value()].submeshes.size());
}

uint32_t MeshRegistry::submesh_index_offset(MeshHandle handle,
	size_t submesh_index) const
{
	return (_meshes[handle.value()].submeshes[submesh_index].index_offset);
}

uint32_t MeshRegistry::submesh_index_count(MeshHandle handle,
	size_t submesh_index) const
{
	return (_meshes[handle.value()].submeshes[submesh_index].index_count);
}

MaterialHandle MeshRegistry::submesh_material(MeshHandle handle,
	size_t submesh_index) const
{
	return (_meshes[handle.value()].submeshes[submesh_index].material);
}

const AABB &MeshRegistry::local_bounds(MeshHandle handle) const
{
	return (_meshes[handle.value()].local_bounds);
}

} // namespace vre
