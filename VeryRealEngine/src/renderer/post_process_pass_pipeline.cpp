#include "post_process_pass.hpp"
#include "vk_check.hpp"

namespace vre
{

void PostProcessPass::create_render_pass(VkDevice device,
	VkFormat swapchain_image_format)
{
	// A second, independent render pass: single color attachment (the
	// actual swapchain image), one subpass. Its fragment shader samples
	// the geometry pass's color/depth as regular textures instead.
	VkAttachmentDescription present_attachment{};
	present_attachment.format = swapchain_image_format;
	present_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	present_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// fullscreen triangle overwrites everything
	present_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	present_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	present_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	present_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	present_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference present_ref{0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkSubpassDescription present_subpass{};
	present_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	present_subpass.colorAttachmentCount = 1;
	present_subpass.pColorAttachments = &present_ref;
	VkSubpassDependency present_dependency{};
	present_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	present_dependency.dstSubpass = 0;
	present_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	present_dependency.srcAccessMask = 0;
	present_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	present_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	VkRenderPassCreateInfo present_render_pass_info{};
	present_render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	present_render_pass_info.attachmentCount = 1;
	present_render_pass_info.pAttachments = &present_attachment;
	present_render_pass_info.subpassCount = 1;
	present_render_pass_info.pSubpasses = &present_subpass;
	present_render_pass_info.dependencyCount = 1;
	present_render_pass_info.pDependencies = &present_dependency;
	VK_CHECK(vkCreateRenderPass(device, &present_render_pass_info, nullptr,
			&_render_pass));
}

void PostProcessPass::create_samplers(VkDevice device)
{
	// Plain (non-comparison) samplers for reading the geometry pass's
	// outputs as ordinary textures. Depth uses NEAREST: filtering depth
	// values makes no physical sense (unlike the shadow map's compare
	// sampler, which filters pass/fail *results*, not raw depths).
	VkSamplerCreateInfo color_sampler_info{};
	color_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	color_sampler_info.magFilter = VK_FILTER_LINEAR;
	color_sampler_info.minFilter = VK_FILTER_LINEAR;
	color_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	color_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	color_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	color_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	VK_CHECK(vkCreateSampler(device, &color_sampler_info, nullptr,
			&_scene_color_sampler));
	VkSamplerCreateInfo depth_sampler_info{};
	depth_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	depth_sampler_info.magFilter = VK_FILTER_NEAREST;
	depth_sampler_info.minFilter = VK_FILTER_NEAREST;
	depth_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_sampler_info.compareEnable = VK_FALSE;
	depth_sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	VK_CHECK(vkCreateSampler(device, &depth_sampler_info, nullptr,
			&_scene_depth_sampler));
}

void PostProcessPass::create_descriptor_set(VkDevice device)
{
	VkDescriptorSetLayoutBinding color_sampler_binding{};
	color_sampler_binding.binding = 0;
	color_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	color_sampler_binding.descriptorCount = 1;
	color_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutBinding depth_sampler_binding{};
	depth_sampler_binding.binding = 1;
	depth_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	depth_sampler_binding.descriptorCount = 1;
	depth_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutBinding	post_bindings[] = {color_sampler_binding,
			depth_sampler_binding};
	VkDescriptorSetLayoutCreateInfo post_layout_info{};
	post_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	post_layout_info.bindingCount = 2;
	post_layout_info.pBindings = post_bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &post_layout_info, nullptr,
			&_set_layout));
	VkDescriptorPoolSize post_pool_size{};
	post_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	post_pool_size.descriptorCount = 2;
	VkDescriptorPoolCreateInfo post_pool_info{};
	post_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	post_pool_info.poolSizeCount = 1;
	post_pool_info.pPoolSizes = &post_pool_size;
	post_pool_info.maxSets = 1;
	VK_CHECK(vkCreateDescriptorPool(device, &post_pool_info, nullptr,
			&_descriptor_pool));
	VkDescriptorSetAllocateInfo post_alloc_info{};
	post_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	post_alloc_info.descriptorPool = _descriptor_pool;
	post_alloc_info.descriptorSetCount = 1;
	post_alloc_info.pSetLayouts = &_set_layout;
	VK_CHECK(vkAllocateDescriptorSets(device, &post_alloc_info,
			&_descriptor_set));
}

