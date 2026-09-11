#include "mixer.hpp"

#include <algorithm>
#include <cmath>

namespace vre
{

SoundHandle Mixer::load_sound(const char *path)
{
    WavClip clip;
    if (!load_wav_file(path, &clip))
        return kInvalidSound;

    std::lock_guard<std::mutex> lock(_mutex);
    _clips.push_back(std::move(clip));
    return static_cast<SoundHandle>(_clips.size() - 1);
}

VoiceHandle Mixer::play(SoundHandle sound, bool loop, float volume)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (sound == kInvalidSound || sound >= _clips.size())
        return kInvalidVoice;

    for (uint32_t i = 0; i < kMaxVoices; i++)
    {
        if (_voices[i].active)
            continue;
        _voices[i].active = true;
        _voices[i].sound = sound;
        _voices[i].source_position = 0.0;
        _voices[i].loop = loop;
        _voices[i].volume = volume;
        return static_cast<VoiceHandle>(i);
    }
    return kInvalidVoice; // every voice slot is busy
}

void Mixer::stop(VoiceHandle voice)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (voice < kMaxVoices)
        _voices[voice].active = false;
}

void Mixer::stop_all()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto &voice : _voices)
        voice.active = false;
}

void Mixer::mix(int16_t *out, size_t frame_count, uint32_t out_sample_rate)
{
    // Accumulate in float (headroom above int16 range) so several
    // simultaneously-loud voices sum correctly and only clip once, at the
    // very end, rather than each voice separately saturating.
    std::vector<float> accumulator(frame_count * 2, 0.0f); // stereo

    std::lock_guard<std::mutex> lock(_mutex);
    for (auto &voice : _voices)
    {
        if (!voice.active)
            continue;

        const WavClip &clip = _clips[voice.sound];
        if (clip.sample_rate == 0 || clip.channels == 0 || clip.frame_count() == 0)
        {
            voice.active = false;
            continue;
        }

        // Resample ratio: how many source frames one output frame advances.
        // A clip authored at a different rate than the device's output
        // plays back at the correct pitch/speed either way.
        double step = static_cast<double>(clip.sample_rate) / static_cast<double>(out_sample_rate);

        for (size_t frame = 0; frame < frame_count; frame++)
        {
            size_t total_source_frames = clip.frame_count();
            double pos = voice.source_position;
            if (pos >= static_cast<double>(total_source_frames))
            {
                if (voice.loop)
                {
                    pos = std::fmod(pos, static_cast<double>(total_source_frames));
                    voice.source_position = pos;
                }
                else
                {
                    voice.active = false;
                    break;
                }
            }

            // Linear interpolation between the two nearest source frames —
            // simple, and enough to avoid audible aliasing at the modest
            // resampling ratios this engine's own assets/device rates
            // produce (e.g. 44100 -> 44100 is exact; 22050 -> 44100 is a
            // clean 2x).
            size_t frame_index_0 = static_cast<size_t>(pos);
            size_t frame_index_1 = std::min(frame_index_0 + 1, total_source_frames - 1);
            float fraction = static_cast<float>(pos - static_cast<double>(frame_index_0));

            for (int channel = 0; channel < 2; channel++)
            {
                // Mono clips duplicate their single channel to both output
                // channels; stereo clips map channel 0/1 directly (higher
                // channel counts aren't supported — see wav_loader.hpp).
                int source_channel = (clip.channels == 1) ? 0 : channel;
                int16_t sample_0 = clip.samples[frame_index_0 * clip.channels + source_channel];
                int16_t sample_1 = clip.samples[frame_index_1 * clip.channels + source_channel];
                float sample = sample_0 + (sample_1 - sample_0) * fraction;

                accumulator[frame * 2 + channel] += sample * voice.volume * _master_volume;
            }

            voice.source_position = pos + step;
        }
    }

    for (size_t i = 0; i < frame_count * 2; i++)
    {
        float clamped = std::max(-32768.0f, std::min(32767.0f, accumulator[i]));
        out[i] = static_cast<int16_t>(clamped);
    }
}

} // namespace vre
