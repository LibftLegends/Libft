#include "../assets/mesh_vertex.hpp"
#include "occlusion_culler.hpp"
#include "vk_check.hpp"

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
	VkShaderModule					vertex_module;
	VkShaderModule					fragment_module;
	VkDynamicState					dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
							VK_DYNAMIC_STATE_SCISSOR};

	_device = device.device();
	// Same vertex input, shader stages, pipeline layout, and render pass as
	// the main graphics pipeline (reusing mesh.vert/mesh.frag rather than
	// writing a dedicated depth-only shader pair — the fragment shader's
	// actual output is simply discarded by colorWriteMask below, which
	// costs a little wasted fragment-shading work but avoids a second set
	// of shader files/descriptor bindings for what's already a handful of
	// low-poly objects). Only the depth/color-write and depth-compare
	// state differ: this pipeline must never itself change what's on
	// screen or in the depth buffer, only report whether it *would* have.
	vertex_module = VulkanDevice::load_shader_module(_device,
			"shaders/mesh.vert.spv");
	fragment_module = VulkanDevice::load_shader_module(_device,
			"shaders/mesh.frag.spv");
	VkPipelineShaderStageCreateInfo vertex_stage{};
	vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertex_stage.module = vertex_module;
	vertex_stage.pName = "main";
	VkPipelineShaderStageCreateInfo fragment_stage{};
	fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_stage.module = fragment_module;
	fragment_stage.pName = "main";
	VkPipelineShaderStageCreateInfo stages[] = {vertex_stage, fragment_stage};
	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(MeshVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// Locations 3/4 (bone_indices/bone_weights): every static mesh's
	// vertices default to bone_indices=[0,0,0,0]/weights=[1,0,0,0], and
	// GlobalUbo::bone_matrices[0] is always the identity matrix, so this is
	// a no-op for anything that isn't an actual animated skeleton. Required
	// here too (not just the main graphics pipeline) since this pipeline
	// binds the very same mesh.vert module, whose input interface declares
	// these locations regardless of which pipeline runs it.
	VkVertexInputAttributeDescription attributes[5]{};
	attributes[0].binding = 0;
	attributes[0].location = 0;
	attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributes[0].offset = MeshVertex::position_offset();
	attributes[1].binding = 0;
	attributes[1].location = 1;
	attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributes[1].offset = MeshVertex::normal_offset();
	attributes[2].binding = 0;
	attributes[2].location = 2;
	attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[2].offset = MeshVertex::uv_offset();
	attributes[3].binding = 0;
	attributes[3].location = 3;
	attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributes[3].offset = MeshVertex::bone_indices_offset();
	attributes[4].binding = 0;
	attributes[4].location = 4;
	attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributes[4].offset = MeshVertex::bone_weights_offset();
	VkPipelineVertexInputStateCreateInfo vertex_input{};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 5;
	vertex_input.pVertexAttributeDescriptions = attributes;
	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		// see ShadowPass::create_shadow_pipeline()
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	// The three lines that actually make this an occlusion-test pipeline
	// rather than a second copy of the main one: never write depth (so it
	// can't hide real geometry drawn later, or corrupt what the SSAO pass
	// reads back), and test with <= rather than the main pipeline's <
	// (strict "less") — an item that WAS drawn for real just above has its
	// own depth already in the buffer at exactly this same value (same
	// geometry, same transform), and a strict "<" would fail that
	// self-comparison, marking every visible object "occluded" by itself.
	VkPipelineDepthStencilStateCreateInfo depth_stencil{};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_FALSE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	// colorWriteMask = 0: this pipeline's fragment output must never reach
	// the framebuffer, only the query's sample count.
	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = 0;
	color_blend_attachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo color_blending{};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;
	VkPipelineDynamicStateCreateInfo dynamic_state{};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;
	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = &depth_stencil;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = geometry_pipeline_layout; // reused verbatim, see comment above
	pipeline_info.renderPass = geometry_render_pass;
	pipeline_info.subpass = 0;
	VK_CHECK(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1,
			&pipeline_info, nullptr, &_pipeline));
	vkDestroyShaderModule(_device, fragment_module, nullptr);
	vkDestroyShaderModule(_device, vertex_module, nullptr);
	VkQueryPoolCreateInfo query_pool_info{};
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

} // namespace vre
