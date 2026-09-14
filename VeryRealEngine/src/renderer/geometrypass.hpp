/**
 * @file geometrypass.hpp
 * @brief The main lit/textured/shadowed geometry render pass: owns the
 * pipeline (via GeometryRenderPassFactory/GeometryPipelineFactory), the
 * render targets (via GeometrySceneTargets), the global UBO (via
 * GeometryGlobalUbo), and the geometry-draw-loop portion of command
 * buffer recording.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../math/vec3.hpp"
#include "../vre.hpp"
#include "geometryglobalubo.hpp"
#include "geometrypipelinefactory.hpp"
#include "geometryrenderpassfactory.hpp"
#include "geometryscenetargets.hpp"
#include "light.hpp"
#include "meshregistry.hpp"
#include "renderitem.hpp"
#include "shadowpass.hpp"
#include "swapchain.hpp"
#include "textureregistry.hpp"
#include "vulkandevice.hpp"

namespace vre
{
class GeometryPass
{
  public:
	static constexpr VkFormat kSceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	GeometryPass();
	~GeometryPass();

	void create(const VulkanDevice &device, const SwapChain &swap_chain,
		const TextureRegistry &texture_registry, const ShadowPass &shadow_pass,
		uint32_t frames_in_flight);
	void destroy();

	/// Recreates the depth/scene-color images and the scene framebuffer
	/// (e.g. after a resize).
	void recreate_swapchain_resources(const VulkanDevice &device,
		const SwapChain &swap_chain);
	/// Destroys the depth/scene-color images and the scene framebuffer.
	void destroy_swapchain_resources();

	VkRenderPass render_pass() const;
	VkImageView scene_color_image_view() const;
	VkImageView depth_image_view() const;
	VkPipelineLayout pipeline_layout() const;
	VkDescriptorSet global_descriptor_set(uint32_t frame_index) const;

	/// Fills and uploads the GlobalUbo for one frame-in-flight slot —
	/// see GeometryGlobalUbo::update() for the parameters.
	void update_global_ubo(uint32_t frame_index, const mat4 &view,
		const mat4 &projection, const mat4 *light_space_matrices,
		uint32_t shadow_caster_count, const vec3 &view_position,
		const std::vector<Light> &lights, float ambient_intensity,
		const std::vector<mat4> &bone_matrices);

	/// Begins the geometry render pass and binds the
	/// pipeline/viewport/scissor/global descriptor set.
	void begin_render_pass(VkCommandBuffer command_buffer, VkExtent2D extent,
		uint32_t frame_index) const;
	/// Draws every item, binding each submesh's material descriptor set
	/// and push constants.
	void draw(VkCommandBuffer command_buffer,
		const std::vector<RenderItem> &draw_items,
		const MeshRegistry &mesh_registry,
		const TextureRegistry &texture_registry) const;
	void end_render_pass(VkCommandBuffer command_buffer) const;

  private:
	// Owns VkRenderPass/VkPipeline and its GeometrySceneTargets/
	// GeometryGlobalUbo members — the pre-C++11 idiom of a private,
	// never-defined copy constructor/assignment operator (this project
	// avoids `= delete`).
	GeometryPass(const GeometryPass &other);
	GeometryPass &operator=(const GeometryPass &other);

	/// Pushed per draw call. Pure GPU-layout data, same treatment as
	/// GeometryGlobalUbo's GlobalUbo, not a fully encapsulated class.
	struct					PushConstants
	{
		mat4				model;
		float				tint[4];
		float material_params[4]; // x = roughness, y = metallic, z/w unused
	};

	VkDevice _device; ///< Non-owning; cached by create() for destroy().

	VkRenderPass			_render_pass;
	VkDescriptorSetLayout	_global_set_layout;
	VkPipelineLayout		_pipeline_layout;
	VkPipeline				_pipeline;

	GeometrySceneTargets	_scene_targets;
	GeometryGlobalUbo		_global_ubo;
};

} // namespace vre
