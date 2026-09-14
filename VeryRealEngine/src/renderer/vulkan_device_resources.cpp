#include "vk_check.hpp"
#include "vulkan_device.hpp"

namespace vre
{

VkFormat VulkanDevice::find_depth_format() const
{
	VkFormat candidates[] = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(_physical_device, format,
			&properties);
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return (format);
	}
	std::fprintf(stderr, "Renderer: no supported depth format found\n");
	std::abort();
}

uint32_t VulkanDevice::find_memory_type(uint32_t type_filter,
	VkMemoryPropertyFlags properties) const
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(_physical_device, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((type_filter & (1 << i))
			&& (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
			return (i);
	}
	std::fprintf(stderr, "Renderer: failed to find suitable memory type\n");
	std::abort();
}

void VulkanDevice::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties, VkBuffer *out_buffer,
	VkDeviceMemory *out_memory) const
{
	VkBufferCreateInfo buffer_info{};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(vkCreateBuffer(_device, &buffer_info, nullptr, out_buffer));

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(_device, *out_buffer, &memory_requirements);

	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits,
			properties);

	VK_CHECK(vkAllocateMemory(_device, &alloc_info, nullptr, out_memory));
	vkBindBufferMemory(_device, *out_buffer, *out_memory, 0);
}

VkCommandBuffer VulkanDevice::begin_single_time_commands() const
{
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = _command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(_device, &alloc_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	return (command_buffer);
}

void VulkanDevice::end_single_time_commands(VkCommandBuffer command_buffer) const
{
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(_graphics_queue);

	vkFreeCommandBuffers(_device, _command_pool, 1, &command_buffer);
}

void VulkanDevice::upload_to_device_local_buffer(const void *data,
	VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *out_buffer,
	VkDeviceMemory *out_memory) const
{
	VkBuffer staging_buffer;
	VkDeviceMemory staging_memory;
	create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer, &staging_memory);

	void *mapped;
	vkMapMemory(_device, staging_memory, 0, size, 0, &mapped);
	std::memcpy(mapped, data, static_cast<size_t>(size));
	vkUnmapMemory(_device, staging_memory);

	create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_buffer, out_memory);

	VkCommandBuffer command_buffer = begin_single_time_commands();
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, staging_buffer, *out_buffer, 1,
		&copy_region);
	end_single_time_commands(command_buffer);

	vkDestroyBuffer(_device, staging_buffer, nullptr);
	vkFreeMemory(_device, staging_memory, nullptr);
}

void VulkanDevice::transition_image_layout(VkImage image, VkFormat /*format*/,
	VkImageLayout old_layout, VkImageLayout new_layout) const
{
	VkCommandBuffer command_buffer = begin_single_time_commands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags source_stage;
	VkPipelineStageFlags destination_stage;

	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
		&& new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		&& new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		std::fprintf(stderr, "Renderer: unsupported image layout transition\n");
		std::abort();
	}

	vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0,
		nullptr, 0, nullptr, 1, &barrier);

	end_single_time_commands(command_buffer);
}

void VulkanDevice::copy_buffer_to_image(VkBuffer buffer, VkImage image,
	uint32_t width, uint32_t height) const
{
	VkCommandBuffer command_buffer = begin_single_time_commands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(command_buffer, buffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	end_single_time_commands(command_buffer);
}

VkImageView VulkanDevice::create_image_view(VkDevice device, VkImage image,
	VkFormat format, VkImageAspectFlags aspect_flags)
{
	VkImageView	view;

	VkImageViewCreateInfo view_info{};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.subresourceRange.aspectMask = aspect_flags;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &view));
	return (view);
}

VkShaderModule VulkanDevice::load_shader_module(VkDevice device,
	const char *path)
{
	size_t			file_size;
	VkShaderModule	module;

	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		std::fprintf(stderr, "Renderer: failed to open shader file \"%s\"\n",
			path);
		std::abort();
	}
	file_size = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(file_size));
	file.close();
	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = buffer.size();
	create_info.pCode = reinterpret_cast<const uint32_t *>(buffer.data());
	VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &module));
	return (module);
}

} // namespace vre
