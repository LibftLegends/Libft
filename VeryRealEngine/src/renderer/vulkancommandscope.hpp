/**
 * @file vulkancommandscope.hpp
 * @brief Allocates, begins, submits, and waits on a single one-off command
 * buffer — the primitive VulkanBufferFactory and VulkanImageFactory build
 * their staging-buffer copies and image-layout transitions on top of.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class VulkanCommandScope
{
  public:
	VulkanCommandScope();
	VulkanCommandScope(const VulkanCommandScope &other);
	VulkanCommandScope &operator=(const VulkanCommandScope &other);
	~VulkanCommandScope();

	/// @return A command buffer allocated and begun for a one-off,
	/// immediately-submitted command.
	static VkCommandBuffer begin(VkDevice device, VkCommandPool command_pool);
	/// Ends, submits, and waits on a command buffer from begin().
	static void end(VkDevice device, VkCommandPool command_pool,
		VkQueue graphics_queue, VkCommandBuffer command_buffer);
};

} // namespace vre
