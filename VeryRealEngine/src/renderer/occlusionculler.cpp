#include "occlusionculler.hpp"
#include "../vulkan/check.hpp"

namespace vre
{
OcclusionCuller::OcclusionCuller() : _device(VK_NULL_HANDLE),
	_pipeline(VK_NULL_HANDLE)
{
}

OcclusionCuller::~OcclusionCuller()
{
}

void OcclusionCuller::create(const VulkanDevice &device,
	VkRenderPass geometry_render_pass,
	VkPipelineLayout geometry_pipeline_layout, uint32_t frames_in_flight)
{
	VkQueryPoolCreateInfo query_pool_info{};

	_device = device.device();
	_pipeline = OcclusionPipelineFactory::create(_device, geometry_render_pass,
			geometry_pipeline_layout);
	query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	query_pool_info.queryType = VK_QUERY_TYPE_OCCLUSION;
	query_pool_info.queryCount = kMaxOcclusionQueries;
	_query_pools.resize(frames_in_flight);
	_query_ids.resize(frames_in_flight);
	for (uint32_t i = 0; i < frames_in_flight; i++)
		VK_CHECK(vkCreateQueryPool(_device, &query_pool_info, nullptr,
				&_query_pools[i]));
}

void OcclusionCuller::destroy()
{
	for (VkQueryPool pool : _query_pools)
		vkDestroyQueryPool(_device, pool, nullptr);
	_query_pools.clear();
	vkDestroyPipeline(_device, _pipeline, nullptr);
	_pipeline = VK_NULL_HANDLE;
}

bool OcclusionCuller::is_known_occluded(uint32_t occlusion_id) const
{
	return (occlusion_id < _visible.size() && _visible[occlusion_id] == 0);
}

void OcclusionCuller::update_results(uint32_t frame_index)
{
	uint32_t	id;

	std::vector<uint32_t> &ids = _query_ids[frame_index];
	if (ids.empty())
		return ;
	std::vector<uint64_t> sample_counts(ids.size());
	// VK_QUERY_RESULT_WAIT_BIT costs nothing extra here in practice: the
	// caller's fence wait, just before this is called, already proved the
	// GPU finished the command buffer that recorded these exact queries
	// (same frame-in-flight slot) — this can only return immediately,
	// never actually block.
	vkGetQueryPoolResults(_device, _query_pools[frame_index], 0,
		static_cast<uint32_t>(ids.size()), sample_counts.size()
		* sizeof(uint64_t), sample_counts.data(), sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	for (size_t i = 0; i < ids.size(); i++)
	{
		id = ids[i];
		if (id >= _visible.size())
			_visible.resize(id + 1, 1); // new ids default to visible
		_visible[id] = (sample_counts[i] > 0) ? 1 : 0;
	}
}

void OcclusionCuller::reset_query_pool(VkCommandBuffer command_buffer,
	uint32_t frame_index) const
{
	// Resets the whole pool unconditionally rather than tracking exactly
	// how many queries the previous recording into this frame-in-flight
	// slot used.
	vkCmdResetQueryPool(command_buffer, _query_pools[frame_index], 0,
		kMaxOcclusionQueries);
}

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
			continue ;
				// nothing to test (and nothing material to bind) for an empty mesh
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
