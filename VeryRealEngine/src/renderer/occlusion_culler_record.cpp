#include "occlusion_culler.hpp"

namespace vre
{

void OcclusionCuller::record(VkCommandBuffer command_buffer,
	uint32_t frame_index, const std::vector<RenderItem> &occlusion_test_items,
	const MeshRegistry &mesh_registry, const TextureRegistry &texture_registry,
	VkPipelineLayout geometry_pipeline_layout,
	VkDescriptorSet global_descriptor_set, std::vector<uint32_t> *out_query_ids)
{
	uint32_t		query_index;
	size_t			submesh_count;
	MaterialHandle	first_material;
	VkDescriptorSet	material_set;

	out_query_ids->clear();
	if (occlusion_test_items.empty())
		return ;
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		_pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		geometry_pipeline_layout, 1, 1, &global_descriptor_set, 0, nullptr);
	query_index = 0;
	for (const auto &item : occlusion_test_items)
	{
		if (query_index >= kMaxOcclusionQueries)
			break ; // documented cap, see kMaxOcclusionQueries
		submesh_count = mesh_registry.submesh_count(item.mesh());
		if (submesh_count == 0)
			continue ; // nothing to test (and nothing material to bind) for an empty mesh
		// Byte-identical to GeometryPass's private PushConstants (this
		// pipeline reuses that pipeline's layout, hence its exact push
		// constant range/size) — content beyond `model` is irrelevant
		// anyway since this pipeline's colorWriteMask is 0.
		struct
		{
			mat4	model;
			float	tint[4];
			float	material_params[4];
		}				push{};
		VkBuffer		vertex_buffers[] = {mesh_registry.vertex_buffer(item.mesh())};
		VkDeviceSize	offsets[] = {0};

		push.model = item.model();
		push.tint[3] = 1.0f; // rest of push is irrelevant: colorWriteMask is 0
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(command_buffer,
			mesh_registry.index_buffer(item.mesh()), 0, VK_INDEX_TYPE_UINT32);
		// Any valid descriptor set 0 works here (the fragment output is
		// discarded either way) — reuse the mesh's own first submesh
		// material rather than adding a dependency on a default material's
		// handle staying valid.
		first_material = mesh_registry.submesh_material(item.mesh(), 0);
		material_set = texture_registry.material_descriptor_set(first_material);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			geometry_pipeline_layout, 0, 1, &material_set, 0, nullptr);
		vkCmdPushConstants(command_buffer, geometry_pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(push), &push);
		vkCmdBeginQuery(command_buffer, _query_pools[frame_index], query_index,
			0);
		for (size_t i = 0; i < submesh_count; i++)
		{
			vkCmdDrawIndexed(command_buffer,
				mesh_registry.submesh_index_count(item.mesh(), i), 1,
				mesh_registry.submesh_index_offset(item.mesh(), i), 0, 0);
		}
		vkCmdEndQuery(command_buffer, _query_pools[frame_index], query_index);
		out_query_ids->push_back(item.occlusion_id());
		query_index++;
	}
	_query_ids[frame_index] = *out_query_ids;
}

} // namespace vre
