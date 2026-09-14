#include "transformcomposer.hpp"

namespace vre
{
TransformComposer::TransformComposer()
{
}

TransformComposer::~TransformComposer()
{
}

mat4 TransformComposer::compose_local(
	const TransformComponent &transform) const
{
	vec3 total_rotation = transform.rotation() + transform.spin_accumulated();
	return (mat4::compose(
		transform.position(), total_rotation, transform.scale()));
}

} // namespace vre
