#include "vulkanimagefactory.hpp"
#include "vkcheck.hpp"
#include "vulkancommandscope.hpp"

namespace vre
{
VulkanImageFactory::VulkanImageFactory()
{
}

VulkanImageFactory::VulkanImageFactory(const VulkanImageFactory &)
{
}

VulkanImageFactory &VulkanImageFactory::operator=(
	const VulkanImageFactory &)
{
	return (*this);
}

VulkanImageFactory::~VulkanImageFactory()
{
}

VkFormat VulkanImageFactory::find_depth_format(
	VkPhysicalDevice physical_device)
{
	VkFormat candidates[] = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physical_device, format,
			&properties);
		if (properties.optimalTilingFeatures
			& VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return (format);
	}
	std::fprintf(stderr, "Renderer: no supported depth format found\n");
	std::abort();
}

void VulkanImageFactory::transition_image_layout(VkDevice device,
	VkCommandPool command_pool, VkQueue graphics_queue, VkImage image,
	VkFormat /*format*/, VkImageLayout old_layout, VkImageLayout new_layout)
{
	VkCommandBuffer command_buffer = VulkanCommandScope::begin(device,
			command_pool);

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

	vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0,
		0, nullptr, 0, nullptr, 1, &barrier);

	VulkanCommandScope::end(device, command_pool, graphics_queue,
		command_buffer);
}

void VulkanImageFactory::copy_buffer_to_image(VkDevice device,
	VkCommandPool command_pool, VkQueue graphics_queue, VkBuffer buffer,
	VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer command_buffer = VulkanCommandScope::begin(device,
			command_pool);

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

	VulkanCommandScope::end(device, command_pool, graphics_queue,
		command_buffer);
}

VkImageView VulkanImageFactory::create_image_view(VkDevice device,
	VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
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

VkShaderModule VulkanImageFactory::load_shader_module(VkDevice device,
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
