/**
 * @file mixer.hpp
 * @brief Platform-independent software audio mixer: owns loaded WAV clips
 * and a fixed pool of playing "voices", and renders them down to an
 * interleaved 16-bit PCM buffer on demand.
 *
 * Deliberately separated from audiolinux.cpp's ALSA glue: everything here
 * (voice bookkeeping, per-voice resampling, mono→stereo, clipping) is the
 * same regardless of which OS API actually pushes the resulting samples to
 * a sound card, so a future backend (CoreAudio, WASAPI) only needs to open
 * a device and periodically call mix() — see audiosystem.hpp's own header
 * comment.
 */
#pragma once

#include "../vre.hpp"
#include "soundhandle.hpp"
#include "voicehandle.hpp"
#include "wavclip.hpp"

namespace vre
{
/**
 * @brief The mixing engine itself, independent of how its output reaches a
 * speaker. Thread-safe: play()/stop()/mix() are all safe to call from
 * different threads (the render/main thread calls play()/stop() in
 * response to gameplay events, a dedicated audio thread calls mix()).
 */
class Mixer
{
  public:
	Mixer();
	~Mixer();

	/** Loads a WAV file, returning a handle usable with play(). See
	 * AudioSystem::load_sound. */
	SoundHandle load_sound(const char *path);

	/// Starts a voice; see AudioSystem::play for parameter semantics.
	VoiceHandle play(SoundHandle sound, bool loop, float volume);
	/// Stops one voice immediately.
	void stop(VoiceHandle voice);
	/// Stops every voice immediately.
	void stop_all();

	/// See AudioSystem::set_master_volume.
	void set_master_volume(float volume);

	/**
		* @brief Renders `frame_count` stereo frames into `out`, advancing
		* every active voice and deactivating any non-looping voice that
		* reaches its end.
		* @param out Destination buffer for interleaved L/R 16-bit samples;
		* must hold at least `frame_count * 2` int16_t values.
		* @param frame_count How many stereo frames to render.
		* @param out_sample_rate The output device's actual sample rate —
		* each voice is resampled from its own clip's rate to this one, so
		* clips authored at any common rate (44100, 48000, ...) play back
		* at the correct pitch/speed regardless of the device's rate.
		*/
	void mix(int16_t *out, size_t frame_count, uint32_t out_sample_rate);

  private:
	/** Fixed pool size: how many sounds can play at once (background loop(s) + a
	 * handful of simultaneous one-shots is all this engine's demo ever needs). */
	static constexpr uint32_t kMaxVoices = 32;

	// Owns a std::mutex, which the standard library itself makes
	// non-copyable — the pre-C++11 idiom of a private, never-defined
	// copy constructor/assignment operator (this project avoids
	// `= delete`), same as Registry's.
	Mixer(const Mixer &other);
	Mixer &operator=(const Mixer &other);

	struct			Voice
	{
		bool		active = false;
		SoundHandle	sound = SoundHandle::invalid();
		/** In source frames (fractional, for resampling). */
		double source_position = 0.0;
		bool		loop = false;
		float		volume = 1.0f;
	};

	std::vector<WavClip> _clips;
	// VoiceHandle is just this array's index (no generation/ABA counter):
	// a documented simplification — stop() on a stale handle either hits
	// an already-inactive slot (a harmless no-op) or, in the rare case a
	// very-short-lived voice was fully recycled in between, stops
	// whatever new voice now occupies that slot instead. Acceptable for
	// this engine's actual usage (a handful of concurrent UI/ambience
	// sounds, not hundreds of overlapping short-lived ones).
	Voice			_voices[kMaxVoices];
	std::mutex _mutex; ///< Guards _clips (append-only after load) and _voices.
	float			_master_volume;
};

} // namespace vre
