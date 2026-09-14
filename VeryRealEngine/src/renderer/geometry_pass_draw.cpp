#include "geometry_pass.hpp"

namespace vre
{

void GeometryPass::begin_render_pass(VkCommandBuffer command_buffer,
	VkExtent2D extent, uint32_t frame_index) const
{
	VkClearValue clear_values[2];
	clear_values[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
	clear_values[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = _render_pass;
	render_pass_info.framebuffer = _framebuffer;
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

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		_pipeline_layout, 1, 1, &_global_descriptor_sets[frame_index], 0,
		nullptr);
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

			VkDescriptorSet material_set = texture_registry.material_descriptor_set(material);
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
