#include "vulkancommandscope.hpp"

namespace vre
{
VulkanCommandScope::VulkanCommandScope()
{
}

VulkanCommandScope::VulkanCommandScope(const VulkanCommandScope &)
{
}

VulkanCommandScope &VulkanCommandScope::operator=(
	const VulkanCommandScope &)
{
	return (*this);
}

VulkanCommandScope::~VulkanCommandScope()
{
}

VkCommandBuffer VulkanCommandScope::begin(VkDevice device,
	VkCommandPool command_pool)
{
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	return (command_buffer);
}

void VulkanCommandScope::end(VkDevice device, VkCommandPool command_pool,
	VkQueue graphics_queue, VkCommandBuffer command_buffer)
{
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue);

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

} // namespace vre
