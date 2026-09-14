#include "renderer.hpp"
#include "vk_check.hpp"

namespace vre
{

Renderer::Renderer() : _window(nullptr), _current_frame(0),
	_last_presented_image_index(0)
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
	create_command_buffers();
	create_sync_objects();
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
	for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
	{
		vkDestroySemaphore(_vulkan_device.device(),
			_render_finished_semaphores[i], nullptr);
		vkDestroySemaphore(_vulkan_device.device(),
			_image_available_semaphores[i], nullptr);
		vkDestroyFence(_vulkan_device.device(), _in_flight_fences[i], nullptr);
	}
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

void Renderer::create_command_buffers()
{
	_command_buffers.resize(kMaxFramesInFlight);
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = _vulkan_device.command_pool();
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = kMaxFramesInFlight;
	VK_CHECK(vkAllocateCommandBuffers(_vulkan_device.device(), &alloc_info,
			_command_buffers.data()));
}

void Renderer::create_sync_objects()
{
	_image_available_semaphores.resize(kMaxFramesInFlight);
	_render_finished_semaphores.resize(kMaxFramesInFlight);
	_in_flight_fences.resize(kMaxFramesInFlight);
	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
	{
		VK_CHECK(vkCreateSemaphore(_vulkan_device.device(), &semaphore_info,
				nullptr, &_image_available_semaphores[i]));
		VK_CHECK(vkCreateSemaphore(_vulkan_device.device(), &semaphore_info,
				nullptr, &_render_finished_semaphores[i]));
		VK_CHECK(vkCreateFence(_vulkan_device.device(), &fence_info, nullptr,
				&_in_flight_fences[i]));
	}
}

void Renderer::recreate_swapchain()
{
	// Extent 0 (minimized window) — wait until the window reports real size again.
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

MeshHandle Renderer::load_skinned_mesh(const char *path, Skeleton *out_skeleton,
	AnimationClip *out_clip)
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

} // namespace vre
