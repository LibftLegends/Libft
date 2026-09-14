/**
 * @file shadowlightmatrix.hpp
 * @brief Computes a shadow caster's light-space (view*projection) matrix —
 * pure math, factored out of ShadowPass so that class stays focused on
 * owning GPU resources and recording draws.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "light.hpp"

namespace vre
{
class ShadowLightMatrix
{
  public:
	ShadowLightMatrix();
	ShadowLightMatrix(const ShadowLightMatrix &other);
	ShadowLightMatrix &operator=(const ShadowLightMatrix &other);
	~ShadowLightMatrix();

	/**
		* @brief Computes `shadow_caster`'s light-space (view*projection) matrix.
		*
		* The engine doesn't compute a dynamic scene-bounds AABB yet (that's
		* future work alongside a general culling system) — the fixed
		* box/distance this uses is sized generously for the demo scene in
		* assets/scenes/demo_scene.json.
		*
		* A point light's shadow really needs an omnidirectional cubemap (6
		* faces) to cover every direction; this uses a single perspective
		* frustum aimed at a fixed scene center instead, which is exact for
		* whatever it covers but won't shadow anything outside that cone —
		* an explicit, documented scope limitation.
		*/
	static mat4 compute(const Light &shadow_caster);
};

} // namespace vre
