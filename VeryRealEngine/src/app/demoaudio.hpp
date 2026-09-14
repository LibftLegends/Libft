/**
 * @file demoaudio.hpp
 * @brief Owns the house demo's audio backend and its three loaded clips
 * (ambient hum, click, door creak) — see audiosystem.hpp's doc comment
 * for why a failed initialize() isn't fatal.
 */
#pragma once

#include "../audio/audiosystem.hpp"
#include "../audio/soundhandle.hpp"
#include "../vre.hpp"

namespace vre
{
class DemoAudio
{
  public:
	DemoAudio();
	~DemoAudio();

	/**
		* @brief Opens the audio device and loads/plays the ambient hum.
		* @return true if the device opened (see AudioSystem::initialize());
		* a false return is not fatal — the demo just runs silently.
		*/
	bool initialize();
	/// Stops the mixer thread and releases the audio backend.
	void destroy();

	/// Plays the door-creak one-shot, if the device is available.
	void play_door_creak();
	/// Plays the UI-click one-shot, if the device is available.
	void play_click();

  private:
	// Owns an AudioSystem* (platform audio device/mixer thread) — the
	// pre-C++11 idiom of a private, never-defined copy constructor/
	// assignment operator (this project avoids `= delete`).
	DemoAudio(const DemoAudio &other);
	DemoAudio &operator=(const DemoAudio &other);

	AudioSystem *_audio;
	bool _available;
	SoundHandle _ambient_sound;
	SoundHandle _click_sound;
	SoundHandle _door_creak_sound;
};

} // namespace vre
