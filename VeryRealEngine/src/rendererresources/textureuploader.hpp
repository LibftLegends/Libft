/**
 * @file textureuploader.hpp
 * @brief Creates a 2D GPU texture image from decoded pixel data and
 * uploads it via a staging buffer — factored out of TextureRegistry so
 * that class stays focused on the material/texture cache itself.
 */
#pragma once

#include "../images/data.hpp"
#include "../vre.hpp"
#include "../vulkan/device.hpp"

namespace vre
{
class TextureUploader
{
  public:
	TextureUploader();
	TextureUploader(const TextureUploader &other);
	TextureUploader &operator=(const TextureUploader &other);
	~TextureUploader();

	/**
		* @brief Creates a device-local VK_FORMAT_R8G8B8A8_UNORM image
		* sized to `image`, uploads its pixels via a staging buffer, and
		* transitions it to shader-read-only-optimal layout, ready to bind.
		* @param device Device to allocate/upload through.
		* @param image Decoded RGBA8 pixel data.
		* @param out_image Receives the created image.
		* @param out_memory Receives the image's backing device memory.
		* @param out_view Receives an image view over the created image.
		*/
	static void upload(const VulkanDevice &device, const ImageData &image,
		VkImage *out_image, VkDeviceMemory *out_memory, VkImageView *out_view);
};

} // namespace vre
