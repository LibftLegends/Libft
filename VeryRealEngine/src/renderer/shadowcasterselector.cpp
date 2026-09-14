#include "shadowcasterselector.hpp"

namespace vre
{
ShadowCasterSelector::ShadowCasterSelector()
{
}

ShadowCasterSelector::ShadowCasterSelector(const ShadowCasterSelector &)
{
}

ShadowCasterSelector &ShadowCasterSelector::operator=(
	const ShadowCasterSelector &)
{
	return (*this);
}

ShadowCasterSelector::~ShadowCasterSelector()
{
}

uint32_t ShadowCasterSelector::select(const ShadowPass &shadow_pass,
	const std::vector<Light> &lights, mat4 *out_light_space_matrices)
{
	Light		default_shadow_caster;
	uint32_t	shadow_caster_count;

	default_shadow_caster.set_type(Light::Type::Directional);
	default_shadow_caster.set_direction_or_position(vec3(-0.4f, -1.0f, -0.3f));
	default_shadow_caster.set_color(vec3(1.0f, 1.0f, 1.0f));
	default_shadow_caster.set_intensity(1.0f);
	shadow_caster_count = lights.empty() ? 1
		: std::min(static_cast<uint32_t>(lights.size()),
			ShadowPass::kMaxShadowCasters);
	for (uint32_t i = 0; i < shadow_caster_count; i++)
	{
		const Light &caster = lights.empty() ? default_shadow_caster : lights[i];
		out_light_space_matrices[i] =
			shadow_pass.compute_light_space_matrix(caster);
	}
	for (uint32_t i = shadow_caster_count; i < ShadowPass::kMaxShadowCasters;
		i++)
		out_light_space_matrices[i] = mat4::identity();
	return (shadow_caster_count);
}

} // namespace vre
