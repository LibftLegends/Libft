#include "pipelinefactory.hpp"
#include "../mesh/vertex.hpp"
#include "../vulkan/check.hpp"
#include "../vulkan/device.hpp"

namespace vre
{
ShadowPipelineFactory::ShadowPipelineFactory()
{
}

ShadowPipelineFactory::ShadowPipelineFactory(const ShadowPipelineFactory &)
{
}

ShadowPipelineFactory &ShadowPipelineFactory::operator=(
	const ShadowPipelineFactory &)
{
	return (*this);
}

ShadowPipelineFactory::~ShadowPipelineFactory()
{
}

void ShadowPipelineFactory::create(VkDevice device, VkRenderPass render_pass,
	uint32_t resolution, size_t push_constants_size,
	VkPipelineLayout *out_pipeline_layout, VkPipeline *out_pipeline)
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
	vertex_input.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 1;
	vertex_input.pVertexAttributeDescriptions = &position_attribute;
	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(resolution);
	viewport.height = static_cast<float>(resolution);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{{0, 0}, {resolution, resolution}};
	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
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
	multisampling.sType =
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo depth_stencil{};
	depth_stencil.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	VkPipelineColorBlendStateCreateInfo color_blending{};
	color_blending.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.attachmentCount = 0;
	VkPushConstantRange push_constant_range{};
	push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_constant_range.offset = 0;
	push_constant_range.size = static_cast<uint32_t>(push_constants_size);
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push_constant_range;
	VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr,
			out_pipeline_layout));
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
	pipeline_info.layout = *out_pipeline_layout;
	pipeline_info.renderPass = render_pass;
	pipeline_info.subpass = 0;
	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
			&pipeline_info, nullptr, out_pipeline));
	vkDestroyShaderModule(device, vertex_module, nullptr);
}

} // namespace vre
