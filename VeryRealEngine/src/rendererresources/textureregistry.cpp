#include "textureregistry.hpp"
#include "../vulkan/check.hpp"

namespace vre
{
TextureRegistry::TextureRegistry() : _device(nullptr),
	_material_set_layout(VK_NULL_HANDLE), _descriptor_pool(VK_NULL_HANDLE),
	_texture_sampler(VK_NULL_HANDLE),
	_default_white_texture(TextureHandle::invalid()),
	_default_material(MaterialHandle::invalid())
{
}

TextureRegistry::~TextureRegistry()
{
}

void TextureRegistry::create_descriptor_set_layout()
{
	// Set 0: per-material diffuse sampler (one descriptor set per material,
	// allocated in create_material()).
	VkDescriptorSetLayoutBinding material_sampler_binding{};
	material_sampler_binding.binding = 0;
	material_sampler_binding.descriptorType =
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	material_sampler_binding.descriptorCount = 1;
	material_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo material_layout_info{};
	material_layout_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	material_layout_info.bindingCount = 1;
	material_layout_info.pBindings = &material_sampler_binding;
	VK_CHECK(vkCreateDescriptorSetLayout(_device->device(),
			&material_layout_info, nullptr, &_material_set_layout));
}

void TextureRegistry::create_descriptor_pool()
{
	const uint32_t	kMaxMaterials = 64;

	// Modest fixed budget: plenty for a hand-authored demo scene. A scene
	// system with dynamic material counts would size (or grow) this instead.
	VkDescriptorPoolSize pool_size{};
	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = kMaxMaterials;
	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	pool_info.maxSets = kMaxMaterials;
	VK_CHECK(vkCreateDescriptorPool(_device->device(), &pool_info, nullptr,
			&_descriptor_pool));
}

void TextureRegistry::create_texture_sampler()
{
	VkPhysicalDeviceProperties	properties;

	vkGetPhysicalDeviceProperties(_device->physical_device(), &properties);
	VkSamplerCreateInfo sampler_info{};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	VK_CHECK(vkCreateSampler(_device->device(), &sampler_info, nullptr,
			&_texture_sampler));
}

void TextureRegistry::create(const VulkanDevice &device)
{
	MaterialData	default_material_data;

	_device = &device;
	create_descriptor_set_layout();
	create_descriptor_pool();
	create_texture_sampler();
	_default_white_texture = create_default_white_texture();
	default_material_data.set_name("__default_white");
	_default_material = create_material(default_material_data);
}

void TextureRegistry::destroy()
{
	VkDevice	device;

	device = _device->device();
	for (const GpuTexture &texture : _textures)
	{
		vkDestroyImageView(device, texture.view, nullptr);
		vkDestroyImage(device, texture.image, nullptr);
		vkFreeMemory(device, texture.memory, nullptr);
	}
	_textures.clear();
	_materials.clear();
	if (_texture_sampler != VK_NULL_HANDLE)
		vkDestroySampler(device, _texture_sampler, nullptr);
	_texture_sampler = VK_NULL_HANDLE;
	if (_descriptor_pool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(device, _descriptor_pool, nullptr);
	_descriptor_pool = VK_NULL_HANDLE;
	if (_material_set_layout != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(device, _material_set_layout, nullptr);
	_material_set_layout = VK_NULL_HANDLE;
}

VkDescriptorSetLayout TextureRegistry::material_set_layout() const
{
	return (_material_set_layout);
}

TextureHandle TextureRegistry::default_white_texture() const
{
	return (_default_white_texture);
}

MaterialHandle TextureRegistry::default_material() const
{
	return (_default_material);
}

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
		std::fprintf(stderr,
			"Renderer: failed to load texture \"%s\", using default white\n",
			path.c_str());
		return (_default_white_texture);
	}
	handle = load_texture_from_image_data(image, path);
	_texture_cache[path] = handle;
	return (handle);
}

TextureHandle TextureRegistry::load_texture_from_image_data(
	const ImageData &image, const std::string & /*debug_name*/)
{
	GpuTexture	texture;

	TextureUploader::upload(*_device, image, &texture.image, &texture.memory,
		&texture.view);
	TextureHandle handle(_textures.size());
	_textures.push_back(texture);
	return (handle);
}

MaterialHandle TextureRegistry::create_material(const MaterialData &data)
{
	TextureHandle	texture;
	VkDevice		device;
	GpuMaterial		material;

	texture = data.diffuse_texture_path().empty() ? _default_white_texture
		: load_texture(data.diffuse_texture_path());
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

VkDescriptorSet TextureRegistry::material_descriptor_set(
	MaterialHandle handle) const
{
	return (_materials[handle.value()].descriptor_set);
}

const float *TextureRegistry::material_diffuse_tint(
	MaterialHandle handle) const
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
