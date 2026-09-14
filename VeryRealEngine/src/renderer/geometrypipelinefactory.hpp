/**
 * @file geometrypipelinefactory.hpp
 * @brief Builds GeometryPass's main lit/textured/shadowed graphics
 * pipeline — factored out of GeometryPass so that class stays focused on
 * owning the color/depth targets, the global UBO, and recording draws.
 * See GeometryRenderPassFactory for the render pass/set layout this
 * pipeline is built against.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class GeometryPipelineFactory
{
  public:
	GeometryPipelineFactory();
	GeometryPipelineFactory(const GeometryPipelineFactory &other);
	GeometryPipelineFactory &operator=(const GeometryPipelineFactory &other);
	~GeometryPipelineFactory();

	/**
		* @brief Builds the main graphics pipeline (skinned vertex input,
		* back-face culling, depth test+write) and its pipeline layout
		* (material set 0 + globals set 1 + one push-constant range).
		* @param device Device to create the pipeline against.
		* @param render_pass Render pass this pipeline is compatible with
		* (see GeometryRenderPassFactory::create_render_pass()).
		* @param material_set_layout Set 0 (per-material), supplied by
		* TextureRegistry.
		* @param global_set_layout Set 1 (per-frame globals), see
		* GeometryRenderPassFactory::create_global_set_layout().
		* @param push_constants_size sizeof(GeometryPass's per-draw
		* push-constant struct).
		* @param out_pipeline_layout Receives the created pipeline layout.
		* @param out_pipeline Receives the created pipeline.
		*/
	static void create(VkDevice device, VkRenderPass render_pass,
		VkDescriptorSetLayout material_set_layout,
		VkDescriptorSetLayout global_set_layout, size_t push_constants_size,
		VkPipelineLayout *out_pipeline_layout, VkPipeline *out_pipeline);
};

} // namespace vre
