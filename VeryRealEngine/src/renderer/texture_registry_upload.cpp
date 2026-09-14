#include "texture_registry.hpp"
#include "vk_check.hpp"

namespace vre
{

TextureHandle TextureRegistry::create_default_white_texture()
{
	ImageData	image;

	image.set_width(1);
	image.set_height(1);
	image.pixels() = {255, 255, 255, 255};
	return (load_texture_from_image_data(image, "__default_white"));
}

TextureHandle TextureRegistry::load_texture(const std::string &path)
{
	std::map<std::string, TextureHandle>::iterator cached;
	ImageData		image;
	TextureHandle	handle;

	cached = _texture_cache.find(path);
	if (cached != _texture_cache.end())
		return (cached->second);
	if (!TgaLoader::load(path.c_str(), &image))
	{
		std::fprintf(stderr, "Renderer: failed to load texture \"%s\", using default white\n",
			path.c_str());
		return (_default_white_texture);
	}
	handle = load_texture_from_image_data(image, path);
	_texture_cache[path] = handle;
	return (handle);
}

TextureHandle TextureRegistry::load_texture_from_image_data(const ImageData &image,
	const std::string & /*debug_name*/)
{
	VkDevice				device;
	VkDeviceSize			image_size;
	VkBuffer				staging_buffer;
	VkDeviceMemory			staging_memory;
	void					*mapped;
	GpuTexture				texture;
	VkMemoryRequirements	memory_requirements;

	device = _device->device();
	image_size = static_cast<VkDeviceSize>(image.pixels().size());
	_device->create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer, &staging_memory);
	vkMapMemory(device, staging_memory, 0, image_size, 0, &mapped);
	std::memcpy(mapped, image.pixels().data(), static_cast<size_t>(image_size));
	vkUnmapMemory(device, staging_memory);
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
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(device, &image_info, nullptr, &texture.image));
	vkGetImageMemoryRequirements(device, texture.image, &memory_requirements);
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = _device->find_memory_type(memory_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &texture.memory));
	vkBindImageMemory(device, texture.image, texture.memory, 0);
	_device->transition_image_layout(texture.image, image_info.format,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	_device->copy_buffer_to_image(staging_buffer, texture.image, image.width(),
		image.height());
	_device->transition_image_layout(texture.image, image_info.format,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkDestroyBuffer(device, staging_buffer, nullptr);
	vkFreeMemory(device, staging_memory, nullptr);
	texture.view = VulkanDevice::create_image_view(device, texture.image,
			image_info.format, VK_IMAGE_ASPECT_COLOR_BIT);
	TextureHandle handle(_textures.size());
	_textures.push_back(texture);
	return (handle);
}

MaterialHandle TextureRegistry::create_material(const MaterialData &data)
{
	TextureHandle	texture;
	VkDevice		device;
	GpuMaterial		material;

	texture = data.diffuse_texture_path().empty() ? _default_white_texture : load_texture(data.diffuse_texture_path());
	device = _device->device();
	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = _descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &_material_set_layout;
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info,
			&material.descriptor_set));
	VkDescriptorImageInfo image_info{};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = _textures[texture.value()].view;
	image_info.sampler = _texture_sampler;
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = material.descriptor_set;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image_info;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	material.diffuse_tint[0] = data.diffuse_color(0);
	material.diffuse_tint[1] = data.diffuse_color(1);
	material.diffuse_tint[2] = data.diffuse_color(2);
	material.roughness = data.roughness();
	material.metallic = data.metallic();
	MaterialHandle handle(_materials.size());
	_materials.push_back(material);
	return (handle);
}

VkDescriptorSet TextureRegistry::material_descriptor_set(MaterialHandle handle) const
{
	return (_materials[handle.value()].descriptor_set);
}

const float *TextureRegistry::material_diffuse_tint(MaterialHandle handle) const
{
	return (_materials[handle.value()].diffuse_tint);
}

float TextureRegistry::material_roughness(MaterialHandle handle) const
{
	return (_materials[handle.value()].roughness);
}

float TextureRegistry::material_metallic(MaterialHandle handle) const
{
	return (_materials[handle.value()].metallic);
}

} // namespace vre
