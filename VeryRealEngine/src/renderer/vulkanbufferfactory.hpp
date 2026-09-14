/**
 * @file vulkanbufferfactory.hpp
 * @brief Buffer allocation and staged device-local uploads — the buffer
 * half of the utility helpers shared by every render pass (see
 * VulkanImageFactory for the image half).
 *
 * Stateless: every method takes the Vulkan handles it needs explicitly,
 * rather than reading them off a VulkanDevice instance.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class VulkanBufferFactory
{
  public:
	VulkanBufferFactory();
	VulkanBufferFactory(const VulkanBufferFactory &other);
	VulkanBufferFactory &operator=(const VulkanBufferFactory &other);
	~VulkanBufferFactory();

	/// @return The memory type index matching `type_filter` and
	/// `properties` on `physical_device`.
	static uint32_t find_memory_type(VkPhysicalDevice physical_device,
		uint32_t type_filter, VkMemoryPropertyFlags properties);
	/// Allocates a VkBuffer + backing VkDeviceMemory with the given
	/// usage/properties.
	static void create_buffer(VkDevice device,
		VkPhysicalDevice physical_device, VkDeviceSize size,
		VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer *out_buffer, VkDeviceMemory *out_memory);
	/// Uploads `data` to a new device-local buffer via a staging buffer.
	static void upload_to_device_local_buffer(VkDevice device,
		VkPhysicalDevice physical_device, VkCommandPool command_pool,
		VkQueue graphics_queue, const void *data, VkDeviceSize size,
		VkBufferUsageFlags usage, VkBuffer *out_buffer,
		VkDeviceMemory *out_memory);
};

} // namespace vre
