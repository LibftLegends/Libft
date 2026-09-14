/**
 * @file occlusion_culler.hpp
 * @brief GPU occlusion queries: a pipeline identical to the main graphics
 * pipeline except color/depth writes are disabled, used only to ask
 * "would this object's own geometry have produced any visible fragment
 * against what's already in the depth buffer." One query pool per
 * frame-in-flight, and persisted per-object visibility state carried
 * across frames (see is_known_occluded()).
 */
#pragma once

#include "../vre.hpp"
#include "mesh_registry.hpp"
#include "render_item.hpp"
#include "texture_registry.hpp"
#include "vulkan_device.hpp"

namespace vre
{

class OcclusionCuller
{
  public:
	/// Hard cap on how many occlusion queries one frame-in-flight's pool can hold.
	static constexpr uint32_t kMaxOcclusionQueries = 256;

	OcclusionCuller();
	~OcclusionCuller();

	/**
	 * @param device Vulkan device to create the pipeline/query pools against.
	 * @param geometry_render_pass Render pass this pipeline is compatible with (the geometry pass's).
	 * @param geometry_pipeline_layout Pipeline layout reused verbatim (see the .cpp's create() comment).
	 * @param frames_in_flight One query pool is created per frame-in-flight slot.
	 */
	void create(const VulkanDevice &device, VkRenderPass geometry_render_pass,
		VkPipelineLayout geometry_pipeline_layout, uint32_t frames_in_flight);
	void destroy();

	/// @return Whether `occlusion_id`'s last known query result said "produced zero visible samples."
	bool is_known_occluded(uint32_t occlusion_id) const;

	/**
		* Reads back the previous recording's query results for this
		* frame-in-flight slot into the persisted visibility state, before
		* that slot's pool is reset and reused for this frame's own
		* queries. Safe to call without a wait: the caller's fence wait,
		* just before this, already proved the GPU finished the work that
		* produced these results.
		*/
	void update_results(uint32_t frame_index);

	/// Must be called outside any render pass instance (Vulkan spec
	/// requirement for vkCmdResetQueryPool), before the geometry render pass begins.
	void reset_query_pool(VkCommandBuffer command_buffer,
		uint32_t frame_index) const;

	/**
	 * @brief Records the occlusion-query pass, inside the geometry
	 * render pass the caller already began (after its real geometry draws).
	 * @param command_buffer Command buffer already inside the geometry render pass.
	 * @param frame_index Which frame-in-flight slot's query pool to record into.
	 * @param occlusion_test_items Items to test (already frustum-visible, drawn-for-real items excluded).
	 * @param mesh_registry Source of vertex/index buffers for the tested items.
	 * @param texture_registry Source of a valid material descriptor set to bind (see the .cpp's comment).
	 * @param geometry_pipeline_layout Pipeline layout reused verbatim from the geometry pass.
	 * @param global_descriptor_set Set 1 (globals), bound the same as the geometry pass.
	 * @param out_query_ids Receives, in the same order queries were
	 * issued, which occlusion_id each one belongs to (see update_results()).
	 */
	void record(VkCommandBuffer command_buffer, uint32_t frame_index,
		const std::vector<RenderItem> &occlusion_test_items,
		const MeshRegistry &mesh_registry,
		const TextureRegistry &texture_registry,
		VkPipelineLayout geometry_pipeline_layout,
		VkDescriptorSet global_descriptor_set,
		std::vector<uint32_t> *out_query_ids);

  private:
	// Owns VkPipeline/VkQueryPool[] — the pre-C++11 idiom of a private,
	// never-defined copy constructor/assignment operator (this project
	// avoids `= delete`).
	OcclusionCuller(const OcclusionCuller &other);
	OcclusionCuller &operator=(const OcclusionCuller &other);

	VkDevice _device; ///< Non-owning; cached by create() for destroy().
	VkPipeline _pipeline;
	std::vector<VkQueryPool> _query_pools;         ///< One per frame-in-flight.
	std::vector<std::vector<uint32_t>> _query_ids;
		///< Per frame-in-flight; see update_results().
	std::vector<uint8_t> _visible;                
		///< Persisted visibility state, indexed by occlusion_id.
};

} // namespace vre
