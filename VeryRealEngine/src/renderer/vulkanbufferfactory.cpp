#include "vulkanbufferfactory.hpp"
#include "vkcheck.hpp"
#include "vulkancommandscope.hpp"

namespace vre
{
VulkanBufferFactory::VulkanBufferFactory()
{
}

VulkanBufferFactory::VulkanBufferFactory(const VulkanBufferFactory &)
{
}

VulkanBufferFactory &VulkanBufferFactory::operator=(
	const VulkanBufferFactory &)
{
	return (*this);
}

VulkanBufferFactory::~VulkanBufferFactory()
{
}

uint32_t VulkanBufferFactory::find_memory_type(
	VkPhysicalDevice physical_device, uint32_t type_filter,
	VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((type_filter & (1 << i))
			&& (memory_properties.memoryTypes[i].propertyFlags & properties)
				== properties)
			return (i);
	}
	std::fprintf(stderr, "Renderer: failed to find suitable memory type\n");
	std::abort();
}

void VulkanBufferFactory::create_buffer(VkDevice device,
	VkPhysicalDevice physical_device, VkDeviceSize size,
	VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VkBuffer *out_buffer, VkDeviceMemory *out_memory)
{
	VkBufferCreateInfo buffer_info{};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, out_buffer));

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(device, *out_buffer, &memory_requirements);

	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(physical_device,
			memory_requirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, out_memory));
	vkBindBufferMemory(device, *out_buffer, *out_memory, 0);
}

void VulkanBufferFactory::upload_to_device_local_buffer(VkDevice device,
	VkPhysicalDevice physical_device, VkCommandPool command_pool,
	VkQueue graphics_queue, const void *data, VkDeviceSize size,
	VkBufferUsageFlags usage, VkBuffer *out_buffer,
	VkDeviceMemory *out_memory)
{
	VkBuffer staging_buffer;
	VkDeviceMemory staging_memory;
	create_buffer(device, physical_device, size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer, &staging_memory);

	void *mapped;
	vkMapMemory(device, staging_memory, 0, size, 0, &mapped);
	std::memcpy(mapped, data, static_cast<size_t>(size));
	vkUnmapMemory(device, staging_memory);

	create_buffer(device, physical_device, size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_buffer, out_memory);

	VkCommandBuffer command_buffer = VulkanCommandScope::begin(device,
			command_pool);
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, staging_buffer, *out_buffer, 1,
		&copy_region);
	VulkanCommandScope::end(device, command_pool, graphics_queue,
		command_buffer);

	vkDestroyBuffer(device, staging_buffer, nullptr);
	vkFreeMemory(device, staging_memory, nullptr);
}

} // namespace vre
