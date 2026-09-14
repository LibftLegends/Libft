/**
 * @file lightswitchinteraction.hpp
 * @brief The house demo's Room B light switch: proximity-triggered on/off,
 * driving one scene light's intensity by index.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../platform/window.hpp"
#include "../scene/scene.hpp"
#include "../vre.hpp"
#include "demoaudio.hpp"

namespace vre
{
class LightSwitchInteraction
{
  public:
	static constexpr float kInteractRadius = 1.6f;
	static constexpr float kOffIntensity = 0.05f;
	static constexpr float kOnIntensity = 4.0f;
	static constexpr size_t kLightIndex = 1;

	LightSwitchInteraction();
	LightSwitchInteraction(const LightSwitchInteraction &other);
	LightSwitchInteraction &operator=(const LightSwitchInteraction &other);
	~LightSwitchInteraction();

	bool is_on() const;
	/// Debug override: sets the on/off state without the F-key toggle.
	void set_on(bool value);
	/// Writes the current on/off state's intensity into `scene`.
	void apply_intensity(Scene *scene) const;

	/**
		* @brief Toggles on/off when F is pressed within kInteractRadius of
		* the "light_switch" node, and writes the new intensity into `scene`.
		*/
	void update(Scene *scene, Window *window, const vec3 &camera_position,
		DemoAudio *audio);

  private:
	bool _on;
};

} // namespace vre
