/**
 * @file null_audio_system.cpp
 * @brief No-op AudioSystem::create() for platforms without a real backend
 * yet (currently: macOS — see Makefile). Loads sounds (so callers can hold
 * valid-looking handles and main.cpp's logic doesn't need `#ifdef`s), but
 * initialize() returns false and play() is always a silent no-op.
 *
 * This exists so a build on a platform without CoreAudio/WASAPI glue still
 * links and runs correctly (silently, without sound) rather than failing to
 * link at all — the same "missing hardware/feature degrades gracefully"
 * principle audio_linux.cpp applies when ALSA itself has no device to open.
 */
#include "audio_system.hpp"
#include "mixer.hpp"

namespace vre
{

namespace
{

class NullAudioSystem : public AudioSystem
{
    public:
        bool initialize() override { return false; }
        void destroy() override {}

        SoundHandle load_sound(const char *path) override { return _mixer.load_sound(path); }
        VoiceHandle play(SoundHandle, bool, float) override { return kInvalidVoice; }
        void stop(VoiceHandle) override {}
        void stop_all() override {}
        void set_master_volume(float) override {}

    private:
        Mixer _mixer; ///< Unused for playback here, just gives load_sound() something real to do.
};

} // namespace

AudioSystem *AudioSystem::create()
{
    return new NullAudioSystem();
}

} // namespace vre
