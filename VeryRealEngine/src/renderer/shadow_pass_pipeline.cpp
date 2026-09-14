#include "../assets/mesh_vertex.hpp"
#include "shadow_pass.hpp"
#include "vk_check.hpp"

namespace vre
{

void ShadowPass::create(const VulkanDevice &device)
{
	_device = device.device();
	create_shadow_resources(device);
	create_shadow_pipeline(_device);
}

void ShadowPass::create_shadow_pipeline(VkDevice device)
{
	VkShaderModule	vertex_module;

	vertex_module = VulkanDevice::load_shader_module(device,
			"shaders/shadow.vert.spv");
	VkPipelineShaderStageCreateInfo vertex_stage{};
	vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertex_stage.module = vertex_module;
	vertex_stage.pName = "main";
	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(MeshVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription position_attribute{};
	position_attribute.binding = 0;
	position_attribute.location = 0;
	position_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attribute.offset = MeshVertex::position_offset();
	VkPipelineVertexInputStateCreateInfo vertex_input{};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 1;
	vertex_input.pVertexAttributeDescriptions = &position_attribute;
	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(kShadowMapResolution);
	viewport.height = static_cast<float>(kShadowMapResolution);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{{0, 0}, {kShadowMapResolution, kShadowMapResolution}};
	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	// Cull front faces (instead of the main pass's back faces) for the
	// shadow pass: a standard trick that biases surviving depth samples
	// toward the lit-side surface, reducing shadow acne without a large bias.
	//
	// frontFace is CLOCKWISE, not the "natural" CCW a right-handed,
	// CCW-authored mesh would suggest: mat4::perspective/orthographic (see
	// vre_math.hpp) negate their Y row to flip into Vulkan's NDC (+Y down),
	// and this pass's viewport isn't flipped back with a negative height —
	// so the same Y-flip that already lands correctly in gl_Position also
	// mirrors screen-space winding, turning every authored-CCW triangle CW
	// by the time the rasterizer sees it. Declaring CLOCKWISE here (and in
	// the main mesh pipeline) compensates for exactly that mirroring.
	rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_TRUE;
	rasterizer.depthBiasConstantFactor = 1.25f;
	rasterizer.depthBiasSlopeFactor = 1.75f;
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo depth_stencil{};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	VkPipelineColorBlendStateCreateInfo color_blending{};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.attachmentCount = 0;
	VkPushConstantRange push_constant_range{};
	push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(ShadowPushConstants);
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push_constant_range;
	VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr,
			&_pipeline_layout));
	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 1;
	pipeline_info.pStages = &vertex_stage;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = &depth_stencil;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.layout = _pipeline_layout;
	pipeline_info.renderPass = _render_pass;
	pipeline_info.subpass = 0;
	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
			&pipeline_info, nullptr, &_pipeline));
	vkDestroyShaderModule(device, vertex_module, nullptr);
}

mat4 ShadowPass::compute_light_space_matrix(const Light &shadow_caster) const
{
	// The engine doesn't compute a dynamic scene-bounds AABB yet (that's
	// future work alongside a general culling system) — this fixed box/
	// distance is sized generously for the demo scene in
	// assets/scenes/demo_scene.json.
	vec3 scene_center(0.0f, 0.5f, 0.0f);
	vec3 up(0.0f, 1.0f, 0.0f);

	if (shadow_caster.type() == Light::Type::Point)
	{
		// A point light's shadow really needs an omnidirectional cubemap
		// (6 faces) to cover every direction; this uses a single
		// perspective frustum aimed at the scene center instead, which is
		// exact for whatever it covers but won't shadow anything outside
		// that cone — an explicit, documented scope limitation.
		//
		// Aiming straight down (the natural default for a ceiling light)
		// was tried and reverted: it views every vertical wall at a
		// near-maximum grazing angle from the shadow camera's perspective,
		// which — even with the normal-offset bias in mesh.vert — still
		// rendered walls as uniformly, incorrectly dark in the two-room
		// house scene. Aiming at a shared point diagonally instead keeps
		// most walls at a shallower, more forgiving angle, which is what
		// actually renders correctly here; this remains scene-shaped
		// (assumes something worth lighting near this coordinate), a
		// known simplification alongside the missing scene-bounds AABB.
		vec3 to_center = vec3::normalize(scene_center
				- shadow_caster.direction_or_position());
		if (std::fabs(vec3::dot(to_center, up)) > 0.99f)
			up = vec3(0.0f, 0.0f, 1.0f);

		mat4 light_view = mat4::look_at(shadow_caster.direction_or_position(),
				scene_center, up);
		mat4 light_projection = mat4::perspective(1.6f /* ~92 degrees */, 1.0f,
				0.1f, 20.0f);
		return (mat4::multiply(light_projection, light_view));
	}

	vec3 light_direction = vec3::normalize(shadow_caster.direction_or_position());
	if (std::fabs(vec3::dot(light_direction, up)) > 0.99f)
		up = vec3(0.0f, 0.0f, 1.0f);
			// avoid a degenerate look_at when the light is near-vertical

	vec3 light_position = scene_center - light_direction * 10.0f;
	mat4 light_view = mat4::look_at(light_position, scene_center, up);
	mat4 light_projection = mat4::orthographic(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f,
			20.0f);
	return (mat4::multiply(light_projection, light_view));
}

void ShadowPass::record(VkCommandBuffer command_buffer, uint32_t caster_index,
	const mat4 &light_space_matrix, const std::vector<RenderItem> &items,
	const MeshRegistry &mesh_registry) const
{
	VkClearValue clear_value{};
	clear_value.depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = _render_pass;
	render_pass_info.framebuffer = _framebuffers[caster_index];
	render_pass_info.renderArea.offset = {0, 0};
	render_pass_info.renderArea.extent = {kShadowMapResolution,
		kShadowMapResolution};
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_value;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info,
		VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		_pipeline);

	for (const auto &item : items)
	{
		ShadowPushConstants push{};
		push.light_mvp = mat4::multiply(light_space_matrix, item.model());

		VkBuffer vertex_buffers[] = {mesh_registry.vertex_buffer(item.mesh())};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(command_buffer,
			mesh_registry.index_buffer(item.mesh()), 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(command_buffer, _pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push);

		size_t submesh_count = mesh_registry.submesh_count(item.mesh());
		for (size_t i = 0; i < submesh_count; i++)
		{
			vkCmdDrawIndexed(command_buffer,
				mesh_registry.submesh_index_count(item.mesh(), i), 1,
				mesh_registry.submesh_index_offset(item.mesh(), i), 0, 0);
		}
	}

	vkCmdEndRenderPass(command_buffer);
}

} // namespace vre
