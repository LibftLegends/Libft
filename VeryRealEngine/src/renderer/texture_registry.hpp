/**
 * @file texture_registry.hpp
 * @brief Owns every GPU-resident texture and material: the material
 * descriptor set layout/pool, the shared texture sampler, and the
 * texture/material arrays themselves (cached by path where applicable).
 */
#pragma once

#include "../assets/material_data.hpp"
#include "../assets/tga_loader.hpp"
#include "../vre.hpp"
#include "material_handle.hpp"
#include "texture_handle.hpp"
#include "vulkan_device.hpp"

namespace vre
{

class TextureRegistry
{
  public:
	TextureRegistry();
	~TextureRegistry();

	/// Creates the material descriptor set layout/pool, texture sampler,
	/// then the default white texture and default material.
	void create(const VulkanDevice &device);
	void destroy();

	VkDescriptorSetLayout material_set_layout() const;
	TextureHandle default_white_texture() const;
	MaterialHandle default_material() const;

	/// Loads a texture from disk (TGA), caching by path.
	/// @return Handle to the loaded (or cached) texture.
	TextureHandle load_texture(const std::string &path);
	/// Uploads already-decoded image data as a texture.
	TextureHandle load_texture_from_image_data(const ImageData &image,
		const std::string &debug_name);
	/** Creates a GPU-resident material (descriptor set + push-constant parameters) from parsed data. */
	MaterialHandle create_material(const MaterialData &data);

	VkDescriptorSet material_descriptor_set(MaterialHandle handle) const;
	const float *material_diffuse_tint(MaterialHandle handle) const;
		///< 3 floats: r, g, b.
	float material_roughness(MaterialHandle handle) const;
	float material_metallic(MaterialHandle handle) const;

  private:
	// Owns VkImage/VkDeviceMemory/VkImageView/VkDescriptorPool/VkSampler
	// arrays — the pre-C++11 idiom of a private, never-defined copy
	// constructor/assignment operator (this project avoids `= delete`).
	TextureRegistry(const TextureRegistry &other);
	TextureRegistry &operator=(const TextureRegistry &other);

	void create_descriptor_set_layout();
	void create_descriptor_pool();
	void create_texture_sampler();
	/// @return Handle to a lazily-created 1x1 white fallback texture.
	TextureHandle create_default_white_texture();

	/// A single GPU-resident texture image. Pure GPU-layout data,
	/// manipulated only by TextureRegistry itself — same treatment as
	/// Renderer::GlobalUbo, not a fully encapsulated class.
	struct					GpuTexture
	{
		VkImage				image = VK_NULL_HANDLE;
		VkDeviceMemory		memory = VK_NULL_HANDLE;
		VkImageView			view = VK_NULL_HANDLE;
	};

	/// A GPU-resident material: a bound descriptor set plus the scalar
	/// parameters pushed per draw call (tint, roughness, metallic).
	struct					GpuMaterial
	{
		VkDescriptorSet		descriptor_set = VK_NULL_HANDLE;
		float				diffuse_tint[3] = {1.0f, 1.0f, 1.0f};
		float				roughness = 0.8f;
		float				metallic = 0.0f;
	};

	const VulkanDevice *_device; ///< Non-owning; set by create().

	VkDescriptorSetLayout	_material_set_layout;
	VkDescriptorPool		_descriptor_pool;
	VkSampler				_texture_sampler;

	std::vector<GpuTexture> _textures;
	std::vector<GpuMaterial> _materials;
	std::map<std::string, TextureHandle> _texture_cache;
	TextureHandle			_default_white_texture;
	MaterialHandle			_default_material;
};

} // namespace vre
