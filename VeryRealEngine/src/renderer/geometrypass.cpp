#include "geometrypass.hpp"
#include "vkcheck.hpp"

namespace vre
{
GeometryPass::GeometryPass() : _device(VK_NULL_HANDLE),
	_render_pass(VK_NULL_HANDLE), _global_set_layout(VK_NULL_HANDLE),
	_pipeline_layout(VK_NULL_HANDLE), _pipeline(VK_NULL_HANDLE)
{
}

GeometryPass::~GeometryPass()
{
}

void GeometryPass::create(const VulkanDevice &device,
	const SwapChain &swap_chain, const TextureRegistry &texture_registry,
	const ShadowPass &shadow_pass, uint32_t frames_in_flight)
{
	_device = device.device();
	_render_pass = GeometryRenderPassFactory::create_render_pass(_device,
			device.find_depth_format(), kSceneColorFormat);
	_global_set_layout = GeometryRenderPassFactory::create_global_set_layout(
			_device, ShadowPass::kMaxShadowCasters);
	GeometryPipelineFactory::create(_device, _render_pass,
		texture_registry.material_set_layout(), _global_set_layout,
		sizeof(PushConstants), &_pipeline_layout, &_pipeline);
	_scene_targets.create(device, swap_chain, _render_pass, kSceneColorFormat);
	_global_ubo.create(device, _global_set_layout, shadow_pass,
		frames_in_flight);
}

void GeometryPass::destroy_swapchain_resources()
{
	_scene_targets.destroy();
}

void GeometryPass::recreate_swapchain_resources(const VulkanDevice &device,
	const SwapChain &swap_chain)
{
	_scene_targets.recreate(device, swap_chain, _render_pass,
		kSceneColorFormat);
}

void GeometryPass::destroy()
{
	_global_ubo.destroy();
	vkDestroyPipeline(_device, _pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(_device, _global_set_layout, nullptr);
	vkDestroyRenderPass(_device, _render_pass, nullptr);
	destroy_swapchain_resources();
}

VkRenderPass GeometryPass::render_pass() const
{
	return (_render_pass);
}

VkImageView GeometryPass::scene_color_image_view() const
{
	return (_scene_targets.scene_color_image_view());
}

VkImageView GeometryPass::depth_image_view() const
{
	return (_scene_targets.depth_image_view());
}

VkPipelineLayout GeometryPass::pipeline_layout() const
{
	return (_pipeline_layout);
}

VkDescriptorSet GeometryPass::global_descriptor_set(uint32_t frame_index)
	const
{
	return (_global_ubo.descriptor_set(frame_index));
}

void GeometryPass::update_global_ubo(uint32_t frame_index, const mat4 &view,
	const mat4 &projection, const mat4 *light_space_matrices,
	uint32_t shadow_caster_count, const vec3 &view_position,
	const std::vector<Light> &lights, float ambient_intensity,
	const std::vector<mat4> &bone_matrices)
{
	_global_ubo.update(frame_index, view, projection, light_space_matrices,
		shadow_caster_count, view_position, lights, ambient_intensity,
		bone_matrices);
}

void GeometryPass::begin_render_pass(VkCommandBuffer command_buffer,
	VkExtent2D extent, uint32_t frame_index) const
{
	VkClearValue clear_values[2];
	clear_values[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
	clear_values[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = _render_pass;
	render_pass_info.framebuffer = _scene_targets.framebuffer();
	render_pass_info.renderArea.offset = {0, 0};
	render_pass_info.renderArea.extent = extent;
	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_values;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info,
		VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		_pipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	VkRect2D scissor{{0, 0}, extent};
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	VkDescriptorSet global_set = _global_ubo.descriptor_set(frame_index);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		_pipeline_layout, 1, 1, &global_set, 0, nullptr);
}

void GeometryPass::draw(VkCommandBuffer command_buffer,
	const std::vector<RenderItem> &draw_items,
	const MeshRegistry &mesh_registry,
	const TextureRegistry &texture_registry) const
{
	for (const auto &item : draw_items)
	{
		PushConstants push{};
		push.model = item.model();

		VkBuffer vertex_buffers[] = {mesh_registry.vertex_buffer(item.mesh())};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(command_buffer,
			mesh_registry.index_buffer(item.mesh()), 0, VK_INDEX_TYPE_UINT32);

		size_t submesh_count = mesh_registry.submesh_count(item.mesh());
		for (size_t i = 0; i < submesh_count; i++)
		{
			MaterialHandle material = mesh_registry.submesh_material(item.mesh(),
					i);
			const float *tint = texture_registry.material_diffuse_tint(material);
			push.tint[0] = tint[0];
			push.tint[1] = tint[1];
			push.tint[2] = tint[2];
			push.tint[3] = 1.0f;
			push.material_params[0] = texture_registry.material_roughness(material);
			push.material_params[1] = texture_registry.material_metallic(material);
			push.material_params[2] = 0.0f;
			push.material_params[3] = 0.0f;

			VkDescriptorSet material_set =
				texture_registry.material_descriptor_set(material);
			vkCmdBindDescriptorSets(command_buffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0, 1,
				&material_set, 0, nullptr);
			vkCmdPushConstants(command_buffer, _pipeline_layout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(PushConstants), &push);

			vkCmdDrawIndexed(command_buffer,
				mesh_registry.submesh_index_count(item.mesh(), i), 1,
				mesh_registry.submesh_index_offset(item.mesh(), i), 0, 0);
		}
	}
}

void GeometryPass::end_render_pass(VkCommandBuffer command_buffer) const
{
	vkCmdEndRenderPass(command_buffer);
}

} // namespace vre
