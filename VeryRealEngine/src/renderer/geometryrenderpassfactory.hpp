/**
 * @file geometryrenderpassfactory.hpp
 * @brief Builds GeometryPass's render pass and global (per-frame,
 * set-1) descriptor set layout — factored out of GeometryPipelineFactory
 * to keep each builder under this project's 250-line cap.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class GeometryRenderPassFactory
{
  public:
	GeometryRenderPassFactory();
	GeometryRenderPassFactory(const GeometryRenderPassFactory &other);
	GeometryRenderPassFactory &operator=(
		const GeometryRenderPassFactory &other);
	~GeometryRenderPassFactory();

	/**
		* @brief Builds the geometry render pass: HDR scene-color + depth
		* attachments, both left sampleable (SHADER_READ_ONLY_OPTIMAL /
		* DEPTH_STENCIL_READ_ONLY_OPTIMAL) for the post-process pass.
		* @param device Device to create the render pass against.
		* @param depth_format Depth attachment format (see
		* VulkanDevice::find_depth_format()).
		* @param scene_color_format Color attachment format (see
		* GeometryPass::kSceneColorFormat).
		* @return The created render pass.
		*/
	static VkRenderPass create_render_pass(VkDevice device,
		VkFormat depth_format, VkFormat scene_color_format);

	/**
		* @brief Builds the set-1 (globals) descriptor set layout: the
		* GlobalUbo (binding 0) and the shadow-map sampler array (binding 1).
		* @param device Device to create the layout against.
		* @param max_shadow_casters Size of the binding-1 sampler array
		* (see ShadowPass::kMaxShadowCasters).
		* @return The created descriptor set layout.
		*/
	static VkDescriptorSetLayout create_global_set_layout(VkDevice device,
		uint32_t max_shadow_casters);
};

} // namespace vre
