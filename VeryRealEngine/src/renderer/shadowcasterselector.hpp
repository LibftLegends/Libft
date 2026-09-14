/**
 * @file shadowcasterselector.hpp
 * @brief Picks which scene lights cast a shadow this frame and computes
 * each one's light-space matrix — factored out of Renderer::draw_frame()
 * to keep that file under this project's 250-line cap.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "light.hpp"
#include "../shadowpass/pass.hpp"

namespace vre
{
class ShadowCasterSelector
{
  public:
	ShadowCasterSelector();
	ShadowCasterSelector(const ShadowCasterSelector &other);
	ShadowCasterSelector &operator=(const ShadowCasterSelector &other);
	~ShadowCasterSelector();

	/**
		* @brief Selects up to ShadowPass::kMaxShadowCasters shadow-casting
		* lights from `lights` — falling back to one default directional
		* light if `lights` is empty, so the scene shows *something*
		* rather than aborting or rendering fully black — and computes
		* each one's light-space matrix via `shadow_pass`. Any remaining
		* (unused) slots in `out_light_space_matrices` are set to identity.
		* @param shadow_pass Used to compute each caster's light-space matrix.
		* @param lights Active scene lights; the first
		* min(kMaxShadowCasters, lights.size()) cast a shadow.
		* @param out_light_space_matrices Receives
		* ShadowPass::kMaxShadowCasters entries.
		* @return How many entries of `out_light_space_matrices` are
		* active casters (the rest are identity).
		*/
	static uint32_t select(const ShadowPass &shadow_pass,
		const std::vector<Light> &lights, mat4 *out_light_space_matrices);
};

} // namespace vre
