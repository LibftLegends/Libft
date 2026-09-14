/**
 * @file pendulumrig.hpp
 * @brief The house demo's skeletal-animation bonus prop (Chapter VII): a
 * small hanging pendulum lamp, GPU-skinned via Renderer::draw_frame()'s
 * bone_matrices parameter.
 */
#pragma once

#include "../animation/animationclip.hpp"
#include "../animation/animator.hpp"
#include "../animation/skeleton.hpp"
#include "../math/mat4.hpp"
#include "../renderer/renderitem.hpp"
#include "../renderer/renderer.hpp"
#include "../vre.hpp"

namespace vre
{
class PendulumRig
{
  public:
	PendulumRig();
	~PendulumRig();

	/**
		* @brief Loads the skinned rig through `renderer` and places it at
		* `position`.
		* @param renderer Renderer to upload the rig's geometry through.
		* @param path Filesystem path to a `.skinnedmesh.json` file.
		* @param position World-space position the rig hangs from.
		*/
	void load(Renderer *renderer, const char *path, const vec3 &position);

	/// Advances the animation clock by `delta_seconds`.
	void update(float delta_seconds);

	/// @return This frame's per-bone skinning matrices, for
	/// draw_frame()'s bone_matrices parameter.
	void collect_bone_matrices(std::vector<mat4> *out_matrices) const;

	/// @return A RenderItem drawing the rig at its current position.
	RenderItem render_item() const;

  private:
	// Owns an Animator holding pointers into _skeleton/_clip — copying
	// this object would leave those pointers aliasing the wrong instance,
	// so, like Renderer/Scene, it gets the pre-C++11 idiom of a private,
	// never-defined copy constructor/assignment operator (this project
	// avoids `= delete`).
	PendulumRig(const PendulumRig &other);
	PendulumRig &operator=(const PendulumRig &other);

	Skeleton _skeleton;
	AnimationClip _clip;
	Animator _animator;
	MeshHandle _mesh;
	mat4 _model;
};

} // namespace vre
