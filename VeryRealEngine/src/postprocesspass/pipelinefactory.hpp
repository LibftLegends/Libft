/**
 * @file pipelinefactory.hpp
 * @brief Builds every one-time Vulkan object PostProcessPass needs (render
 * pass, samplers, descriptor set layout/pool/set, pipeline) — factored out
 * of that class so it stays focused on the framebuffers (resized often)
 * and recording the pass itself.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class PostProcessPipelineFactory
{
  public:
	/// Everything PostProcessPipelineFactory::create() builds, handed
	/// back to PostProcessPass to own.
	struct					Resources
	{
		VkRenderPass		render_pass = VK_NULL_HANDLE;
		VkSampler			scene_color_sampler = VK_NULL_HANDLE;
		VkSampler			scene_depth_sampler = VK_NULL_HANDLE;
		VkDescriptorSetLayout	set_layout = VK_NULL_HANDLE;
		VkDescriptorPool	descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorSet		descriptor_set = VK_NULL_HANDLE;
		VkPipelineLayout	pipeline_layout = VK_NULL_HANDLE;
		VkPipeline			pipeline = VK_NULL_HANDLE;
	};

	PostProcessPipelineFactory();
	PostProcessPipelineFactory(const PostProcessPipelineFactory &other);
	PostProcessPipelineFactory &operator=(
		const PostProcessPipelineFactory &other);
	~PostProcessPipelineFactory();

	/**
		* @brief Builds the render pass, plain (non-comparison)
		* color/depth input samplers, the 2-binding descriptor set
		* layout/pool/set, and the fullscreen-triangle pipeline.
		* @param device Device to create every object against.
		* @param swapchain_image_format Format of the presented image
		* (the render pass's only attachment).
		* @param push_constants_size sizeof(PostProcessPass's
		* fragment-stage push-constant struct).
		* @return The created objects, ready for PostProcessPass to own.
		*/
	static Resources create(VkDevice device,
		VkFormat swapchain_image_format, size_t push_constants_size);
};

} // namespace vre
