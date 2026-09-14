#include "renderer.hpp"
#include "vk_check.hpp"

namespace vre
{

void Renderer::record_command_buffer(VkCommandBuffer command_buffer,
	uint32_t image_index, const mat4 &projection,
	const std::vector<RenderItem> &draw_items,
	const std::vector<RenderItem> &occlusion_test_items,
	std::vector<uint32_t> *out_query_ids, float screen_motion_blur_x,
	float screen_motion_blur_y)
{
	VkExtent2D	extent;

	// Must happen outside any render pass instance (Vulkan spec
	// requirement for vkCmdResetQueryPool).
	_occlusion_culler.reset_query_pool(command_buffer, _current_frame);
	extent = _swap_chain.extent();
	_geometry_pass.begin_render_pass(command_buffer, extent, _current_frame);
	_geometry_pass.draw(command_buffer, draw_items, _mesh_registry,
		_texture_registry);
	// Occlusion pass: for every frustum-visible item with a stable
	// occlusion_id (see RenderItem), redraw it once more — now with
	// OcclusionCuller's own pipeline (no color/depth writes) — wrapped in a
	// query against the depth buffer the draw above just finished writing.
	// The result is read back two frames from now (OcclusionCuller::
	// update_results()) to decide whether *that* future frame draws this
	// object for real at all.
	_occlusion_culler.record(command_buffer, _current_frame,
		occlusion_test_items, _mesh_registry, _texture_registry,
		_geometry_pass.pipeline_layout(),
		_geometry_pass.global_descriptor_set(_current_frame), out_query_ids);
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
	VkDevice				device;
	uint32_t				image_index;
	VkResult				result;
	Light					default_shadow_caster;
	uint32_t				shadow_caster_count;
	mat4					light_space_matrices[ShadowPass::kMaxShadowCasters];
	VkCommandBuffer			command_buffer;
	VkSemaphore				wait_semaphores[] = {_image_available_semaphores[_current_frame]};
	VkPipelineStageFlags	wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore				signal_semaphores[] = {_render_finished_semaphores[_current_frame]};
	VkSwapchainKHR			swapchains[] = {_swap_chain.swapchain()};

	device = _vulkan_device.device();
	vkWaitForFences(device, 1, &_in_flight_fences[_current_frame], VK_TRUE,
		UINT64_MAX);
	// Must run before record_command_buffer() resets this frame-in-flight
	// slot's query pool, and after the fence wait above guarantees the
	// results are actually ready.
	_occlusion_culler.update_results(_current_frame);
	image_index = 0;
	result = vkAcquireNextImageKHR(device, _swap_chain.swapchain(), UINT64_MAX,
			_image_available_semaphores[_current_frame], VK_NULL_HANDLE,
			&image_index);
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
	vkResetFences(device, 1, &_in_flight_fences[_current_frame]);
	// The first min(kMaxShadowCasters, lights.size()) lights each cast a
	// shadow. A scene with no lights at all still gets one default
	// directional caster, so it shows *something* rather than aborting or
	// rendering fully black.
	default_shadow_caster.set_type(Light::Type::Directional);
	default_shadow_caster.set_direction_or_position(vec3(-0.4f, -1.0f, -0.3f));
	default_shadow_caster.set_color(vec3(1.0f, 1.0f, 1.0f));
	default_shadow_caster.set_intensity(1.0f);
	shadow_caster_count = lights.empty() ? 1 : std::min(static_cast<uint32_t>(lights.size()),
			ShadowPass::kMaxShadowCasters);
	for (uint32_t i = 0; i < shadow_caster_count; i++)
	{
		const Light &caster = lights.empty() ? default_shadow_caster : lights[i];
		light_space_matrices[i] = _shadow_pass.compute_light_space_matrix(caster);
	}
	for (uint32_t i = shadow_caster_count; i < ShadowPass::kMaxShadowCasters; i++)
		light_space_matrices[i] = mat4::identity();
	_geometry_pass.update_global_ubo(_current_frame, view, projection,
		light_space_matrices, shadow_caster_count, view_position, lights,
		ambient_intensity, bone_matrices);
	command_buffer = _command_buffers[_current_frame];
	vkResetCommandBuffer(command_buffer, 0);
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
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
	_last_frame_stats.set_frustum_visible(static_cast<uint32_t>(frustum_visible_items.size()));
	_last_frame_stats.set_drawn(static_cast<uint32_t>(draw_items.size()));
	VK_CHECK(vkEndCommandBuffer(command_buffer));
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;
	VK_CHECK(vkQueueSubmit(_vulkan_device.graphics_queue(), 1, &submit_info,
			_in_flight_fences[_current_frame]));
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &image_index;
	result = vkQueuePresentKHR(_vulkan_device.present_queue(), &present_info);
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
	_current_frame = (_current_frame + 1) % kMaxFramesInFlight;
}

} // namespace vre
