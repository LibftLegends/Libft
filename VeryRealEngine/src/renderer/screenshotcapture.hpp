/**
 * @file screenshotcapture.hpp
 * @brief One-off debug capture of the last-presented swapchain image to a
 * PPM file.
 */
#pragma once

#include "../vre.hpp"
#include "../vulkan/check.hpp"
#include "../vulkan/device.hpp"

namespace vre
{
class ScreenshotCapture
{
  public:
	ScreenshotCapture();
	ScreenshotCapture(const ScreenshotCapture &other);
	ScreenshotCapture &operator=(const ScreenshotCapture &other);
	~ScreenshotCapture();

	/**
	 * @param device Device to allocate the readback staging buffer against.
	 * @param image The swapchain image to read back; must already be
	 * idle (fully presented).
	 * @param image_format Pixel format of `image` (must be a format
	 * capture() supports; see the .cpp).
	 * @param width Image width, in pixels.
	 * @param height Image height, in pixels.
	 * @param path Output PPM file path.
	 * @return Whether the PPM file was written successfully.
	 */
	bool capture(const VulkanDevice &device, VkImage image,
		VkFormat image_format, uint32_t width, uint32_t height,
		const char *path) const;
};

} // namespace vre
