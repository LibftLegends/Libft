#include "post_process_pass.hpp"

namespace vre
{

void PostProcessPass::record(VkCommandBuffer command_buffer,
	uint32_t image_index, VkExtent2D extent, const mat4 &projection,
	float screen_motion_blur_x, float screen_motion_blur_y) const
{
	// Samples the geometry pass's color+depth (now in
	// SHADER_READ_ONLY_OPTIMAL/DEPTH_STENCIL_READ_ONLY_OPTIMAL per
	// GeometryPass::create_render_pass()'s finalLayouts) as regular
	// textures, computes screen-space ambient occlusion from the depth
	// buffer, and writes the AO-modulated color to the actual swapchain image.
	VkClearValue clear_value{};
	clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
		// unused: loadOp is DONT_CARE

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = _render_pass;
	render_pass_info.framebuffer = _framebuffers[image_index];
	render_pass_info.renderArea.offset = {0, 0};
	render_pass_info.renderArea.extent = extent;
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_value;

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
		_pipeline_layout, 0, 1, &_descriptor_set, 0, nullptr);

	PostPushConstants post_push{};
	post_push.proj_params[0] = projection.m(0);
	post_push.proj_params[1] = projection.m(5);
	post_push.proj_params[2] = projection.m(10);
	post_push.proj_params[3] = projection.m(14);
	post_push.ao_params[0] = 0.18f; // AO sample radius, view-space units
	post_push.ao_params[1] = 0.06f; // depth-comparison bias, avoids self-occlusion
	post_push.ao_params[2] = 0.75f; // occlusion strength (0 = no AO, 1 = full effect)
	post_push.ao_params[3] = 0.0f;
	post_push.bloom_params[0] = 1.0f; // bloom threshold: only above-SDR-range color contributes
	post_push.bloom_params[1] = 0.6f; // bloom intensity
	post_push.bloom_params[2] = 2.5f; // sample step, in texels
	post_push.bloom_params[3] = 0.0f;
	post_push.dof_params[0] = 3.0f; // focus distance, view-space units — a "look at something a few meters away" default
	post_push.dof_params[1] = 1.5f; // focus range: +/- this many units stay fully sharp
	post_push.dof_params[2] = 3.0f; // falloff range: blur ramps to max over this many units
	post_push.dof_params[3] = 6.0f; // max circle-of-confusion radius, in texels
	post_push.motion_blur_params[0] = screen_motion_blur_x;
	post_push.motion_blur_params[1] = screen_motion_blur_y;
	post_push.motion_blur_params[2] = 0.0f;
	post_push.motion_blur_params[3] = 0.0f;
	vkCmdPushConstants(command_buffer, _pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostPushConstants), &post_push);

	vkCmdDraw(command_buffer, 3, 1, 0, 0); // fullscreen triangle, generated in post.vert

	vkCmdEndRenderPass(command_buffer);
}

} // namespace vre
