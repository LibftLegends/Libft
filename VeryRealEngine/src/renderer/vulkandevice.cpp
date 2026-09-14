#include "vulkandevice.hpp"

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

void VulkanDevice::create_logical_device()
{
	float	queue_priority;

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
	std::vector<const char *> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
	// MoltenVK devices advertise VK_KHR_portability_subset; the spec
	// requires enabling it whenever a device supports it. Checked
	// dynamically (rather than assumed) so this has no effect running
	// against a real Vulkan driver that doesn't expose it.
	uint32_t available_extension_count = 0;
	vkEnumerateDeviceExtensionProperties(_physical_device, nullptr,
		&available_extension_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(
		available_extension_count);
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
	create_info.queueCreateInfoCount =
		static_cast<uint32_t>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount =
		static_cast<uint32_t>(device_extensions.size());
	create_info.ppEnabledExtensionNames = device_extensions.data();
	if (_validation_enabled)
	{
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = &kValidationLayer;
	}
	VK_CHECK(vkCreateDevice(_physical_device, &create_info, nullptr,
			&_device));
	vkGetDeviceQueue(_device, _graphics_queue_family, 0, &_graphics_queue);
	vkGetDeviceQueue(_device, _present_queue_family, 0, &_present_queue);
}

void VulkanDevice::create_command_pool()
{
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = _graphics_queue_family;
	VK_CHECK(vkCreateCommandPool(_device, &pool_info, nullptr,
			&_command_pool));
}

void VulkanDevice::create(VkInstance instance, VkSurfaceKHR surface,
	bool validation_enabled)
{
	_validation_enabled = validation_enabled;
	if (!VulkanPhysicalDeviceSelector::select(instance, surface,
			&_physical_device, &_graphics_queue_family,
			&_present_queue_family))
		std::abort();
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

VkFormat VulkanDevice::find_depth_format() const
{
	return (VulkanImageFactory::find_depth_format(_physical_device));
}

uint32_t VulkanDevice::find_memory_type(uint32_t type_filter,
	VkMemoryPropertyFlags properties) const
{
	return (VulkanBufferFactory::find_memory_type(_physical_device,
			type_filter, properties));
}

void VulkanDevice::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties, VkBuffer *out_buffer,
	VkDeviceMemory *out_memory) const
{
	VulkanBufferFactory::create_buffer(_device, _physical_device, size,
		usage, properties, out_buffer, out_memory);
}

VkCommandBuffer VulkanDevice::begin_single_time_commands() const
{
	return (VulkanCommandScope::begin(_device, _command_pool));
}

void VulkanDevice::end_single_time_commands(
	VkCommandBuffer command_buffer) const
{
	VulkanCommandScope::end(_device, _command_pool, _graphics_queue,
		command_buffer);
}

void VulkanDevice::upload_to_device_local_buffer(const void *data,
	VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *out_buffer,
	VkDeviceMemory *out_memory) const
{
	VulkanBufferFactory::upload_to_device_local_buffer(_device,
		_physical_device, _command_pool, _graphics_queue, data, size, usage,
		out_buffer, out_memory);
}

void VulkanDevice::transition_image_layout(VkImage image, VkFormat format,
	VkImageLayout old_layout, VkImageLayout new_layout) const
{
	VulkanImageFactory::transition_image_layout(_device, _command_pool,
		_graphics_queue, image, format, old_layout, new_layout);
}

void VulkanDevice::copy_buffer_to_image(VkBuffer buffer, VkImage image,
	uint32_t width, uint32_t height) const
{
	VulkanImageFactory::copy_buffer_to_image(_device, _command_pool,
		_graphics_queue, buffer, image, width, height);
}

VkImageView VulkanDevice::create_image_view(VkDevice device, VkImage image,
	VkFormat format, VkImageAspectFlags aspect_flags)
{
	return (VulkanImageFactory::create_image_view(device, image, format,
			aspect_flags));
}

VkShaderModule VulkanDevice::load_shader_module(VkDevice device,
	const char *path)
{
	return (VulkanImageFactory::load_shader_module(device, path));
}

} // namespace vre
