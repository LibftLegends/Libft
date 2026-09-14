#include "swapchain_support_details.hpp"
#include "vk_check.hpp"
#include "vulkan_device.hpp"

namespace vre
{

VulkanDevice::VulkanDevice() : _physical_device(VK_NULL_HANDLE),
	_device(VK_NULL_HANDLE), _graphics_queue_family(0),
	_present_queue_family(0), _graphics_queue(VK_NULL_HANDLE),
	_present_queue(VK_NULL_HANDLE), _command_pool(VK_NULL_HANDLE),
	_validation_enabled(false)
{
}

VulkanDevice::~VulkanDevice()
{
}

bool VulkanDevice::device_supports_extensions(VkPhysicalDevice device)
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

bool VulkanDevice::find_queue_families(VkPhysicalDevice device,
	VkSurfaceKHR surface, uint32_t *out_graphics_family,
	uint32_t *out_present_family)
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

void VulkanDevice::pick_physical_device(VkInstance instance,
	VkSurfaceKHR surface)
{
	uint32_t					device_count;
	SwapchainSupportDetails		support;
		VkPhysicalDeviceFeatures features;
	uint32_t					graphics_family;
	uint32_t					present_family;
	VkPhysicalDeviceProperties	properties;

	device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	if (device_count == 0)
	{
		std::fprintf(stderr,
			"Renderer: no Vulkan-capable physical device found\n");
		std::abort();
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
		_physical_device = device;
		_graphics_queue_family = graphics_family;
		_present_queue_family = present_family;
		break ;
	}
	if (_physical_device == VK_NULL_HANDLE)
	{
		std::fprintf(stderr,
			"Renderer: no suitable Vulkan physical device found\n");
		std::abort();
	}
	vkGetPhysicalDeviceProperties(_physical_device, &properties);
	std::fprintf(stderr, "Renderer: using physical device \"%s\"\n",
		properties.deviceName);
}

void VulkanDevice::create_logical_device()
{
	float		queue_priority;
	uint32_t	available_extension_count;

	std::set<uint32_t> unique_queue_families = {_graphics_queue_family,
		_present_queue_family};
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	queue_priority = 1.0f;
	for (uint32_t family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}
	VkPhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = VK_TRUE;
	std::vector<const char *> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
	// MoltenVK devices advertise VK_KHR_portability_subset; the spec
	// requires enabling it whenever a device supports it. Checked
	// dynamically (rather than assumed) so this has no effect running
	// against a real Vulkan driver that doesn't expose it.
	available_extension_count = 0;
	vkEnumerateDeviceExtensionProperties(_physical_device, nullptr,
		&available_extension_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(available_extension_count);
	vkEnumerateDeviceExtensionProperties(_physical_device, nullptr,
		&available_extension_count, available_extensions.data());
	for (const VkExtensionProperties &extension : available_extensions)
	{
		if (std::strcmp(extension.extensionName,
				"VK_KHR_portability_subset") == 0)
		{
			device_extensions.push_back("VK_KHR_portability_subset");
			break ;
		}
	}
#endif
	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	create_info.ppEnabledExtensionNames = device_extensions.data();
	if (_validation_enabled)
	{
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = &kValidationLayer;
	}
	VK_CHECK(vkCreateDevice(_physical_device, &create_info, nullptr, &_device));
	vkGetDeviceQueue(_device, _graphics_queue_family, 0, &_graphics_queue);
	vkGetDeviceQueue(_device, _present_queue_family, 0, &_present_queue);
}

void VulkanDevice::create_command_pool()
{
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = _graphics_queue_family;
	VK_CHECK(vkCreateCommandPool(_device, &pool_info, nullptr, &_command_pool));
}

void VulkanDevice::create(VkInstance instance, VkSurfaceKHR surface,
	bool validation_enabled)
{
	_validation_enabled = validation_enabled;
	pick_physical_device(instance, surface);
	create_logical_device();
	create_command_pool();
}

void VulkanDevice::destroy()
{
	if (_command_pool != VK_NULL_HANDLE)
		vkDestroyCommandPool(_device, _command_pool, nullptr);
	_command_pool = VK_NULL_HANDLE;
	if (_device != VK_NULL_HANDLE)
		vkDestroyDevice(_device, nullptr);
	_device = VK_NULL_HANDLE;
}

VkPhysicalDevice VulkanDevice::physical_device() const
{
	return (_physical_device);
}

VkDevice VulkanDevice::device() const
{
	return (_device);
}

VkQueue VulkanDevice::graphics_queue() const
{
	return (_graphics_queue);
}

VkQueue VulkanDevice::present_queue() const
{
	return (_present_queue);
}

uint32_t VulkanDevice::graphics_queue_family() const
{
	return (_graphics_queue_family);
}

uint32_t VulkanDevice::present_queue_family() const
{
	return (_present_queue_family);
}

VkCommandPool VulkanDevice::command_pool() const
{
	return (_command_pool);
}

} // namespace vre