void PostProcessPass::create_pipeline(VkDevice device)
{
	VkShaderModule					vertex_module;
	VkShaderModule					fragment_module;
	VkDynamicState					post_dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
							VK_DYNAMIC_STATE_SCISSOR};

	vertex_module = VulkanDevice::load_shader_module(device,
			"shaders/post.vert.spv");
	fragment_module = VulkanDevice::load_shader_module(device,
			"shaders/post.frag.spv");
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
	VkPipelineShaderStageCreateInfo	post_stages[] = {vertex_stage,
			fragment_stage};
	// No vertex buffer: shaders/post.vert generates a fullscreen triangle
	// from gl_VertexIndex alone.
	VkPipelineVertexInputStateCreateInfo post_vertex_input{};
	post_vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VkPipelineInputAssemblyStateCreateInfo post_input_assembly{};
	post_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	post_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo post_viewport_state{};
	post_viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	post_viewport_state.viewportCount = 1;
	post_viewport_state.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo post_rasterizer{};
	post_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	post_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	post_rasterizer.lineWidth = 1.0f;
	post_rasterizer.cullMode = VK_CULL_MODE_NONE;
	VkPipelineMultisampleStateCreateInfo post_multisampling{};
	post_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	post_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo post_depth_stencil{};
		// no depth attachment in this subpass
	VkPipelineColorBlendAttachmentState post_color_blend_attachment{};
	post_color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	post_color_blend_attachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo post_color_blending{};
	post_color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	post_color_blending.attachmentCount = 1;
	post_color_blending.pAttachments = &post_color_blend_attachment;
	VkPipelineDynamicStateCreateInfo post_dynamic_state{};
	post_dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	post_dynamic_state.dynamicStateCount = 2;
	post_dynamic_state.pDynamicStates = post_dynamic_states;
	VkPushConstantRange post_push_constant_range{};
	post_push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	post_push_constant_range.offset = 0;
	post_push_constant_range.size = sizeof(PostPushConstants);
	VkPipelineLayoutCreateInfo post_layout_create_info{};
	post_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	post_layout_create_info.setLayoutCount = 1;
	post_layout_create_info.pSetLayouts = &_set_layout;
	post_layout_create_info.pushConstantRangeCount = 1;
	post_layout_create_info.pPushConstantRanges = &post_push_constant_range;
	VK_CHECK(vkCreatePipelineLayout(device, &post_layout_create_info, nullptr,
			&_pipeline_layout));
	VkGraphicsPipelineCreateInfo post_pipeline_info{};
	post_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	post_pipeline_info.stageCount = 2;
	post_pipeline_info.pStages = post_stages;
	post_pipeline_info.pVertexInputState = &post_vertex_input;
	post_pipeline_info.pInputAssemblyState = &post_input_assembly;
	post_pipeline_info.pViewportState = &post_viewport_state;
	post_pipeline_info.pRasterizationState = &post_rasterizer;
	post_pipeline_info.pMultisampleState = &post_multisampling;
	post_pipeline_info.pDepthStencilState = &post_depth_stencil;
	post_pipeline_info.pColorBlendState = &post_color_blending;
	post_pipeline_info.pDynamicState = &post_dynamic_state;
	post_pipeline_info.layout = _pipeline_layout;
	post_pipeline_info.renderPass = _render_pass;
	post_pipeline_info.subpass = 0;
	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
			&post_pipeline_info, nullptr, &_pipeline));
	vkDestroyShaderModule(device, fragment_module, nullptr);
	vkDestroyShaderModule(device, vertex_module, nullptr);
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

} // namespace vre
