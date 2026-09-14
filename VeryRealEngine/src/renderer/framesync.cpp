#include "framesync.hpp"
#include "../vulkan/check.hpp"

namespace vre
{
RendererFrameSync::RendererFrameSync() : _frames_in_flight(0),
	_current_frame(0)
{
}

RendererFrameSync::~RendererFrameSync()
{
}

void RendererFrameSync::create(const VulkanDevice &device,
	uint32_t frames_in_flight)
{
	VkDevice	vk_device;

	_frames_in_flight = frames_in_flight;
	vk_device = device.device();
	_command_buffers.resize(frames_in_flight);
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = device.command_pool();
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = frames_in_flight;
	VK_CHECK(vkAllocateCommandBuffers(vk_device, &alloc_info,
			_command_buffers.data()));

	_image_available_semaphores.resize(frames_in_flight);
	_render_finished_semaphores.resize(frames_in_flight);
	_in_flight_fences.resize(frames_in_flight);
	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < frames_in_flight; i++)
	{
		VK_CHECK(vkCreateSemaphore(vk_device, &semaphore_info, nullptr,
				&_image_available_semaphores[i]));
		VK_CHECK(vkCreateSemaphore(vk_device, &semaphore_info, nullptr,
				&_render_finished_semaphores[i]));
		VK_CHECK(vkCreateFence(vk_device, &fence_info, nullptr,
				&_in_flight_fences[i]));
	}
}

void RendererFrameSync::destroy(const VulkanDevice &device)
{
	VkDevice	vk_device;

	vk_device = device.device();
	for (uint32_t i = 0; i < _frames_in_flight; i++)
	{
		vkDestroySemaphore(vk_device, _render_finished_semaphores[i], nullptr);
		vkDestroySemaphore(vk_device, _image_available_semaphores[i], nullptr);
		vkDestroyFence(vk_device, _in_flight_fences[i], nullptr);
	}
}

uint32_t RendererFrameSync::current_frame() const
{
	return (_current_frame);
}

VkCommandBuffer RendererFrameSync::command_buffer() const
{
	return (_command_buffers[_current_frame]);
}

void RendererFrameSync::wait_for_fence(const VulkanDevice &device) const
{
	vkWaitForFences(device.device(), 1, &_in_flight_fences[_current_frame],
		VK_TRUE, UINT64_MAX);
}

VkResult RendererFrameSync::acquire_next_image(const VulkanDevice &device,
	VkSwapchainKHR swapchain, uint32_t *out_image_index) const
{
	return (vkAcquireNextImageKHR(device.device(), swapchain, UINT64_MAX,
			_image_available_semaphores[_current_frame], VK_NULL_HANDLE,
			out_image_index));
}

void RendererFrameSync::reset_fence(const VulkanDevice &device) const
{
	vkResetFences(device.device(), 1, &_in_flight_fences[_current_frame]);
}

VkCommandBuffer RendererFrameSync::begin_command_buffer() const
{
	VkCommandBuffer	command_buffer;

	command_buffer = _command_buffers[_current_frame];
	vkResetCommandBuffer(command_buffer, 0);
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
	return (command_buffer);
}

void RendererFrameSync::end_and_submit(const VulkanDevice &device,
	VkCommandBuffer command_buffer) const
{
	VkSemaphore				wait_semaphores[] = {
		_image_available_semaphores[_current_frame]};
	VkPipelineStageFlags	wait_stages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore				signal_semaphores[] = {
		_render_finished_semaphores[_current_frame]};

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
	VK_CHECK(vkQueueSubmit(device.graphics_queue(), 1, &submit_info,
			_in_flight_fences[_current_frame]));
}

VkResult RendererFrameSync::present(const VulkanDevice &device,
	VkSwapchainKHR swapchain, uint32_t image_index)
{
	VkResult		result;
	VkSemaphore		signal_semaphores[] = {
		_render_finished_semaphores[_current_frame]};
	VkSwapchainKHR	swapchains[] = {swapchain};

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &image_index;
	result = vkQueuePresentKHR(device.present_queue(), &present_info);
	_current_frame = (_current_frame + 1) % _frames_in_flight;
	return (result);
}

} // namespace vre
