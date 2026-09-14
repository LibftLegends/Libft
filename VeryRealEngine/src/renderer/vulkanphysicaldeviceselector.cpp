#include "vulkanphysicaldeviceselector.hpp"

namespace vre
{
VulkanPhysicalDeviceSelector::VulkanPhysicalDeviceSelector()
{
}

VulkanPhysicalDeviceSelector::VulkanPhysicalDeviceSelector(
	const VulkanPhysicalDeviceSelector &)
{
}

VulkanPhysicalDeviceSelector &VulkanPhysicalDeviceSelector::operator=(
	const VulkanPhysicalDeviceSelector &)
{
	return (*this);
}

VulkanPhysicalDeviceSelector::~VulkanPhysicalDeviceSelector()
{
}

bool VulkanPhysicalDeviceSelector::device_supports_extensions(
	VkPhysicalDevice device)
{
	uint32_t	extension_count;

	extension_count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
		nullptr);
	std::vector<VkExtensionProperties> available(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
		available.data());
	for (const auto &extension : available)
	{
		if (std::strcmp(extension.extensionName,
				VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
			return (true);
	}
	return (false);
}

bool VulkanPhysicalDeviceSelector::find_queue_families(
	VkPhysicalDevice device, VkSurfaceKHR surface,
	uint32_t *out_graphics_family, uint32_t *out_present_family)
{
	uint32_t	queue_family_count;
	bool		found_graphics;
	bool		found_present;
	VkBool32	present_support;

	queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
		nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
		queue_families.data());
	found_graphics = false;
	found_present = false;
	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			*out_graphics_family = i;
			found_graphics = true;
		}
		present_support = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
			&present_support);
		if (present_support == VK_TRUE)
		{
			*out_present_family = i;
			found_present = true;
		}
		if (found_graphics && found_present)
			return (true);
	}
	return (found_graphics && found_present);
}

bool VulkanPhysicalDeviceSelector::select(VkInstance instance,
	VkSurfaceKHR surface, VkPhysicalDevice *out_device,
	uint32_t *out_graphics_family, uint32_t *out_present_family)
{
	uint32_t				device_count;
	SwapchainSupportDetails	support;
	VkPhysicalDeviceFeatures	features;
	uint32_t				graphics_family;
	uint32_t				present_family;
	VkPhysicalDeviceProperties	properties;

	*out_device = VK_NULL_HANDLE;
	device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	if (device_count == 0)
	{
		std::fprintf(stderr,
			"Renderer: no Vulkan-capable physical device found\n");
		return (false);
	}
	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
	for (auto device : devices)
	{
		if (!device_supports_extensions(device))
			continue ;
		support = SwapchainSupportDetails::query(device, surface);
		if (support.formats().empty() || support.present_modes().empty())
			continue ;
		vkGetPhysicalDeviceFeatures(device, &features);
		if (!features.samplerAnisotropy)
			continue ;
		graphics_family = 0;
		present_family = 0;
		if (!find_queue_families(device, surface, &graphics_family,
				&present_family))
			continue ;
		*out_device = device;
		*out_graphics_family = graphics_family;
		*out_present_family = present_family;
		break ;
	}
	if (*out_device == VK_NULL_HANDLE)
	{
		std::fprintf(stderr,
			"Renderer: no suitable Vulkan physical device found\n");
		return (false);
	}
	vkGetPhysicalDeviceProperties(*out_device, &properties);
	std::fprintf(stderr, "Renderer: using physical device \"%s\"\n",
		properties.deviceName);
	return (true);
}

} // namespace vre
