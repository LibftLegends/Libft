/**
 * @file vulkanphysicaldeviceselector.hpp
 * @brief Picks a suitable physical device (swapchain support, anisotropic
 * filtering, a graphics + present queue family) for a VulkanDevice to own.
 */
#pragma once

#include "../vre.hpp"
#include "swapchainsupportdetails.hpp"

namespace vre
{
class VulkanPhysicalDeviceSelector
{
  public:
	VulkanPhysicalDeviceSelector();
	VulkanPhysicalDeviceSelector(const VulkanPhysicalDeviceSelector &other);
	VulkanPhysicalDeviceSelector &operator=(
		const VulkanPhysicalDeviceSelector &other);
	~VulkanPhysicalDeviceSelector();

	/**
		* @brief Picks the first physical device supporting
		* VK_KHR_swapchain, a non-empty swapchain format/present-mode set,
		* sampler anisotropy, and both a graphics and a present queue family.
		* @param instance Vulkan instance to enumerate physical devices from.
		* @param surface Presentation surface, used to find a
		* present-capable queue family.
		* @param out_device Receives the chosen physical device.
		* @param out_graphics_family Receives the graphics queue family index.
		* @param out_present_family Receives the present queue family index.
		* @return false (aborting the process — see vkcheck.hpp's own
		* failure policy) if no suitable device exists.
		*/
	static bool select(VkInstance instance, VkSurfaceKHR surface,
		VkPhysicalDevice *out_device, uint32_t *out_graphics_family,
		uint32_t *out_present_family);

  private:
	static bool device_supports_extensions(VkPhysicalDevice device);
	static bool find_queue_families(VkPhysicalDevice device,
		VkSurfaceKHR surface, uint32_t *out_graphics_family,
		uint32_t *out_present_family);
};

} // namespace vre
