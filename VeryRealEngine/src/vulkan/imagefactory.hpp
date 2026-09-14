/**
 * @file imagefactory.hpp
 * @brief Image/shader-module utility helpers shared by every render pass
 * (see VulkanBufferFactory for the buffer half): depth format selection,
 * image views, layout transitions, buffer-to-image copies, and loading a
 * precompiled SPIR-V shader module.
 *
 * Stateless: every method takes the Vulkan handles it needs explicitly,
 * rather than reading them off a VulkanDevice instance.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class VulkanImageFactory
{
  public:
	VulkanImageFactory();
	VulkanImageFactory(const VulkanImageFactory &other);
	VulkanImageFactory &operator=(const VulkanImageFactory &other);
	~VulkanImageFactory();

	/// @return The best supported depth format for `physical_device`.
	static VkFormat find_depth_format(VkPhysicalDevice physical_device);
	/// Records (and submits/waits on) an image layout transition barrier.
	static void transition_image_layout(VkDevice device,
		VkCommandPool command_pool, VkQueue graphics_queue, VkImage image,
		VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
	/// Records (and submits/waits on) a buffer-to-image copy for
	/// uploading pixel data.
	static void copy_buffer_to_image(VkDevice device,
		VkCommandPool command_pool, VkQueue graphics_queue, VkBuffer buffer,
		VkImage image, uint32_t width, uint32_t height);
	/// @return An image view over `image`, shared by every render pass
	/// that needs one.
	static VkImageView create_image_view(VkDevice device, VkImage image,
		VkFormat format, VkImageAspectFlags aspect_flags);
	/// @return A shader module loaded from a precompiled SPIR-V `.spv`
	/// file, shared by every pass.
	static VkShaderModule load_shader_module(VkDevice device,
		const char *path);
};

} // namespace vre
