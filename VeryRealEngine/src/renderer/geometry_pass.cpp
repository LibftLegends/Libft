#include "geometry_pass.hpp"
#include "vk_check.hpp"

namespace vre
{

GeometryPass::GeometryPass() : _device(VK_NULL_HANDLE),
	_depth_format(VK_FORMAT_UNDEFINED), _depth_image(VK_NULL_HANDLE),
	_depth_image_memory(VK_NULL_HANDLE), _depth_image_view(VK_NULL_HANDLE),
	_scene_color_image(VK_NULL_HANDLE),
	_scene_color_image_memory(VK_NULL_HANDLE),
	_scene_color_image_view(VK_NULL_HANDLE), _render_pass(VK_NULL_HANDLE),
	_framebuffer(VK_NULL_HANDLE), _global_set_layout(VK_NULL_HANDLE),
	_global_descriptor_pool(VK_NULL_HANDLE), _pipeline_layout(VK_NULL_HANDLE),
	_pipeline(VK_NULL_HANDLE)
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
	create_depth_and_color_resources(device, swap_chain);
	create_render_pass(_device, _depth_format);
	create_global_set_layout(_device);
	create_graphics_pipeline(_device, texture_registry.material_set_layout());
	create_framebuffer(_device, swap_chain.extent());
	create_global_ubo_resources(device, shadow_pass, frames_in_flight);
}

void GeometryPass::create_depth_and_color_resources(const VulkanDevice &device,
	const SwapChain &swap_chain)
{
	VkDevice				vk_device;
	VkExtent2D				extent;
	VkMemoryRequirements	memory_requirements;
	VkMemoryRequirements	color_memory_requirements;

	vk_device = device.device();
	extent = swap_chain.extent();
	_depth_format = device.find_depth_format();
	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = extent.width;
	image_info.extent.height = extent.height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = _depth_format;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// SAMPLED_BIT: the post-process pass samples this back (as a regular
	// texture, with arbitrary-offset lookups for SSAO's kernel) to
	// reconstruct view-space position.
	image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(vk_device, &image_info, nullptr, &_depth_image));
	vkGetImageMemoryRequirements(vk_device, _depth_image, &memory_requirements);
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = device.find_memory_type(memory_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(vk_device, &alloc_info, nullptr,
			&_depth_image_memory));
	vkBindImageMemory(vk_device, _depth_image, _depth_image_memory, 0);
	_depth_image_view = VulkanDevice::create_image_view(vk_device, _depth_image,
			_depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
	// The geometry pass's actual render target (not the swapchain image
	// directly) — R16G16B16A16_SFLOAT leaves HDR headroom for a future
	// tonemap pass, and gives the post-process pass a color texture to
	// sample alongside the depth buffer above.
	VkImageCreateInfo color_image_info{};
	color_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	color_image_info.imageType = VK_IMAGE_TYPE_2D;
	color_image_info.extent.width = extent.width;
	color_image_info.extent.height = extent.height;
	color_image_info.extent.depth = 1;
	color_image_info.mipLevels = 1;
	color_image_info.arrayLayers = 1;
	color_image_info.format = kSceneColorFormat;
	color_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	color_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	color_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	color_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(vk_device, &color_image_info, nullptr,
			&_scene_color_image));
	vkGetImageMemoryRequirements(vk_device, _scene_color_image,
		&color_memory_requirements);
	VkMemoryAllocateInfo color_alloc_info{};
	color_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	color_alloc_info.allocationSize = color_memory_requirements.size;
	color_alloc_info.memoryTypeIndex = device.find_memory_type(color_memory_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(vk_device, &color_alloc_info, nullptr,
			&_scene_color_image_memory));
	vkBindImageMemory(vk_device, _scene_color_image, _scene_color_image_memory,
		0);
	_scene_color_image_view = VulkanDevice::create_image_view(vk_device,
			_scene_color_image, kSceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

void GeometryPass::create_framebuffer(VkDevice device, VkExtent2D extent)
{
	VkImageView	attachments[] = {_scene_color_image_view, _depth_image_view};

	// A single framebuffer, unlike the swapchain-image-indexed post-process
	// ones — the geometry pass always renders into the same scene
	// color/depth images regardless of which swapchain image will
	// eventually be presented.
	VkFramebufferCreateInfo framebuffer_info{};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = _render_pass;
	framebuffer_info.attachmentCount = 2;
	framebuffer_info.pAttachments = attachments;
	framebuffer_info.width = extent.width;
	framebuffer_info.height = extent.height;
	framebuffer_info.layers = 1;
	VK_CHECK(vkCreateFramebuffer(device, &framebuffer_info, nullptr,
			&_framebuffer));
}

void GeometryPass::destroy_swapchain_resources()
{
	if (_framebuffer != VK_NULL_HANDLE)
		vkDestroyFramebuffer(_device, _framebuffer, nullptr);
	_framebuffer = VK_NULL_HANDLE;
	vkDestroyImageView(_device, _depth_image_view, nullptr);
	vkDestroyImage(_device, _depth_image, nullptr);
	vkFreeMemory(_device, _depth_image_memory, nullptr);
	vkDestroyImageView(_device, _scene_color_image_view, nullptr);
	vkDestroyImage(_device, _scene_color_image, nullptr);
	vkFreeMemory(_device, _scene_color_image_memory, nullptr);
}

void GeometryPass::recreate_swapchain_resources(const VulkanDevice &device,
	const SwapChain &swap_chain)
{
	destroy_swapchain_resources();
	create_depth_and_color_resources(device, swap_chain);
	create_framebuffer(_device, swap_chain.extent());
}

void GeometryPass::destroy()
{
	for (size_t i = 0; i < _ubo_buffers.size(); i++)
	{
		vkUnmapMemory(_device, _ubo_memories[i]);
		vkDestroyBuffer(_device, _ubo_buffers[i], nullptr);
		vkFreeMemory(_device, _ubo_memories[i], nullptr);
	}
	_ubo_buffers.clear();
	_ubo_memories.clear();
	_ubo_mapped.clear();
	vkDestroyDescriptorPool(_device, _global_descriptor_pool, nullptr);
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
	return (_scene_color_image_view);
}

VkImageView GeometryPass::depth_image_view() const
{
	return (_depth_image_view);
}

VkPipelineLayout GeometryPass::pipeline_layout() const
{
	return (_pipeline_layout);
}

VkDescriptorSet GeometryPass::global_descriptor_set(uint32_t frame_index) const
{
	return (_global_descriptor_sets[frame_index]);
}

} // namespace vre
