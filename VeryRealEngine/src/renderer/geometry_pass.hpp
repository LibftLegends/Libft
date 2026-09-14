/**
 * @file geometry_pass.hpp
 * @brief The main lit/textured/shadowed geometry render pass: the
 * intermediate HDR scene-color + depth render targets, the graphics
 * pipeline, the global (per-frame) descriptor set/UBO, and the
 * geometry-draw-loop portion of command buffer recording.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../math/vec3.hpp"
#include "../vre.hpp"
#include "light.hpp"
#include "mesh_registry.hpp"
#include "render_item.hpp"
#include "shadow_pass.hpp"
#include "swap_chain.hpp"
#include "texture_registry.hpp"
#include "vulkan_device.hpp"

namespace vre
{

class GeometryPass
{
  public:
	static constexpr VkFormat kSceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	static constexpr uint32_t kMaxLights = 4;
		///< Upper bound on simultaneously shaded lights.
	/**
		* Upper bound on simultaneously skinned bones (see mesh_data.hpp's
		* MeshVertex doc comment) — slot 0 is always the identity matrix
		* for unweighted static-mesh vertices, so kMaxBones - 1 real bones
		* are actually usable.
		*/
	static constexpr uint32_t kMaxBones = 16;

	GeometryPass();
	~GeometryPass();

	void create(const VulkanDevice &device, const SwapChain &swap_chain,
		const TextureRegistry &texture_registry, const ShadowPass &shadow_pass,
		uint32_t frames_in_flight);
	void destroy();

	/// Recreates the depth/scene-color images and the scene framebuffer (e.g. after a resize).
	void recreate_swapchain_resources(const VulkanDevice &device,
		const SwapChain &swap_chain);
	/// Destroys the depth/scene-color images and the scene framebuffer.
	void destroy_swapchain_resources();

	VkRenderPass render_pass() const;
	VkImageView scene_color_image_view() const;
	VkImageView depth_image_view() const;
	VkPipelineLayout pipeline_layout() const;
	VkDescriptorSet global_descriptor_set(uint32_t frame_index) const;

	/**
	 * @brief Fills and uploads the GlobalUbo for one frame-in-flight slot.
	 * @param frame_index Which frame-in-flight slot's UBO buffer to update.
	 * @param view Camera view matrix.
	 * @param projection Camera projection matrix.
	 * @param light_space_matrices Per-caster light-space matrices, ShadowPass::kMaxShadowCasters entries.
	 * @param shadow_caster_count Number of active entries in light_space_matrices.
	 * @param view_position Camera world-space position.
	 * @param lights Active scene lights.
	 * @param ambient_intensity Flat ambient term added before per-light shading.
	 * @param bone_matrices Slot 0 is always identity; caller passes real bones starting at slot 1.
	 */
	void update_global_ubo(uint32_t frame_index, const mat4 &view,
		const mat4 &projection, const mat4 *light_space_matrices,
		uint32_t shadow_caster_count, const vec3 &view_position,
		const std::vector<Light> &lights, float ambient_intensity,
		const std::vector<mat4> &bone_matrices);

	/// Begins the geometry render pass and binds the pipeline/viewport/scissor/global descriptor set.
	void begin_render_pass(VkCommandBuffer command_buffer, VkExtent2D extent,
		uint32_t frame_index) const;
	/** Draws every item, binding each submesh's material descriptor set and push constants. */
	void draw(VkCommandBuffer command_buffer,
		const std::vector<RenderItem> &draw_items,
		const MeshRegistry &mesh_registry,
		const TextureRegistry &texture_registry) const;
	void end_render_pass(VkCommandBuffer command_buffer) const;

  private:
	// Owns VkImage/VkRenderPass/VkPipeline/VkBuffer[] — the pre-C++11
	// idiom of a private, never-defined copy constructor/assignment
	// operator (this project avoids `= delete`).
	GeometryPass(const GeometryPass &other);
	GeometryPass &operator=(const GeometryPass &other);

	void create_depth_and_color_resources(const VulkanDevice &device,
		const SwapChain &swap_chain);
	void create_render_pass(VkDevice device, VkFormat depth_format);
	void create_global_set_layout(VkDevice device);
	void create_graphics_pipeline(VkDevice device,
		VkDescriptorSetLayout material_set_layout);
	void create_framebuffer(VkDevice device, VkExtent2D extent);
	void create_global_ubo_resources(const VulkanDevice &device,
		const ShadowPass &shadow_pass, uint32_t frames_in_flight);

	/// Pushed per draw call. Pure GPU-layout data, same treatment as
	/// GlobalUbo below, not a fully encapsulated class.
	struct					PushConstants
	{
		mat4				model;
		float				tint[4];
		float material_params[4]; // x = roughness, y = metallic, z/w unused
	};

	/**
		* Mirrors the GlobalUbo block declared in shaders/mesh.vert /
		* mesh.frag — layout and field order must match exactly (std140).
		*/
	struct					GlobalUbo
	{
		mat4				view_proj;
		mat4				light_space_matrices[ShadowPass::kMaxShadowCasters];
		/** xyz + type (0=dir,1=point) */
		float				light_direction_or_position[kMaxLights][4];
		float light_color_intensity[kMaxLights][4];       ///< rgb + intensity
		/** x = light count, y = ambient */
		float				light_count_ambient[4];
		float				view_position[4];
		float shadow_caster_count[4]; ///< x = active shadow casters
		mat4				bone_matrices[kMaxBones];
	};

	VkDevice _device; ///< Non-owning; cached by create() for destroy().
	VkFormat				_depth_format;

	VkImage					_depth_image;
	VkDeviceMemory			_depth_image_memory;
	VkImageView				_depth_image_view;

	VkImage					_scene_color_image;
	VkDeviceMemory			_scene_color_image_memory;
	VkImageView				_scene_color_image_view;

	VkRenderPass			_render_pass;
	VkFramebuffer			_framebuffer;
	VkDescriptorSetLayout	_global_set_layout;
	VkDescriptorPool		_global_descriptor_pool;
	VkPipelineLayout		_pipeline_layout;
	VkPipeline				_pipeline;

	std::vector<VkBuffer> _ubo_buffers;
	std::vector<VkDeviceMemory> _ubo_memories;
	std::vector<void *> _ubo_mapped;
	std::vector<VkDescriptorSet> _global_descriptor_sets;
};

} // namespace vre
