/**
 * @file audio_system.hpp
 * @brief Abstract platform audio-output interface, mirroring
 * ../platform/window.hpp's shape: one small backend per OS behind a common
 * interface (ALSA on Linux; see src/platform/linux/audio_linux.hpp), plus a
 * platform-independent WAV loader and software mixer built on top of it.
 *
 * This is the engine's bonus "Sound System" (Chapter VII): background
 * music/ambience playback plus one-shot sound effects, both driven from a
 * hand-written WAVE loader (wav_loader.hpp) — no third-party audio library.
 */
#pragma once

#include <cstdint>

namespace vre
{

/// Opaque handle to a loaded sound clip (returned by AudioSystem::load_sound).
using SoundHandle = uint32_t;
/// Sentinel meaning "no sound"/"load failed".
constexpr SoundHandle kInvalidSound = static_cast<SoundHandle>(-1);

/// Opaque handle to one playing instance of a sound (returned by AudioSystem::play).
using VoiceHandle = uint32_t;
/// Sentinel meaning "no voice"/"play failed".
constexpr VoiceHandle kInvalidVoice = static_cast<VoiceHandle>(-1);

/**
 * @brief Abstract audio backend: loads WAV clips and mixes/plays any number
 * of them concurrently (background ambience plus one-shot effects don't
 * need to interrupt each other).
 *
 * The actual mixing happens off the render thread (see audio_linux.cpp's
 * dedicated ALSA writer thread) so a slow frame never causes an audio
 * dropout, and so the render loop never blocks waiting on the sound card.
 */
class AudioSystem
{
    public:
        virtual ~AudioSystem() = default;

        /**
         * @brief Opens the platform's default audio output device and
         * starts the mixer thread.
         * @return true on success. A false return is not fatal to the
         * caller — see main.cpp: the engine runs, silently, without audio
         * rather than aborting, since a missing/busy sound device is a
         * environment condition, not a programming error.
         */
        virtual bool initialize() = 0;
        /// Stops the mixer thread and closes the audio device.
        virtual void destroy() = 0;

        /**
         * @brief Loads a WAV file into memory, ready to be played any
         * number of times (and concurrently with itself — e.g. rapid
         * repeated clicks) via play().
         * @param path Filesystem path to a `.wav` file.
         * @return A handle to pass to play(), or kInvalidSound on failure
         * (missing file, unsupported codec — see wav_loader.hpp).
         */
        virtual SoundHandle load_sound(const char *path) = 0;

        /**
         * @brief Starts playing a loaded sound.
         * @param sound A handle from load_sound().
         * @param loop If true, the sound repeats indefinitely until stop()
         * is called on the returned voice (used for background ambience);
         * if false, it plays once and the voice frees itself (used for
         * one-shot effects like a door creak or a UI click).
         * @param volume Linear gain multiplier, 0.0–1.0+ (values above 1.0
         * are allowed and simply clip at the mixer, matching how the
         * subject's own bonus list treats this as a simple mixing feature,
         * not a full DSP chain).
         * @return A handle identifying this specific playing instance, or
         * kInvalidVoice if `sound` is invalid or every voice slot is busy.
         */
        virtual VoiceHandle play(SoundHandle sound, bool loop = false, float volume = 1.0f) = 0;

        /// Stops one specific playing voice immediately (no fade-out).
        virtual void stop(VoiceHandle voice) = 0;
        /// Stops every currently playing voice immediately.
        virtual void stop_all() = 0;

        /// Sets a global gain multiplier applied to every voice (default 1.0).
        virtual void set_master_volume(float volume) = 0;

        /// @return A newly constructed audio backend appropriate for the current platform.
        static AudioSystem *create();
};

} // namespace vre
