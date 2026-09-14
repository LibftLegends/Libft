/**
 * @file transformcomposer.hpp
 * @brief Composes a TransformComponent's position/rotation/scale (plus
 * accumulated spin) into a single local-space matrix. Extracted out of
 * Scene so both the render-collection system and the world-space query
 * methods share one implementation of "what a TransformComponent means
 * as a matrix" instead of each re-deriving it.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../components/transform.hpp"

namespace vre
{
class TransformComposer
{
	public:
		TransformComposer();
		~TransformComposer();

		/// @return The local (parent-relative) matrix for `transform`,
		/// including accumulated spin.
		mat4 compose_local(const TransformComponent &transform) const;

	private:
		TransformComposer(const TransformComposer &other);
		TransformComposer &operator=(const TransformComposer &other);
};

} // namespace vre
