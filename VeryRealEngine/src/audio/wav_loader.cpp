#include "wav_loader.hpp"

#include <cstdio>
#include <cstring>

namespace vre
{

namespace
{

/// Reads a little-endian value of type T from a raw byte buffer at `offset`.
template <typename T>
T read_le(const uint8_t *data, size_t offset)
{
    T value;
    std::memcpy(&value, data + offset, sizeof(T));
    return value; // x86/ARM64 are both little-endian; WAVE's fields are too.
}

} // namespace

bool load_wav_file(const char *path, WavClip *out_clip)
{
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        std::fprintf(stderr, "load_wav_file: could not open \"%s\"\n", path);
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    long file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (file_size < 44) // smallest possible valid RIFF/WAVE/fmt/data header set
    {
        std::fprintf(stderr, "load_wav_file: \"%s\" is too small to be a WAVE file\n", path);
        std::fclose(file);
        return false;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(file_size));
    size_t read_count = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read_count != bytes.size())
    {
        std::fprintf(stderr, "load_wav_file: short read on \"%s\"\n", path);
        return false;
    }

    if (std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
    {
        std::fprintf(stderr, "load_wav_file: \"%s\" is not a RIFF/WAVE file\n", path);
        return false;
    }

    // Walk the chunk list looking for "fmt " and "data" — a WAVE file may
    // have other chunks (LIST, fact, ...) before/after/between them, so this
    // can't assume a fixed layout the way a minimal writer (like the one
    // that generated this project's own assets) would produce.
    uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    const uint8_t *pcm_data = nullptr;
    uint32_t pcm_data_size = 0;

    size_t offset = 12; // past "RIFF" + size + "WAVE"
    while (offset + 8 <= bytes.size())
    {
        char chunk_id[5] = {0};
        std::memcpy(chunk_id, bytes.data() + offset, 4);
        uint32_t chunk_size = read_le<uint32_t>(bytes.data(), offset + 4);
        size_t chunk_data_offset = offset + 8;

        if (chunk_data_offset + chunk_size > bytes.size())
        {
            std::fprintf(stderr, "load_wav_file: \"%s\" has a truncated \"%s\" chunk\n",
                path, chunk_id);
            return false;
        }

        if (std::strcmp(chunk_id, "fmt ") == 0 && chunk_size >= 16)
        {
            audio_format = read_le<uint16_t>(bytes.data(), chunk_data_offset + 0);
            channels = read_le<uint16_t>(bytes.data(), chunk_data_offset + 2);
            sample_rate = read_le<uint32_t>(bytes.data(), chunk_data_offset + 4);
            bits_per_sample = read_le<uint16_t>(bytes.data(), chunk_data_offset + 14);
        }
        else if (std::strcmp(chunk_id, "data") == 0)
        {
            pcm_data = bytes.data() + chunk_data_offset;
            pcm_data_size = chunk_size;
        }

        // Chunks are word-aligned: a chunk with an odd size has one byte of
        // padding after it that isn't reflected in chunk_size.
        offset = chunk_data_offset + chunk_size + (chunk_size % 2);
    }

    if (audio_format == 0 || pcm_data == nullptr)
    {
        std::fprintf(stderr, "load_wav_file: \"%s\" is missing a \"fmt \" or \"data\" chunk\n",
            path);
        return false;
    }
    if (audio_format != 1) // WAVE_FORMAT_PCM
    {
        std::fprintf(stderr,
            "load_wav_file: \"%s\" uses compressed audio format %u, only uncompressed "
            "PCM (format 1) is supported\n", path, audio_format);
        return false;
    }
    if (channels == 0 || (bits_per_sample != 8 && bits_per_sample != 16))
    {
        std::fprintf(stderr,
            "load_wav_file: \"%s\" has an unsupported channel count (%u) or bit depth "
            "(%u) — only 8-bit or 16-bit PCM is supported\n",
            path, channels, bits_per_sample);
        return false;
    }

    WavClip clip;
    clip.sample_rate = sample_rate;
    clip.channels = channels;

    if (bits_per_sample == 16)
    {
        size_t sample_count = pcm_data_size / sizeof(int16_t);
        clip.samples.resize(sample_count);
        std::memcpy(clip.samples.data(), pcm_data, sample_count * sizeof(int16_t));
    }
    else // 8-bit PCM is unsigned, centered on 128 — up-convert to signed 16-bit.
    {
        clip.samples.resize(pcm_data_size);
        for (uint32_t i = 0; i < pcm_data_size; i++)
        {
            int16_t centered = static_cast<int16_t>(pcm_data[i]) - 128;
            clip.samples[i] = static_cast<int16_t>(centered * 256);
        }
    }

    *out_clip = std::move(clip);
    return true;
}

} // namespace vre
