/**
 * @file shadow_pass.hpp
 * @brief Depth-only shadow map render pass/pipeline, shared by every
 * shadow-casting light, plus the per-caster image/view/framebuffer
 * resources and light-space matrix math.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "light.hpp"
#include "mesh_registry.hpp"
#include "render_item.hpp"
#include "vulkan_device.hpp"

namespace vre
{

class ShadowPass
{
  public:
	static constexpr uint32_t kShadowMapResolution = 2048;
		///< Width/height of each shadow map, in texels.
	/**
		* At most this many scene lights get a real-time shadow map (the
		* rest still light the scene, just without casting shadows) — a
		* fixed, small cap: each caster is a full extra depth-only render
		* pass per frame, and the demo scenes only ever need one or two.
		*/
	static constexpr uint32_t kMaxShadowCasters = 2;

	ShadowPass();
	~ShadowPass();

	/// Creates the shadow render pass, per-caster image/view/framebuffer
	/// resources, sampler, and depth-only pipeline.
	void create(const VulkanDevice &device);
	void destroy();

	VkImageView shadow_image_view(uint32_t caster_index) const;
	VkSampler sampler() const;

	/**
		* @brief Computes a shadow caster's light-space (view*projection) matrix.
		* See the original doc comment on Renderer::compute_light_space_matrix
		* (now here) for the directional-vs-point and scene-bounds caveats.
		*/
	mat4 compute_light_space_matrix(const Light &shadow_caster) const;

	/**
	 * @brief Records one shadow-caster's depth-only render pass.
	 * @param command_buffer Command buffer to record into.
	 * @param caster_index Which shadow map (and framebuffer) to render into.
	 * @param light_space_matrix This caster's light-space (view*projection) matrix.
	 * @param items Full, unculled render item list (a caster outside the
	 * camera frustum can still need to cast a shadow into it).
	 * @param mesh_registry Source of vertex/index buffers for the drawn items.
	 */
	void record(VkCommandBuffer command_buffer, uint32_t caster_index,
		const mat4 &light_space_matrix, const std::vector<RenderItem> &items,
		const MeshRegistry &mesh_registry) const;

  private:
	// Owns VkImage/VkImageView/VkFramebuffer arrays and the shadow
	// pipeline — the pre-C++11 idiom of a private, never-defined copy
	// constructor/assignment operator (this project avoids `= delete`).
	ShadowPass(const ShadowPass &other);
	ShadowPass &operator=(const ShadowPass &other);

	void create_shadow_resources(const VulkanDevice &device);
	void create_shadow_pipeline(VkDevice device);

	/// Pushed per draw call in shadow.vert. Pure GPU-layout data, same
	/// treatment as Renderer::GlobalUbo, not a fully encapsulated class.
	struct				ShadowPushConstants
	{
		mat4			light_mvp;
	};

	VkDevice _device; ///< Non-owning; cached by create() for destroy().

	VkFormat			_depth_format;
	VkRenderPass		_render_pass;
	VkPipelineLayout	_pipeline_layout;
	VkPipeline			_pipeline;
	VkImage				_images[kMaxShadowCasters];
	VkDeviceMemory		_image_memories[kMaxShadowCasters];
	VkImageView			_image_views[kMaxShadowCasters];
	VkFramebuffer		_framebuffers[kMaxShadowCasters];
	VkSampler			_sampler;
};

} // namespace vre
