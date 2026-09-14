#include "post_process_pass.hpp"
#include "vk_check.hpp"

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
	_device = device.device();
	create_render_pass(_device, swap_chain.image_format());
	create_samplers(_device);
	create_descriptor_set(_device);
	create_pipeline(_device);
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
	depth_input_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
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

} // namespace vre
