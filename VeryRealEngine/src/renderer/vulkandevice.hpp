/**
 * @file vulkandevice.hpp
 * @brief Picks a physical device (via VulkanPhysicalDeviceSelector), owns
 * the logical VkDevice/queues/command pool, and forwards the buffer/image
 * utility helpers shared by every render pass to VulkanBufferFactory /
 * VulkanImageFactory.
 */
#pragma once

#include "../vre.hpp"
#include "vkcheck.hpp"
#include "vulkanbufferfactory.hpp"
#include "vulkancommandscope.hpp"
#include "vulkanimagefactory.hpp"
#include "vulkanphysicaldeviceselector.hpp"

namespace vre
{
class VulkanDevice
{
  public:
	VulkanDevice();
	~VulkanDevice();

	/**
	 * @brief Picks a physical device, creates the logical device +
	 * queues, and creates the command pool.
	 * @param instance Vulkan instance to pick a physical device from.
	 * @param surface Presentation surface, used to find a
	 * present-capable queue family.
	 * @param validation_enabled Whether VulkanInstance enabled
	 * validation layers (see VulkanInstance::validation_enabled()).
	 */
	void create(VkInstance instance, VkSurfaceKHR surface,
		bool validation_enabled);
	/// Destroys the command pool, then the logical device.
	void destroy();

	VkPhysicalDevice physical_device() const;
	VkDevice device() const;
	VkQueue graphics_queue() const;
	VkQueue present_queue() const;
	uint32_t graphics_queue_family() const;
	uint32_t present_queue_family() const;
	VkCommandPool command_pool() const;

	/// @return The best supported depth format for this physical device.
	VkFormat find_depth_format() const;
	/// @return The memory type index matching `type_filter` and
	/// `properties`.
	uint32_t find_memory_type(uint32_t type_filter,
		VkMemoryPropertyFlags properties) const;
	/// Allocates a VkBuffer + backing VkDeviceMemory with the given
	/// usage/properties.
	void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer *out_buffer,
		VkDeviceMemory *out_memory) const;
	/// Uploads `data` to a new device-local buffer via a staging buffer.
	void upload_to_device_local_buffer(const void *data, VkDeviceSize size,
		VkBufferUsageFlags usage, VkBuffer *out_buffer,
		VkDeviceMemory *out_memory) const;

	/// @return A command buffer allocated and begun for a one-off,
	/// immediately-submitted command.
	VkCommandBuffer begin_single_time_commands() const;
	/// Ends, submits, and waits on a command buffer from
	/// begin_single_time_commands().
	void end_single_time_commands(VkCommandBuffer command_buffer) const;
	/// Records an image layout transition barrier.
	void transition_image_layout(VkImage image, VkFormat format,
		VkImageLayout old_layout, VkImageLayout new_layout) const;
	/// Records a buffer-to-image copy for uploading pixel data.
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width,
		uint32_t height) const;

	/// @return An image view over `image`, shared by every render pass
	/// that needs one.
	static VkImageView create_image_view(VkDevice device, VkImage image,
		VkFormat format, VkImageAspectFlags aspect_flags);
	/// @return A shader module loaded from a precompiled SPIR-V `.spv`
	/// file, shared by every pass.
	static VkShaderModule load_shader_module(VkDevice device,
		const char *path);

  private:
	// Owns VkDevice/VkCommandPool — the pre-C++11 idiom of a private,
	// never-defined copy constructor/assignment operator (this project
	// avoids `= delete`).
	VulkanDevice(const VulkanDevice &other);
	VulkanDevice &operator=(const VulkanDevice &other);

	void create_logical_device();
	void create_command_pool();

	static constexpr const char *kValidationLayer = "VK_LAYER_KHRONOS_validation";

	VkPhysicalDevice _physical_device;
	VkDevice _device;
	uint32_t _graphics_queue_family;
	uint32_t _present_queue_family;
	VkQueue _graphics_queue;
	VkQueue _present_queue;
	VkCommandPool _command_pool;
	bool _validation_enabled;
};

} // namespace vre
