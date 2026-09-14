#include "geometryrenderpassfactory.hpp"
#include "vkcheck.hpp"

namespace vre
{
GeometryRenderPassFactory::GeometryRenderPassFactory()
{
}

GeometryRenderPassFactory::GeometryRenderPassFactory(
	const GeometryRenderPassFactory &)
{
}

GeometryRenderPassFactory &GeometryRenderPassFactory::operator=(
	const GeometryRenderPassFactory &)
{
	return (*this);
}

GeometryRenderPassFactory::~GeometryRenderPassFactory()
{
}

VkRenderPass GeometryRenderPassFactory::create_render_pass(VkDevice device,
	VkFormat depth_format, VkFormat scene_color_format)
{
	VkRenderPass	render_pass;

	// Geometry pass: writes the intermediate scene-color target and the
	// depth buffer. Both finalLayouts leave them ready to be *sampled* (as
	// regular textures, not input attachments — the post pass's SSAO needs
	// arbitrary-offset neighbor lookups for its kernel, which Vulkan input
	// attachments cannot do; they only allow reading a fragment's own pixel).
	VkAttachmentDescription scene_color_attachment{};
	scene_color_attachment.format = scene_color_format;
	scene_color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	scene_color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	scene_color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	scene_color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	scene_color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	scene_color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	scene_color_attachment.finalLayout =
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = depth_format;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout =
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentDescription attachments[] = {scene_color_attachment,
		depth_attachment};
	VkAttachmentReference scene_color_ref{0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depth_ref{1,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &scene_color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	// Same shape as the shadow pass's dependencies: don't start writing
	// this frame's color/depth before the post pass finished *sampling*
	// last frame's, and make sure this frame's writes finish before the
	// post pass samples them.
	VkSubpassDependency dependencies[2]{};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkRenderPassCreateInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 2;
	render_pass_info.pAttachments = attachments;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 2;
	render_pass_info.pDependencies = dependencies;
	VK_CHECK(vkCreateRenderPass(device, &render_pass_info, nullptr,
			&render_pass));
	return (render_pass);
}

VkDescriptorSetLayout GeometryRenderPassFactory::create_global_set_layout(
	VkDevice device, uint32_t max_shadow_casters)
{
	VkDescriptorSetLayout	set_layout;

	// Set 1: per-frame globals — the GlobalUbo (view/proj, lights, shadow
	// matrix) and the shadow map itself, shared by every draw call in the
	// frame.
	VkDescriptorSetLayoutBinding ubo_binding{};
	ubo_binding.binding = 0;
	ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_binding.descriptorCount = 1;
	ubo_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
		| VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutBinding shadow_sampler_binding{};
	shadow_sampler_binding.binding = 1;
	shadow_sampler_binding.descriptorType =
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadow_sampler_binding.descriptorCount = max_shadow_casters;
	shadow_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutBinding global_bindings[] = {ubo_binding,
		shadow_sampler_binding};
	VkDescriptorSetLayoutCreateInfo global_layout_info{};
	global_layout_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	global_layout_info.bindingCount = 2;
	global_layout_info.pBindings = global_bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &global_layout_info, nullptr,
			&set_layout));
	return (set_layout);
}

} // namespace vre
