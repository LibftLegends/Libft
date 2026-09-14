#include "screenshotcapture.hpp"

namespace vre
{
ScreenshotCapture::ScreenshotCapture()
{
}

ScreenshotCapture::ScreenshotCapture(const ScreenshotCapture & /*other*/)
{
}

ScreenshotCapture &ScreenshotCapture::operator=(const ScreenshotCapture &
		/*other*/)
{
	return (*this);
}

ScreenshotCapture::~ScreenshotCapture()
{
}

bool ScreenshotCapture::capture(const VulkanDevice &device, VkImage image,
	VkFormat image_format, uint32_t width, uint32_t height,
	const char *path) const
{
	VkDevice vk_device = device.device();
	VkDeviceSize buffer_size = static_cast<VkDeviceSize>(width) * height * 4;

	VkBuffer staging_buffer;
	VkDeviceMemory staging_memory;
	device.create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer, &staging_memory);

	VkCommandBuffer command_buffer = device.begin_single_time_commands();

	VkImageMemoryBarrier to_transfer_src{};
	to_transfer_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	to_transfer_src.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	to_transfer_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	to_transfer_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_transfer_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_transfer_src.image = image;
	to_transfer_src.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	to_transfer_src.srcAccessMask = 0;
	to_transfer_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
		&to_transfer_src);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {width, height, 1};
	vkCmdCopyImageToBuffer(command_buffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer, 1, &region);

	// Restore PRESENT_SRC_KHR: this image goes back into the normal
	// acquire/draw/present rotation next time its index comes up.
	VkImageMemoryBarrier back_to_present = to_transfer_src;
	back_to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	back_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	back_to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	back_to_present.dstAccessMask = 0;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
		&back_to_present);

	device.end_single_time_commands(command_buffer);
		// waits for queue idle before returning

	void *mapped = nullptr;
	vkMapMemory(vk_device, staging_memory, 0, buffer_size, 0, &mapped);
	const uint8_t *pixels = static_cast<const uint8_t *>(mapped);

	// Swapchain formats are practically always some 8-bit-per-channel BGRA
	// or RGBA variant (see SwapChain::choose_surface_format()'s BGRA8_SRGB
	// preference) — swap channels 0/2 only for the BGR* case so the PPM
	// (which is always RGB) comes out with correct colors either way.
	bool is_bgr_order = (image_format == VK_FORMAT_B8G8R8A8_SRGB
			|| image_format == VK_FORMAT_B8G8R8A8_UNORM);

	std::ofstream out(path, std::ios::binary);
	if (!out.is_open())
	{
		vkUnmapMemory(vk_device, staging_memory);
		vkDestroyBuffer(vk_device, staging_buffer, nullptr);
		vkFreeMemory(vk_device, staging_memory, nullptr);
		std::fprintf(stderr,
			"Renderer: capture_screenshot: failed to open \"%s\"\n", path);
		return (false);
	}

	out << "P6\n" << width << " " << height << "\n255\n";
	std::vector<uint8_t> row(static_cast<size_t>(width) * 3);
	for (uint32_t y = 0; y < height; y++)
	{
		const uint8_t *src_row = pixels + static_cast<size_t>(y) * width * 4;
		for (uint32_t x = 0; x < width; x++)
		{
			const uint8_t *src_pixel = src_row + static_cast<size_t>(x) * 4;
			uint8_t r = is_bgr_order ? src_pixel[2] : src_pixel[0];
			uint8_t g = src_pixel[1];
			uint8_t b = is_bgr_order ? src_pixel[0] : src_pixel[2];
			row[x * 3 + 0] = r;
			row[x * 3 + 1] = g;
			row[x * 3 + 2] = b;
		}
		out.write(reinterpret_cast<const char *>(row.data()),
			static_cast<std::streamsize>(row.size()));
	}
	out.close();

	vkUnmapMemory(vk_device, staging_memory);
	vkDestroyBuffer(vk_device, staging_buffer, nullptr);
	vkFreeMemory(vk_device, staging_memory, nullptr);

	std::fprintf(stderr, "Renderer: wrote screenshot to \"%s\" (%ux%u)\n", path,
		width, height);
	return (true);
}

} // namespace vre
