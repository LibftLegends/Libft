#include "geometryglobalubo.hpp"
#include "vkcheck.hpp"

namespace vre
{
GeometryGlobalUbo::GeometryGlobalUbo() : _device(VK_NULL_HANDLE),
	_descriptor_pool(VK_NULL_HANDLE)
{
}

GeometryGlobalUbo::~GeometryGlobalUbo()
{
}

VkDeviceSize GeometryGlobalUbo::ubo_size()
{
	return (sizeof(GlobalUbo));
}

void GeometryGlobalUbo::create(const VulkanDevice &device,
	VkDescriptorSetLayout global_set_layout, const ShadowPass &shadow_pass,
	uint32_t frames_in_flight)
{
	VkDeviceSize	buffer_size;

	_device = device.device();
	buffer_size = sizeof(GlobalUbo);
	_ubo_buffers.resize(frames_in_flight);
	_ubo_memories.resize(frames_in_flight);
	_ubo_mapped.resize(frames_in_flight);
	for (uint32_t i = 0; i < frames_in_flight; i++)
	{
		device.create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&_ubo_buffers[i], &_ubo_memories[i]);
		vkMapMemory(_device, _ubo_memories[i], 0, buffer_size, 0,
			&_ubo_mapped[i]);
	}
	VkDescriptorPoolSize pool_sizes[2]{};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = frames_in_flight;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = frames_in_flight
		* ShadowPass::kMaxShadowCasters;
	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 2;
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = frames_in_flight;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr,
			&_descriptor_pool));
	std::vector<VkDescriptorSetLayout> layouts(frames_in_flight,
		global_set_layout);
	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = _descriptor_pool;
	alloc_info.descriptorSetCount = frames_in_flight;
	alloc_info.pSetLayouts = layouts.data();
	_descriptor_sets.resize(frames_in_flight);
	VK_CHECK(vkAllocateDescriptorSets(_device, &alloc_info,
			_descriptor_sets.data()));
	for (uint32_t i = 0; i < frames_in_flight; i++)
	{
		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = _ubo_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = sizeof(GlobalUbo);
		VkDescriptorImageInfo shadow_image_infos[ShadowPass::kMaxShadowCasters]{};
		for (uint32_t c = 0; c < ShadowPass::kMaxShadowCasters; c++)
		{
			shadow_image_infos[c].imageLayout =
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			shadow_image_infos[c].imageView = shadow_pass.shadow_image_view(c);
			shadow_image_infos[c].sampler = shadow_pass.sampler();
		}
		VkWriteDescriptorSet writes[2]{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = _descriptor_sets[i];
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &buffer_info;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = _descriptor_sets[i];
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = ShadowPass::kMaxShadowCasters;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = shadow_image_infos;
		vkUpdateDescriptorSets(_device, 2, writes, 0, nullptr);
	}
}

void GeometryGlobalUbo::destroy()
{
	for (size_t i = 0; i < _ubo_buffers.size(); i++)
	{
		vkUnmapMemory(_device, _ubo_memories[i]);
		vkDestroyBuffer(_device, _ubo_buffers[i], nullptr);
		vkFreeMemory(_device, _ubo_memories[i], nullptr);
	}
	_ubo_buffers.clear();
	_ubo_memories.clear();
	_ubo_mapped.clear();
	vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
	_descriptor_pool = VK_NULL_HANDLE;
}

VkDescriptorSet GeometryGlobalUbo::descriptor_set(uint32_t frame_index) const
{
	return (_descriptor_sets[frame_index]);
}

void GeometryGlobalUbo::update(uint32_t frame_index, const mat4 &view,
	const mat4 &projection, const mat4 *light_space_matrices,
	uint32_t shadow_caster_count, const vec3 &view_position,
	const std::vector<Light> &lights, float ambient_intensity,
	const std::vector<mat4> &bone_matrices)
{
	mat4		identity;
	uint32_t	bone_count;
	uint32_t	light_count;

	GlobalUbo ubo{};
	ubo.view_proj = mat4::multiply(projection, view);
	// Slot 0 is always the identity (every static mesh's vertices are fully
	// weighted to it — see meshdata.hpp's MeshVertex doc comment); every
	// other slot also defaults to identity so an out-of-range bone_indices
	// value on some future asset reads harmless identity rather than
	// uninitialized/zero (a zero mat4 would collapse every position it
	// touches to the origin, a much worse failure mode than "no visible
	// deformation").
	identity = mat4::identity();
	for (uint32_t i = 0; i < kMaxBones; i++)
		ubo.bone_matrices[i] = identity;
	bone_count = std::min(static_cast<uint32_t>(bone_matrices.size()), kMaxBones
			- 1);
	for (uint32_t i = 0; i < bone_count; i++)
		ubo.bone_matrices[i + 1] = bone_matrices[i];
	for (uint32_t i = 0; i < ShadowPass::kMaxShadowCasters; i++)
		ubo.light_space_matrices[i] = light_space_matrices[i];
	ubo.shadow_caster_count[0] = static_cast<float>(shadow_caster_count);
	light_count = std::min(static_cast<uint32_t>(lights.size()), kMaxLights);
	for (uint32_t i = 0; i < light_count; i++)
	{
		const Light &light = lights[i];
		ubo.light_direction_or_position[i][0] = light.direction_or_position().x();
		ubo.light_direction_or_position[i][1] = light.direction_or_position().y();
		ubo.light_direction_or_position[i][2] = light.direction_or_position().z();
		ubo.light_direction_or_position[i][3] =
			(light.type() == Light::Type::Point) ? 1.0f : 0.0f;
		ubo.light_color_intensity[i][0] = light.color().x();
		ubo.light_color_intensity[i][1] = light.color().y();
		ubo.light_color_intensity[i][2] = light.color().z();
		ubo.light_color_intensity[i][3] = light.intensity();
	}
	ubo.light_count_ambient[0] = static_cast<float>(light_count);
	ubo.light_count_ambient[1] = ambient_intensity;
	ubo.view_position[0] = view_position.x();
	ubo.view_position[1] = view_position.y();
	ubo.view_position[2] = view_position.z();
	std::memcpy(_ubo_mapped[frame_index], &ubo, sizeof(GlobalUbo));
}

} // namespace vre
