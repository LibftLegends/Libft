/**
 * @file pass.hpp
 * @brief Post-process composite pass: a fullscreen-triangle fragment
 * shader that samples the geometry pass's color+depth as regular textures
 * (SSAO/bloom/tonemap/DOF/motion-blur) and writes the result to the
 * swapchain image actually presented.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "pipelinefactory.hpp"
#include "../vulkan/swapchain.hpp"
#include "../vulkan/device.hpp"

namespace vre
{
class PostProcessPass
{
  public:
	PostProcessPass();
	~PostProcessPass();

	/// Creates the render pass, samplers, descriptor set layout/pool/set,
	/// pipeline, and per-swapchain-image framebuffers.
	void create(const VulkanDevice &device, const SwapChain &swap_chain,
		VkImageView scene_color_view, VkImageView depth_view);
	void destroy();

	/// Destroys the per-swapchain-image framebuffers (e.g. before a resize).
	void destroy_framebuffers();
	/// Recreates the per-swapchain-image framebuffers and repoints the
	/// descriptor set at the (also just-recreated) color/depth views.
	void recreate_swapchain_resources(const VulkanDevice &device,
		const SwapChain &swap_chain, VkImageView scene_color_view,
		VkImageView depth_view);

	/// @brief Records the post-process pass: samples the geometry pass's
	/// outputs and writes the composited result to swapchain image
	/// `image_index`.
	void record(VkCommandBuffer command_buffer, uint32_t image_index,
		VkExtent2D extent, const mat4 &projection, float screen_motion_blur_x,
		float screen_motion_blur_y) const;

  private:
	// Owns VkRenderPass/VkFramebuffer[]/VkPipeline/VkSampler — the
	// pre-C++11 idiom of a private, never-defined copy
	// constructor/assignment operator (this project avoids `= delete`).
	PostProcessPass(const PostProcessPass &other);
	PostProcessPass &operator=(const PostProcessPass &other);

	void create_framebuffers(VkDevice device, const SwapChain &swap_chain);
	void update_descriptor_set(VkDevice device, VkImageView scene_color_view,
		VkImageView depth_view);

	/**
		* Pushed once for the post-process (SSAO composite) fullscreen
		* triangle — see shaders/post.frag. Rather than a full
		* inverse-projection matrix, proj_params carries just the 4
		* nonzero entries of our perspective projection (see
		* mat4::perspective) that let the shader analytically
		* reconstruct/reproject view-space position from depth — cheaper
		* than a matrix multiply and keeps this comfortably under the
		* guaranteed minimum 128-byte push-constant budget. Assumes the
		* camera projection is always perspective (true for every camera
		* this engine currently builds). Pure GPU-layout data, same
		* treatment as GeometryPass::GlobalUbo, not a fully encapsulated
		* class.
		*/
	struct					PostPushConstants
	{
		float proj_params[4];
			// x = proj.m(0), y = proj.m(5), z = proj.m(10), w = proj.m(14)
		float ao_params[4];
			// x = radius, y = bias, z = strength, w = unused
		float bloom_params[4];
			// x = threshold, y = intensity, z = sample step (texels), w = unused
		float dof_params[4];
			// x = focus distance, y = focus range, z = falloff range,
			// w = max CoC (texels)
		float motion_blur_params[4];
			// x = screen-space velocity.x, y = velocity.y, z/w = unused
	};

	VkDevice _device; ///< Non-owning; cached by create() for destroy().

	VkRenderPass			_render_pass;
	std::vector<VkFramebuffer> _framebuffers;
	VkSampler				_scene_color_sampler;
	VkSampler				_scene_depth_sampler;
	VkDescriptorSetLayout	_set_layout;
	VkDescriptorPool		_descriptor_pool;
	VkDescriptorSet			_descriptor_set;
	VkPipelineLayout		_pipeline_layout;
	VkPipeline				_pipeline;
};

} // namespace vre
