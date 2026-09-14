/**
 * @file doorinteraction.hpp
 * @brief The house demo's door: proximity-triggered open/close, eased
 * toward its target angle and mirrored into the "door_hinge" scene node.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../platform/window.hpp"
#include "../scene/scene.hpp"
#include "../vre.hpp"
#include "demoaudio.hpp"

namespace vre
{
class DoorInteraction
{
  public:
	static constexpr float kInteractRadius = 1.6f;

	DoorInteraction();
	DoorInteraction(const DoorInteraction &other);
	DoorInteraction &operator=(const DoorInteraction &other);
	~DoorInteraction();

	bool is_open() const;
	/// Debug override: sets the fully-open angle used by force_open()
	/// and update().
	void set_open_angle(float value);
	/// Debug override: opens the door immediately, skipping the lerp animation.
	void force_open();

	/**
		* @brief Toggles open/closed when E is pressed within
		* kInteractRadius of the "door_hinge" node, eases the angle toward
		* its target, and writes the result back into `scene`.
		*/
	void update(float delta_seconds, Scene *scene, Window *window,
		const vec3 &camera_position, DemoAudio *audio);

  private:
	bool _open;
	float _angle;
	float _open_angle;
};

} // namespace vre
