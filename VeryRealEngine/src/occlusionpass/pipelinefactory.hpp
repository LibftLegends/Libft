/**
 * @file pipelinefactory.hpp
 * @brief Builds OcclusionCuller's graphics pipeline — factored out of
 * OcclusionCuller so that class stays focused on owning the query pools
 * and recording queries into a command buffer.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class OcclusionPipelineFactory
{
  public:
	OcclusionPipelineFactory();
	OcclusionPipelineFactory(const OcclusionPipelineFactory &other);
	OcclusionPipelineFactory &operator=(
		const OcclusionPipelineFactory &other);
	~OcclusionPipelineFactory();

	/**
		* @brief Builds a pipeline identical to the main geometry pipeline
		* except color/depth writes are disabled and the depth compare op
		* is <= instead of < (see the .cpp for why) — used only to ask
		* "would this object's own geometry have produced any visible
		* fragment against what's already in the depth buffer."
		* @param device Device to create the pipeline against.
		* @param geometry_render_pass Render pass this pipeline is
		* compatible with (the geometry pass's).
		* @param geometry_pipeline_layout Pipeline layout reused verbatim
		* (see the .cpp for why that's safe).
		* @return The created pipeline.
		*/
	static VkPipeline create(VkDevice device, VkRenderPass geometry_render_pass,
		VkPipelineLayout geometry_pipeline_layout);
};

} // namespace vre
