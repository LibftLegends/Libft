#include "textureuploader.hpp"
#include "vkcheck.hpp"

namespace vre
{
TextureUploader::TextureUploader()
{
}

TextureUploader::TextureUploader(const TextureUploader &)
{
}

TextureUploader &TextureUploader::operator=(const TextureUploader &)
{
	return (*this);
}

TextureUploader::~TextureUploader()
{
}

void TextureUploader::upload(const VulkanDevice &device,
	const ImageData &image, VkImage *out_image, VkDeviceMemory *out_memory,
	VkImageView *out_view)
{
	VkDevice				vk_device;
	VkDeviceSize			image_size;
	VkBuffer				staging_buffer;
	VkDeviceMemory			staging_memory;
	void					*mapped;
	VkMemoryRequirements	memory_requirements;

	vk_device = device.device();
	image_size = static_cast<VkDeviceSize>(image.pixels().size());
	device.create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer, &staging_memory);
	vkMapMemory(vk_device, staging_memory, 0, image_size, 0, &mapped);
	std::memcpy(mapped, image.pixels().data(),
		static_cast<size_t>(image_size));
	vkUnmapMemory(vk_device, staging_memory);

	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = image.width();
	image_info.extent.height = image.height();
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	// UNORM, not SRGB: this engine doesn't do gamma-correct lighting yet
	// (a follow-up to step 5's lighting pass), so textures are sampled and
	// shaded as plain linear color for now.
	image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(vk_device, &image_info, nullptr, out_image));
	vkGetImageMemoryRequirements(vk_device, *out_image, &memory_requirements);
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = device.find_memory_type(
			memory_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(vk_device, &alloc_info, nullptr, out_memory));
	vkBindImageMemory(vk_device, *out_image, *out_memory, 0);

	device.transition_image_layout(*out_image, image_info.format,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	device.copy_buffer_to_image(staging_buffer, *out_image, image.width(),
		image.height());
	device.transition_image_layout(*out_image, image_info.format,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(vk_device, staging_buffer, nullptr);
	vkFreeMemory(vk_device, staging_memory, nullptr);
	*out_view = VulkanDevice::create_image_view(vk_device, *out_image,
			image_info.format, VK_IMAGE_ASPECT_COLOR_BIT);
}

} // namespace vre
