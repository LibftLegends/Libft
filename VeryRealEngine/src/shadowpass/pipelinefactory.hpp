/**
 * @file pipelinefactory.hpp
 * @brief Builds ShadowPass's depth-only graphics pipeline — factored out
 * of ShadowPass so that class stays focused on owning the per-caster
 * image/framebuffer resources and recording shadow draws.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class ShadowPipelineFactory
{
  public:
	ShadowPipelineFactory();
	ShadowPipelineFactory(const ShadowPipelineFactory &other);
	ShadowPipelineFactory &operator=(const ShadowPipelineFactory &other);
	~ShadowPipelineFactory();

	/**
		* @brief Builds the depth-only shadow pipeline (position-only
		* vertex input, front-face culling + depth bias to reduce acne, no
		* color attachment) and its push-constant-only pipeline layout.
		* @param device Device to create the pipeline against.
		* @param render_pass ShadowPass's depth-only render pass.
		* @param resolution Shadow map width/height, in texels (see
		* ShadowPass::kShadowMapResolution).
		* @param push_constants_size sizeof(ShadowPass's push-constant struct).
		* @param out_pipeline_layout Receives the created pipeline layout.
		* @param out_pipeline Receives the created pipeline.
		*/
	static void create(VkDevice device, VkRenderPass render_pass,
		uint32_t resolution, size_t push_constants_size,
		VkPipelineLayout *out_pipeline_layout, VkPipeline *out_pipeline);
};

} // namespace vre
