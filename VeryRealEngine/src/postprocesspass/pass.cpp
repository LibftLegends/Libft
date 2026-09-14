#include "pass.hpp"
#include "../vulkan/check.hpp"

namespace vre
{
PostProcessPass::PostProcessPass() : _device(VK_NULL_HANDLE),
	_render_pass(VK_NULL_HANDLE), _scene_color_sampler(VK_NULL_HANDLE),
	_scene_depth_sampler(VK_NULL_HANDLE), _set_layout(VK_NULL_HANDLE),
	_descriptor_pool(VK_NULL_HANDLE), _descriptor_set(VK_NULL_HANDLE),
	_pipeline_layout(VK_NULL_HANDLE), _pipeline(VK_NULL_HANDLE)
{
}

PostProcessPass::~PostProcessPass()
{
}

void PostProcessPass::create(const VulkanDevice &device,
	const SwapChain &swap_chain, VkImageView scene_color_view,
	VkImageView depth_view)
{
	PostProcessPipelineFactory::Resources resources;

	_device = device.device();
	resources = PostProcessPipelineFactory::create(_device,
			swap_chain.image_format(), sizeof(PostPushConstants));
	_render_pass = resources.render_pass;
	_scene_color_sampler = resources.scene_color_sampler;
	_scene_depth_sampler = resources.scene_depth_sampler;
	_set_layout = resources.set_layout;
	_descriptor_pool = resources.descriptor_pool;
	_descriptor_set = resources.descriptor_set;
	_pipeline_layout = resources.pipeline_layout;
	_pipeline = resources.pipeline;
	create_framebuffers(_device, swap_chain);
	update_descriptor_set(_device, scene_color_view, depth_view);
}

void PostProcessPass::destroy_framebuffers()
{
	for (VkFramebuffer framebuffer : _framebuffers)
		vkDestroyFramebuffer(_device, framebuffer, nullptr);
	_framebuffers.clear();
}

void PostProcessPass::recreate_swapchain_resources(const VulkanDevice &
	/*device*/, const SwapChain &swap_chain, VkImageView scene_color_view,
	VkImageView depth_view)
{
	destroy_framebuffers();
	create_framebuffers(_device, swap_chain);
	update_descriptor_set(_device, scene_color_view, depth_view);
}

void PostProcessPass::update_descriptor_set(VkDevice device,
	VkImageView scene_color_view, VkImageView depth_view)
{
	VkDescriptorImageInfo color_input_info{};
	color_input_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	color_input_info.imageView = scene_color_view;
	color_input_info.sampler = _scene_color_sampler;
	VkDescriptorImageInfo depth_input_info{};
	depth_input_info.imageLayout =
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depth_input_info.imageView = depth_view;
	depth_input_info.sampler = _scene_depth_sampler;
	VkWriteDescriptorSet writes[2]{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = _descriptor_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &color_input_info;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = _descriptor_set;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &depth_input_info;
	vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
}

void PostProcessPass::create_framebuffers(VkDevice device,
	const SwapChain &swap_chain)
{
	VkImageView	attachment;

	_framebuffers.resize(swap_chain.image_count());
	for (size_t i = 0; i < swap_chain.image_count(); i++)
	{
		attachment = swap_chain.image_view(i);
		VkFramebufferCreateInfo framebuffer_info{};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = _render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = &attachment;
		framebuffer_info.width = swap_chain.extent().width;
		framebuffer_info.height = swap_chain.extent().height;
		framebuffer_info.layers = 1;
		VK_CHECK(vkCreateFramebuffer(device, &framebuffer_info, nullptr,
				&_framebuffers[i]));
	}
}

void PostProcessPass::destroy()
{
	destroy_framebuffers();
	vkDestroyPipeline(_device, _pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
	vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(_device, _set_layout, nullptr);
	vkDestroySampler(_device, _scene_color_sampler, nullptr);
	vkDestroySampler(_device, _scene_depth_sampler, nullptr);
	vkDestroyRenderPass(_device, _render_pass, nullptr);
}

void PostProcessPass::record(VkCommandBuffer command_buffer,
	uint32_t image_index, VkExtent2D extent, const mat4 &projection,
	float screen_motion_blur_x, float screen_motion_blur_y) const
{
	// Samples the geometry pass's color+depth (now in
	// SHADER_READ_ONLY_OPTIMAL/DEPTH_STENCIL_READ_ONLY_OPTIMAL per
	// PostProcessPipelineFactory's render pass finalLayouts) as regular
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
	post_push.ao_params[1] = 0.06f;
		// depth-comparison bias, avoids self-occlusion
	post_push.ao_params[2] = 0.75f;
		// occlusion strength (0 = no AO, 1 = full effect)
	post_push.ao_params[3] = 0.0f;
	post_push.bloom_params[0] = 1.0f;
		// bloom threshold: only above-SDR-range color contributes
	post_push.bloom_params[1] = 0.6f; // bloom intensity
	post_push.bloom_params[2] = 2.5f; // sample step, in texels
	post_push.bloom_params[3] = 0.0f;
	post_push.dof_params[0] = 3.0f;
		// focus distance, view-space units — a "look at something a
		// few meters away" default
	post_push.dof_params[1] = 1.5f;
		// focus range: +/- this many units stay fully sharp
	post_push.dof_params[2] = 3.0f;
		// falloff range: blur ramps to max over this many units
	post_push.dof_params[3] = 6.0f;
		// max circle-of-confusion radius, in texels
	post_push.motion_blur_params[0] = screen_motion_blur_x;
	post_push.motion_blur_params[1] = screen_motion_blur_y;
	post_push.motion_blur_params[2] = 0.0f;
	post_push.motion_blur_params[3] = 0.0f;
	vkCmdPushConstants(command_buffer, _pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostPushConstants),
		&post_push);

	vkCmdDraw(command_buffer, 3, 1, 0, 0);
		// fullscreen triangle, generated in post.vert

	vkCmdEndRenderPass(command_buffer);
}

} // namespace vre
