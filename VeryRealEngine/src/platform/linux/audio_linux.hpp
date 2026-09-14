/**
 * @file audio_linux.hpp
 * @brief Linux audio backend: ALSA (`libasound`) direct PCM playback — the
 * same category of "system library" the subject names XCB as an example of
 * (ALSA ships with the Linux kernel/userspace stack, not a third-party
 * dependency), same spirit as window_linux.hpp's choice of plain Xlib.
 *
 * Owns a dedicated writer thread: ALSA's blocking `snd_pcm_writei` would
 * otherwise have to be called from somewhere, and calling it from the
 * render loop would make every frame's timing depend on the sound card
 * keeping up, which is exactly the kind of coupling a separate thread
 * avoids.
 */
#pragma once

#include "../../audio/audio_system.hpp"
#include "../../audio/mixer.hpp"
#include "../../vre.hpp"

namespace vre
{

/// ALSA-backed implementation of AudioSystem.
class AudioLinux : public AudioSystem
{
  public:
	AudioLinux();
	~AudioLinux() override;

	bool initialize() override;
	void destroy() override;

	SoundHandle load_sound(const char *path) override;
	VoiceHandle play(SoundHandle sound, bool loop, float volume) override;
	void stop(VoiceHandle voice) override;
	void stop_all() override;
	void set_master_volume(float volume) override;

  private:
	// Owns a Mixer/std::thread/std::atomic, none of which the standard
	// library allows to be copied — the pre-C++11 idiom of a private,
	// never-defined copy constructor/assignment operator (this project
	// avoids `= delete`).
	AudioLinux(const AudioLinux &other);
	AudioLinux &operator=(const AudioLinux &other);

	/// Writer-thread body: repeatedly mixes a period's worth of frames and writes them to ALSA.
	void writer_thread_main();

	snd_pcm_t *_pcm_handle;
	Mixer _mixer;
	std::thread _writer_thread;
	std::atomic<bool> _running;
	uint32_t _sample_rate;
	snd_pcm_uframes_t _period_frames;
};

} // namespace vre
