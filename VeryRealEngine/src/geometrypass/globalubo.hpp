/**
 * @file globalubo.hpp
 * @brief The geometry pass's set-1 (per-frame globals) UBO buffers and
 * descriptor sets — factored out of GeometryPass so that class stays
 * focused on the render targets/pipeline and recording draws.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../math/vec3.hpp"
#include "../vre.hpp"
#include "../renderer/light.hpp"
#include "../shadowpass/pass.hpp"
#include "../vulkan/device.hpp"

namespace vre
{
class GeometryGlobalUbo
{
  public:
	static constexpr uint32_t kMaxLights = 4;
		///< Upper bound on simultaneously shaded lights.
	/**
		* Upper bound on simultaneously skinned bones (see meshdata.hpp's
		* MeshVertex doc comment) — slot 0 is always the identity matrix
		* for unweighted static-mesh vertices, so kMaxBones - 1 real bones
		* are actually usable.
		*/
	static constexpr uint32_t kMaxBones = 16;

	GeometryGlobalUbo();
	~GeometryGlobalUbo();

	/// @return sizeof the GlobalUbo block (for pipeline-layout-independent
	/// sizing, e.g. buffer allocation).
	static VkDeviceSize ubo_size();

	/// Creates one UBO buffer + descriptor set per frame-in-flight slot,
	/// bound to `global_set_layout` (set 1) and `shadow_pass`'s map/sampler.
	void create(const VulkanDevice &device,
		VkDescriptorSetLayout global_set_layout, const ShadowPass &shadow_pass,
		uint32_t frames_in_flight);
	void destroy();

	VkDescriptorSet descriptor_set(uint32_t frame_index) const;

	/**
		* @brief Fills and uploads the GlobalUbo for one frame-in-flight slot.
		* @param frame_index Which frame-in-flight slot's UBO buffer to update.
		* @param view Camera view matrix.
		* @param projection Camera projection matrix.
		* @param light_space_matrices Per-caster light-space matrices,
		* ShadowPass::kMaxShadowCasters entries.
		* @param shadow_caster_count Number of active entries in
		* light_space_matrices.
		* @param view_position Camera world-space position.
		* @param lights Active scene lights.
		* @param ambient_intensity Flat ambient term added before per-light shading.
		* @param bone_matrices Slot 0 is always identity; caller passes
		* real bones starting at slot 1.
		*/
	void update(uint32_t frame_index, const mat4 &view,
		const mat4 &projection, const mat4 *light_space_matrices,
		uint32_t shadow_caster_count, const vec3 &view_position,
		const std::vector<Light> &lights, float ambient_intensity,
		const std::vector<mat4> &bone_matrices);

  private:
	// Owns VkBuffer[]/VkDescriptorPool — the pre-C++11 idiom of a
	// private, never-defined copy constructor/assignment operator (this
	// project avoids `= delete`).
	GeometryGlobalUbo(const GeometryGlobalUbo &other);
	GeometryGlobalUbo &operator=(const GeometryGlobalUbo &other);

	/**
		* Mirrors the GlobalUbo block declared in shaders/mesh.vert /
		* mesh.frag — layout and field order must match exactly (std140).
		*/
	struct				GlobalUbo
	{
		mat4			view_proj;
		mat4			light_space_matrices[ShadowPass::kMaxShadowCasters];
		/// xyz + type (0=dir,1=point)
		float			light_direction_or_position[kMaxLights][4];
		float light_color_intensity[kMaxLights][4]; ///< rgb + intensity
		/// x = light count, y = ambient
		float			light_count_ambient[4];
		float			view_position[4];
		float shadow_caster_count[4]; ///< x = active shadow casters
		mat4			bone_matrices[kMaxBones];
	};

	VkDevice _device; ///< Non-owning; cached by create() for destroy().
	VkDescriptorPool _descriptor_pool;

	std::vector<VkBuffer> _ubo_buffers;
	std::vector<VkDeviceMemory> _ubo_memories;
	std::vector<void *> _ubo_mapped;
	std::vector<VkDescriptorSet> _descriptor_sets;
};

} // namespace vre
