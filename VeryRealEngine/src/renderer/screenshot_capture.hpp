/**
 * @file screenshot_capture.hpp

	* @brief One-off debug capture of the last-presented swapchain image to a PPM file.
 */
#pragma once

#include "../vre.hpp"
#include "vulkan_device.hpp"

namespace vre
{

class ScreenshotCapture
{
  public:
	ScreenshotCapture();
	ScreenshotCapture(const ScreenshotCapture &other);
	ScreenshotCapture &operator=(const ScreenshotCapture &other);
	~ScreenshotCapture();

	/// @param image The swapchain image to read back; must already be idle (fully presented).
	/// @return Whether the PPM file was written successfully.
	bool capture(const VulkanDevice &device, VkImage image,
		VkFormat image_format, uint32_t width, uint32_t height,
		const char *path) const;
};

} // namespace vre
