#include "renderer.hpp"
#include "vkcheck.hpp"

namespace vre
{
Renderer::Renderer() : _window(nullptr), _last_presented_image_index(0)
{
}

Renderer::~Renderer()
{
	destroy();
}

bool Renderer::initialize(Window *window)
{
	_window = window;
	_vulkan_instance.create(window);
	_vulkan_device.create(_vulkan_instance.instance(),
		_vulkan_instance.surface(), _vulkan_instance.validation_enabled());
	_swap_chain.create(_vulkan_device.physical_device(),
		_vulkan_device.device(), _vulkan_instance.surface(), window,
		_vulkan_device.graphics_queue_family(),
		_vulkan_device.present_queue_family());
	_swap_chain.create_image_views(_vulkan_device.device());
	_shadow_pass.create(_vulkan_device);
	_texture_registry.create(_vulkan_device);
	_mesh_registry.create(_vulkan_device, &_texture_registry);
	_geometry_pass.create(_vulkan_device, _swap_chain, _texture_registry,
		_shadow_pass, kMaxFramesInFlight);
	_occlusion_culler.create(_vulkan_device, _geometry_pass.render_pass(),
		_geometry_pass.pipeline_layout(), kMaxFramesInFlight);
	_post_process_pass.create(_vulkan_device, _swap_chain,
		_geometry_pass.scene_color_image_view(),
		_geometry_pass.depth_image_view());
	_frame_sync.create(_vulkan_device, kMaxFramesInFlight);
	return (true);
}

void Renderer::wait_idle()
{
	if (_vulkan_device.device() != VK_NULL_HANDLE)
		vkDeviceWaitIdle(_vulkan_device.device());
}

void Renderer::destroy()
{
	if (_vulkan_device.device() == VK_NULL_HANDLE)
		return ;
	wait_idle();
	_frame_sync.destroy(_vulkan_device);
	_mesh_registry.destroy();
	_texture_registry.destroy();
	_post_process_pass.destroy();
	_occlusion_culler.destroy();
	_geometry_pass.destroy();
	_shadow_pass.destroy();
	_swap_chain.destroy(_vulkan_device.device());
	_vulkan_device.destroy();
	_vulkan_instance.destroy();
}

void Renderer::recreate_swapchain()
{
	// Extent 0 (minimized window) — wait until the window reports real
	// size again.
	while (_window->get_width() == 0 || _window->get_height() == 0)
		_window->poll_events();
	wait_idle();
	_post_process_pass.destroy_framebuffers();
	_geometry_pass.destroy_swapchain_resources();
	_swap_chain.destroy(_vulkan_device.device());
	_swap_chain.create(_vulkan_device.physical_device(),
		_vulkan_device.device(), _vulkan_instance.surface(), _window,
		_vulkan_device.graphics_queue_family(),
		_vulkan_device.present_queue_family());
	_swap_chain.create_image_views(_vulkan_device.device());
	_geometry_pass.recreate_swapchain_resources(_vulkan_device, _swap_chain);
	_post_process_pass.recreate_swapchain_resources(_vulkan_device, _swap_chain,
		_geometry_pass.scene_color_image_view(),
		_geometry_pass.depth_image_view());
}

MeshHandle Renderer::load_mesh_from_obj(const char *path)
{
	return (_mesh_registry.load_mesh_from_obj(path));
}

MeshHandle Renderer::load_skinned_mesh(const char *path,
	Skeleton *out_skeleton, AnimationClip *out_clip)
{
	return (_mesh_registry.load_skinned_mesh(path, out_skeleton, out_clip));
}

const FrameStats &Renderer::get_last_frame_stats() const
{
	return (_last_frame_stats);
}

bool Renderer::capture_screenshot(const char *path)
{
	VkImage		image;
	VkExtent2D	extent;

	// One-off debug operation, not a per-frame one — a full stall here
	// (rather than the careful per-frame-in-flight synchronization
	// draw_frame() uses) is the simplest way to guarantee the presented
	// image is actually done presenting before this reads it back.
	wait_idle();
	image = _swap_chain.image(_last_presented_image_index);
	extent = _swap_chain.extent();
	return (_screenshot_capture.capture(_vulkan_device, image,
			_swap_chain.image_format(), extent.width, extent.height, path));
}

void Renderer::record_command_buffer(VkCommandBuffer command_buffer,
	uint32_t image_index, const mat4 &projection,
	const std::vector<RenderItem> &draw_items,
	const std::vector<RenderItem> &occlusion_test_items,
	std::vector<uint32_t> *out_query_ids, float screen_motion_blur_x,
	float screen_motion_blur_y)
{
	VkExtent2D	extent;
	uint32_t	frame_index;

	frame_index = _frame_sync.current_frame();
	// Must happen outside any render pass instance (Vulkan spec
	// requirement for vkCmdResetQueryPool).
	_occlusion_culler.reset_query_pool(command_buffer, frame_index);
	extent = _swap_chain.extent();
	_geometry_pass.begin_render_pass(command_buffer, extent, frame_index);
	_geometry_pass.draw(command_buffer, draw_items, _mesh_registry,
		_texture_registry);
	// Occlusion pass: for every frustum-visible item with a stable
	// occlusion_id (see RenderItem), redraw it once more — now with
	// OcclusionCuller's own pipeline (no color/depth writes) — wrapped in a
	// query against the depth buffer the draw above just finished writing.
	// The result is read back two frames from now (OcclusionCuller::
	// update_results()) to decide whether *that* future frame draws this
	// object for real at all.
	_occlusion_culler.record(command_buffer, frame_index,
		occlusion_test_items, _mesh_registry, _texture_registry,
		_geometry_pass.pipeline_layout(),
		_geometry_pass.global_descriptor_set(frame_index), out_query_ids);
	_geometry_pass.end_render_pass(command_buffer);
	_post_process_pass.record(command_buffer, image_index, extent, projection,
		screen_motion_blur_x, screen_motion_blur_y);
}

void Renderer::draw_frame(const mat4 &view, const mat4 &projection,
	const vec3 &view_position, const std::vector<Light> &lights,
	float ambient_intensity, const std::vector<RenderItem> &items,
	float screen_motion_blur_x, float screen_motion_blur_y,
	const std::vector<mat4> &bone_matrices)
{
	uint32_t		image_index;
	VkResult		result;
	uint32_t		frame_index;
	uint32_t		shadow_caster_count;
	mat4			light_space_matrices[ShadowPass::kMaxShadowCasters];
	VkCommandBuffer	command_buffer;

	_frame_sync.wait_for_fence(_vulkan_device);
	frame_index = _frame_sync.current_frame();
	// Must run before record_command_buffer() resets this frame-in-flight
	// slot's query pool, and after the fence wait above guarantees the
	// results are actually ready.
	_occlusion_culler.update_results(frame_index);
	image_index = 0;
	result = _frame_sync.acquire_next_image(_vulkan_device,
			_swap_chain.swapchain(), &image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreate_swapchain();
		return ;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		std::fprintf(stderr, "Renderer: vkAcquireNextImageKHR failed (%d)\n",
			static_cast<int>(result));
		std::abort();
	}
	_frame_sync.reset_fence(_vulkan_device);
	shadow_caster_count = ShadowCasterSelector::select(_shadow_pass, lights,
			light_space_matrices);
	_geometry_pass.update_global_ubo(frame_index, view, projection,
		light_space_matrices, shadow_caster_count, view_position, lights,
		ambient_intensity, bone_matrices);
	command_buffer = _frame_sync.begin_command_buffer();
	for (uint32_t i = 0; i < shadow_caster_count; i++)
		_shadow_pass.record(command_buffer, i, light_space_matrices[i], items,
			_mesh_registry);
	// Frustum culling: an object whose world-space bounding box doesn't
	// intersect the camera frustum can't contribute a single visible
	// pixel, so it's dropped before even reaching the GPU. Shadow passes
	// above deliberately still use the full, unculled `items`: a caster
	// outside the camera's view can still cast a shadow into it.
	std::vector<RenderItem> frustum_visible_items = _frustum_culler.cull(view,
			projection, items, _mesh_registry);
	// Occlusion culling: of the frustum-visible items, skip any whose last
	// known query result (from OcclusionCuller::update_results(), a couple
	// of frames ago) said "produced zero visible samples." Items with no
	// occlusion_id (e.g. particles) are never skipped.
	std::vector<RenderItem> draw_items;
	draw_items.reserve(frustum_visible_items.size());
	for (const auto &item : frustum_visible_items)
	{
		if (!_occlusion_culler.is_known_occluded(item.occlusion_id()))
			draw_items.push_back(item);
	}
	// What gets (re-)tested this frame for *next* time: every
	// frustum-visible item that actually has a stable identity to test.
	std::vector<RenderItem> occlusion_test_items;
	occlusion_test_items.reserve(frustum_visible_items.size());
	for (const auto &item : frustum_visible_items)
	{
		if (item.occlusion_id() != RenderItem::no_occlusion_id())
			occlusion_test_items.push_back(item);
	}
	std::vector<uint32_t> query_ids;
	record_command_buffer(command_buffer, image_index, projection, draw_items,
		occlusion_test_items, &query_ids, screen_motion_blur_x,
		screen_motion_blur_y);
	_last_frame_stats.set_total_items(static_cast<uint32_t>(items.size()));
	_last_frame_stats.set_frustum_visible(
		static_cast<uint32_t>(frustum_visible_items.size()));
	_last_frame_stats.set_drawn(static_cast<uint32_t>(draw_items.size()));
	_frame_sync.end_and_submit(_vulkan_device, command_buffer);
	result = _frame_sync.present(_vulkan_device, _swap_chain.swapchain(),
			image_index);
	_last_presented_image_index = image_index;
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
		|| _window->was_resized())
	{
		_window->clear_resized_flag();
		recreate_swapchain();
	}
	else if (result != VK_SUCCESS)
	{
		std::fprintf(stderr, "Renderer: vkQueuePresentKHR failed (%d)\n",
			static_cast<int>(result));
		std::abort();
	}
}

} // namespace vre
