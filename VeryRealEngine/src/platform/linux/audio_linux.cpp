#include "audio_linux.hpp"

#include <cstdio>
#include <vector>

namespace vre
{

AudioLinux::AudioLinux() = default;

AudioLinux::~AudioLinux()
{
    destroy();
}

bool AudioLinux::initialize()
{
    int result = snd_pcm_open(&_pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (result < 0)
    {
        // Not fatal — see audio_system.hpp's initialize() doc comment: no
        // audio device (a headless CI box, a sandboxed session with no
        // sound permission, ...) means the engine runs silently, not that
        // it aborts.
        std::fprintf(stderr, "AudioLinux: snd_pcm_open failed: %s (running without audio)\n",
            snd_strerror(result));
        _pcm_handle = nullptr;
        return false;
    }

    snd_pcm_hw_params_t *hw_params = nullptr;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(_pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(_pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(_pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(_pcm_handle, hw_params, 2);

    unsigned int rate = _sample_rate;
    snd_pcm_hw_params_set_rate_near(_pcm_handle, hw_params, &rate, nullptr);
    _sample_rate = rate; // ALSA may have picked the nearest rate the device actually supports

    // A ~20ms period is a reasonable balance: small enough that play()/stop()
    // feel responsive (a UI click shouldn't wait 200ms to start), large
    // enough that the writer thread isn't waking up so often it becomes a
    // scheduling burden.
    snd_pcm_uframes_t period_frames = _sample_rate / 50;
    snd_pcm_hw_params_set_period_size_near(_pcm_handle, hw_params, &period_frames, nullptr);

    result = snd_pcm_hw_params(_pcm_handle, hw_params);
    if (result < 0)
    {
        std::fprintf(stderr, "AudioLinux: snd_pcm_hw_params failed: %s (running without audio)\n",
            snd_strerror(result));
        snd_pcm_close(_pcm_handle);
        _pcm_handle = nullptr;
        return false;
    }

    snd_pcm_hw_params_get_period_size(hw_params, &_period_frames, nullptr);
    if (_period_frames == 0)
        _period_frames = period_frames;

    result = snd_pcm_prepare(_pcm_handle);
    if (result < 0)
    {
        std::fprintf(stderr, "AudioLinux: snd_pcm_prepare failed: %s (running without audio)\n",
            snd_strerror(result));
        snd_pcm_close(_pcm_handle);
        _pcm_handle = nullptr;
        return false;
    }

    _running = true;
    _writer_thread = std::thread(&AudioLinux::writer_thread_main, this);
    return true;
}

void AudioLinux::destroy()
{
    _running = false;
    if (_writer_thread.joinable())
        _writer_thread.join();

    if (_pcm_handle != nullptr)
    {
        snd_pcm_close(_pcm_handle);
        _pcm_handle = nullptr;

        // ALSA's own config parser (invoked internally by snd_pcm_open, to
        // resolve "default" through its plugin chain — on this system,
        // ultimately into libpipewire's ALSA compatibility plugin) caches
        // parsed global config in memory that snd_pcm_close does not free;
        // it's reclaimed by the OS at process exit either way, but shows up
        // as a real (if externally-caused) leak under ASan/valgrind — a
        // well-documented ALSA quirk, not a bug in this file. This is the
        // library's own documented fix, not a workaround invented here.
        snd_config_update_free_global();
    }
}

void AudioLinux::writer_thread_main()
{
    std::vector<int16_t> period_buffer(_period_frames * 2); // stereo, interleaved

    while (_running.load())
    {
        _mixer.mix(period_buffer.data(), _period_frames, _sample_rate);

        snd_pcm_sframes_t written = snd_pcm_writei(_pcm_handle, period_buffer.data(),
            _period_frames);
        if (written < 0)
        {
            // An underrun (writer thread fell behind the device) is
            // recoverable with snd_pcm_recover rather than a fatal
            // condition — the subject's "no unexpected termination"
            // requirement applies to audio glitches too, not just crashes.
            written = snd_pcm_recover(_pcm_handle, static_cast<int>(written), 0);
            if (written < 0)
            {
                std::fprintf(stderr, "AudioLinux: snd_pcm_writei unrecoverable: %s\n",
                    snd_strerror(static_cast<int>(written)));
                break;
            }
        }
    }
}

AudioSystem *AudioSystem::create()
{
    return new AudioLinux();
}

} // namespace vre
