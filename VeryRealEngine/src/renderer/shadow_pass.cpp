#include "shadow_pass.hpp"
#include "vk_check.hpp"

namespace vre
{

ShadowPass::ShadowPass() : _device(VK_NULL_HANDLE),
	_depth_format(VK_FORMAT_UNDEFINED), _render_pass(VK_NULL_HANDLE),
	_pipeline_layout(VK_NULL_HANDLE), _pipeline(VK_NULL_HANDLE),
	_sampler(VK_NULL_HANDLE)
{
	for (uint32_t i = 0; i < kMaxShadowCasters; i++)
	{
		_images[i] = VK_NULL_HANDLE;
		_image_memories[i] = VK_NULL_HANDLE;
		_image_views[i] = VK_NULL_HANDLE;
		_framebuffers[i] = VK_NULL_HANDLE;
	}
}

ShadowPass::~ShadowPass()
{
}

void ShadowPass::create_shadow_resources(const VulkanDevice &device)
{
	VkDevice	vk_device;
		VkMemoryRequirements memory_requirements;

	vk_device = device.device();
	_depth_format = device.find_depth_format();
	for (uint32_t i = 0; i < kMaxShadowCasters; i++)
	{
		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent.width = kShadowMapResolution;
		image_info.extent.height = kShadowMapResolution;
		image_info.extent.depth = 1;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.format = _depth_format;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK(vkCreateImage(vk_device, &image_info, nullptr, &_images[i]));
		vkGetImageMemoryRequirements(vk_device, _images[i],
			&memory_requirements);
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memory_requirements.size;
		alloc_info.memoryTypeIndex = device.find_memory_type(memory_requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK(vkAllocateMemory(vk_device, &alloc_info, nullptr,
				&_image_memories[i]));
		vkBindImageMemory(vk_device, _images[i], _image_memories[i], 0);
		_image_views[i] = VulkanDevice::create_image_view(vk_device, _images[i],
				_depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
	// Depth-only render pass: no color attachment. finalLayout leaves the
	// image ready to be sampled by the main pass's fragment shader —
	// re-rendered (and re-transitioned) fresh every frame, which is what
	// makes shadows track both static and moving geometry correctly.
	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = _depth_format;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkAttachmentReference depth_ref{};
	depth_ref.attachment = 0;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depth_ref;
	VkSubpassDependency dependencies[2]{};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	VkRenderPassCreateInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &depth_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 2;
	render_pass_info.pDependencies = dependencies;
	VK_CHECK(vkCreateRenderPass(vk_device, &render_pass_info, nullptr,
			&_render_pass));
	for (uint32_t i = 0; i < kMaxShadowCasters; i++)
	{
		VkFramebufferCreateInfo framebuffer_info{};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = _render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = &_image_views[i];
		framebuffer_info.width = kShadowMapResolution;
		framebuffer_info.height = kShadowMapResolution;
		framebuffer_info.layers = 1;
		VK_CHECK(vkCreateFramebuffer(vk_device, &framebuffer_info, nullptr,
				&_framebuffers[i]));
	}
	// A hardware depth-compare sampler: LINEAR filtering over a comparison
	// (not the raw depth value) gets bilinearly-filtered PCF essentially
	// for free — combined with the 3x3 manual tap in mesh.frag's pcf(),
	// that's what turns the shadow edges soft instead of a single
	// hard-edged sample. CLAMP_TO_BORDER(white, i.e. depth 1.0) means a
	// fragment outside the light's frustum always compares as "closer than
	// the shadow map" and reads back as fully lit, not an artificial edge.
	VkSamplerCreateInfo sampler_info{};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler_info.compareEnable = VK_TRUE;
	sampler_info.compareOp = VK_COMPARE_OP_LESS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	VK_CHECK(vkCreateSampler(vk_device, &sampler_info, nullptr, &_sampler));
}

void ShadowPass::destroy()
{
	for (uint32_t i = 0; i < kMaxShadowCasters; i++)
	{
		vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
		vkDestroyImageView(_device, _image_views[i], nullptr);
		vkDestroyImage(_device, _images[i], nullptr);
		vkFreeMemory(_device, _image_memories[i], nullptr);
	}
	vkDestroySampler(_device, _sampler, nullptr);
	vkDestroyPipeline(_device, _pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
	vkDestroyRenderPass(_device, _render_pass, nullptr);
}

VkImageView ShadowPass::shadow_image_view(uint32_t caster_index) const
{
	return (_image_views[caster_index]);
}

VkSampler ShadowPass::sampler() const
{
	return (_sampler);
}

} // namespace vre
