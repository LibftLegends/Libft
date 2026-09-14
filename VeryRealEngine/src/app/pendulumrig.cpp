#include "pendulumrig.hpp"

namespace vre
{
PendulumRig::PendulumRig() : _animator(nullptr, nullptr)
{
}

PendulumRig::~PendulumRig()
{
}

void PendulumRig::load(Renderer *renderer, const char *path,
	const vec3 &position)
{
	_mesh = renderer->load_skinned_mesh(path, &_skeleton, &_clip);
	_animator = Animator(&_skeleton, &_clip);
	_model = mat4::translate(position);
}

void PendulumRig::update(float delta_seconds)
{
	_animator.update(delta_seconds);
}

void PendulumRig::collect_bone_matrices(std::vector<mat4> *out_matrices) const
{
	_animator.compute_bone_matrices(out_matrices);
}

RenderItem PendulumRig::render_item() const
{
	return (RenderItem(_mesh, _model));
}

} // namespace vre
