/**
 * @file renderer.hpp
 * @brief Facade over the Vulkan renderer subsystems: instance/device/
 * swapchain setup, shadow/geometry/post-process passes, frustum+occlusion
 * culling, and the mesh/material/texture registries. Owns per-frame
 * orchestration (draw_frame()'s sequencing) directly; the command
 * buffers/semaphores/fences behind it, and everything else, are each
 * delegated to one dedicated class per concern.
 */
#pragma once

#include "../animation/animationclip.hpp"
#include "../animation/skeleton.hpp"
#include "../math/mat4.hpp"
#include "../math/vec3.hpp"
#include "../platform/window.hpp"
#include "../vre.hpp"
#include "framestats.hpp"
#include "frustumculler.hpp"
#include "geometrypass.hpp"
#include "light.hpp"
#include "meshhandle.hpp"
#include "meshregistry.hpp"
#include "occlusionculler.hpp"
#include "postprocesspass.hpp"
#include "renderitem.hpp"
#include "rendererframesync.hpp"
#include "screenshotcapture.hpp"
#include "shadowcasterselector.hpp"
#include "shadowpass.hpp"
#include "swapchain.hpp"
#include "textureregistry.hpp"
#include "vulkandevice.hpp"
#include "vulkaninstance.hpp"

namespace vre
{
class Renderer
{
  public:
	static constexpr uint32_t kMaxFramesInFlight = 2;
		///< Frames pipelined concurrently (double-buffering).

	Renderer();
	~Renderer();

	/**
		* @brief Brings up the entire Vulkan stack against the given window:
		* instance, surface, device, swapchain, render passes, pipelines,
		* and per-frame sync objects.
		* @param window Platform window to render into; must outlive the Renderer.
		* @return (true on success); false if any setup step failed.
		*/
	bool initialize(Window *window);
	/// Tears down every Vulkan object this Renderer owns, in reverse
	/// dependency order.
	void destroy();
	/// Blocks until the device has finished all submitted work — call
	/// before destroy().
	void wait_idle();

	/// Loads an .obj (and any .mtl/.tga it references) and uploads it to
	/// GPU-resident buffers, caching by path.
	MeshHandle load_mesh_from_obj(const char *path);
	/// Loads a skinned rig (see skinnedmeshloader.hpp) and uploads its
	/// geometry, ready for GPU linear-blend skinning via draw_frame()'s
	/// `bone_matrices` parameter.
	MeshHandle load_skinned_mesh(const char *path, Skeleton *out_skeleton,
		AnimationClip *out_clip);

	/**
		* @brief Renders one frame: shadow passes for the active casters,
		* the geometry pass (with frustum + occlusion culling applied to
		* `items`), and the post-process composite to the swapchain.
		* @param view Camera view matrix.
		* @param projection Camera projection matrix.
		* @param view_position Camera world-space position (used for specular/AO).
		* @param lights Active scene lights.
		* @param ambient_intensity Flat ambient term added before per-light shading.
		* @param items Candidate draw list for this frame, before culling.
		* @param screen_motion_blur_x Approximate screen-space camera-pan
		* velocity, horizontal (UV units/frame) for the post-process
		* motion-blur bonus effect.
		* @param screen_motion_blur_y Same, vertical component.
		* @param bone_matrices Current skinning matrices; element `i` lands
		* in slot `i + 1` (slot 0 is always identity). Capped at
		* GeometryGlobalUbo::kMaxBones - 1 entries.
		*/
	void draw_frame(const mat4 &view, const mat4 &projection,
		const vec3 &view_position, const std::vector<Light> &lights,
		float ambient_intensity, const std::vector<RenderItem> &items,
		float screen_motion_blur_x = 0.0f, float screen_motion_blur_y = 0.0f,
		const std::vector<mat4> &bone_matrices = {});

	/// @return Culling/draw counters from the most recently drawn frame.
	const FrameStats &get_last_frame_stats() const;

	/**
		* @brief Debug/verification aid: writes the most recently presented
		* swapchain image out as a binary PPM file.
		* @param path Output file path (should end in ".ppm").
		* @return true on success.
		*/
	bool capture_screenshot(const char *path);

  private:
	// Owns every Vulkan subsystem below — the pre-C++11 idiom of a
	// private, never-defined copy constructor/assignment operator
	// (this project avoids `= delete`).
	Renderer(const Renderer &other);
	Renderer &operator=(const Renderer &other);

	/// Recreates the swapchain and everything sized from it (e.g. after a resize).
	void recreate_swapchain();

	/**
		* @brief Records the shadow passes, the geometry pass (draw +
		* occlusion-query sub-passes), and the post-process composite pass.
		*/
	void record_command_buffer(VkCommandBuffer command_buffer,
		uint32_t image_index, const mat4 &projection,
		const std::vector<RenderItem> &draw_items,
		const std::vector<RenderItem> &occlusion_test_items,
		std::vector<uint32_t> *out_query_ids, float screen_motion_blur_x,
		float screen_motion_blur_y);

	Window *_window;
		///< Non-owning pointer to the platform window passed to initialize().

	VulkanInstance _vulkan_instance;
	VulkanDevice _vulkan_device;
	SwapChain _swap_chain;
	ShadowPass _shadow_pass;
	TextureRegistry _texture_registry;
	MeshRegistry _mesh_registry;
	GeometryPass _geometry_pass;
	FrustumCuller _frustum_culler;
	OcclusionCuller _occlusion_culler;
	PostProcessPass _post_process_pass;
	ScreenshotCapture _screenshot_capture;
	RendererFrameSync _frame_sync;

	FrameStats _last_frame_stats;
		///< Backing storage for get_last_frame_stats().
	uint32_t _last_presented_image_index;
		///< Which swapchain image capture_screenshot() reads back.
};

} // namespace vre
