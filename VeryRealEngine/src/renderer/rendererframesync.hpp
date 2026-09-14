/**
 * @file rendererframesync.hpp
 * @brief The per-frame-in-flight command buffers, semaphores, and fences —
 * factored out of Renderer so that class stays focused on deciding what
 * to draw, not the swapchain-acquire/submit/present mechanics.
 *
 * Methods are deliberately granular (mirroring draw_frame()'s exact
 * sequence of Vulkan calls) rather than one big "do a frame" method,
 * since Renderer needs to do real work (occlusion-result readback,
 * culling, command recording) at specific points in between them.
 */
#pragma once

#include "../vre.hpp"
#include "vulkandevice.hpp"

namespace vre
{
class RendererFrameSync
{
  public:
	RendererFrameSync();
	~RendererFrameSync();

	/// Allocates one command buffer and creates one image-available/
	/// render-finished semaphore pair and one (initially signaled) fence
	/// per frame-in-flight slot.
	void create(const VulkanDevice &device, uint32_t frames_in_flight);
	void destroy(const VulkanDevice &device);

	/// @return Which frame-in-flight slot is currently active.
	uint32_t current_frame() const;
	/// @return The current frame-in-flight slot's command buffer.
	VkCommandBuffer command_buffer() const;

	/// Blocks until the current frame-in-flight slot's previous
	/// submission (if any) has finished on the GPU.
	void wait_for_fence(const VulkanDevice &device) const;
	/// @return The swapchain image index to render into this frame,
	/// via the current slot's image-available semaphore.
	VkResult acquire_next_image(const VulkanDevice &device,
		VkSwapchainKHR swapchain, uint32_t *out_image_index) const;
	/// Resets the current frame-in-flight slot's fence, ready to be
	/// signaled again by this frame's submit().
	void reset_fence(const VulkanDevice &device) const;

	/// Begins recording the current frame-in-flight slot's command
	/// buffer (reset first). @return That command buffer.
	VkCommandBuffer begin_command_buffer() const;
	/// Ends recording and submits `command_buffer` to the graphics
	/// queue, waiting on this frame's image-available semaphore and
	/// signaling its render-finished semaphore + fence.
	void end_and_submit(const VulkanDevice &device,
		VkCommandBuffer command_buffer) const;
	/// Presents `image_index` once the submitted work finishes, then
	/// advances to the next frame-in-flight slot.
	VkResult present(const VulkanDevice &device, VkSwapchainKHR swapchain,
		uint32_t image_index);

  private:
	// Owns VkCommandBuffer[]/VkSemaphore[]/VkFence[] — the pre-C++11
	// idiom of a private, never-defined copy constructor/assignment
	// operator (this project avoids `= delete`).
	RendererFrameSync(const RendererFrameSync &other);
	RendererFrameSync &operator=(const RendererFrameSync &other);

	uint32_t _frames_in_flight;
	uint32_t _current_frame;

	std::vector<VkCommandBuffer> _command_buffers;
	std::vector<VkSemaphore> _image_available_semaphores;
	std::vector<VkSemaphore> _render_finished_semaphores;
	std::vector<VkFence> _in_flight_fences;
};

} // namespace vre
