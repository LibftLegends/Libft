/**
 * @file wav_loader.hpp
 * @brief From-scratch WAVE/RIFF file loader — parses uncompressed PCM audio
 * (8-bit unsigned or 16-bit signed, mono or stereo) into a flat interleaved
 * sample buffer, the same way obj_loader.hpp/tga_loader.hpp hand-parse their
 * own formats. No third-party audio library (libsndfile, dr_wav, ...).
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vre
{

/**
 * @brief Decoded PCM audio: 16-bit signed samples, interleaved by channel
 * (frame 0 = samples[0..channels-1], frame 1 = samples[channels..2*channels-1], ...).
 *
 * 8-bit source files are up-converted to 16-bit on load, so every consumer
 * (the mixer in particular) only ever deals with one sample format.
 */
struct WavClip
{
    std::vector<int16_t> samples; ///< Interleaved 16-bit PCM samples.
    uint32_t sample_rate = 0;     ///< Samples per second per channel (e.g. 44100).
    uint16_t channels = 0;        ///< 1 = mono, 2 = stereo.

    /// Total frames (one frame = one sample per channel), independent of channel count.
    size_t frame_count() const { return channels > 0 ? samples.size() / channels : 0; }
};

/**
 * @brief Loads a PCM WAVE file from disk.
 * @param path Filesystem path to a `.wav` file.
 * @param out_clip Filled with the decoded samples/format on success; left
 * untouched on failure.
 * @return true on success, false if the file is missing, isn't a valid
 * RIFF/WAVE container, or uses a codec other than uncompressed PCM
 * (`fmt` tag other than 1) — this loader intentionally does not decode
 * compressed formats (ADPCM, MP3-in-WAV, etc.), matching the subject's "no
 * non-system library" constraint: a full audio codec is exactly the kind of
 * thing a real dependency (libsndfile, ffmpeg) exists for, and is out of
 * scope for a hand-written loader.
 */
bool load_wav_file(const char *path, WavClip *out_clip);

} // namespace vre
