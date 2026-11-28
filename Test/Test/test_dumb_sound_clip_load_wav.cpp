#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/DUMB/dumb_io.hpp"
#include "../../Modules/DUMB/sound_device.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstdio>

static constexpr char PCM_WAVE[] = {
    'R', 'I', 'F', 'F', 54, 0, 0, 0, 'W', 'A', 'V', 'E',
    'J', 'U', 'N', 'K', 5, 0, 0, 0, 'd', 'a', 't', 'a', 'X', 0,
    'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
    17, 43, 0, 0, 34, 86, 0, 0, 2, 0, 16, 0,
    'd', 'a', 't', 'a', 4, 0, 0, 0, 1, 0, 2, 0
};

static int32_t test_dumb_load_wave(ft_sound_clip &clip, const char *bytes, ft_size_t size)
{
    char path[128];
    std::snprintf(path, sizeof(path), "build_logs/dumb_audio_" FT_UINT64_DECIMAL_FORMAT ".wav",
        cmp_get_process_id());
    const int32_t written = ft_write_file(path, bytes, size);
    if (written != FT_ERR_SUCCESS)
        return (written);
    const int32_t loaded = clip.load_wav(path);
    (void)std::remove(path);
    return (loaded);
}

FT_TEST(test_dumb_sound_clip_load_wav_skips_padded_chunks)
{
    ft_sound_clip clip;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clip.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_dumb_load_wave(clip, PCM_WAVE, sizeof(PCM_WAVE)));
    FT_ASSERT_EQ(11025, clip.get_spec()->freq);
    FT_ASSERT_EQ(1, clip.get_spec()->channels);
    FT_ASSERT_EQ(4U, clip.get_size());
    const uint8_t *samples = clip.get_data();
    FT_ASSERT_EQ(1U, samples[0]);
    FT_ASSERT_EQ(0U, samples[1]);
    FT_ASSERT_EQ(2U, samples[2]);
    FT_ASSERT_EQ(0U, samples[3]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_dumb_load_wave(clip, PCM_WAVE, sizeof(PCM_WAVE)));
    FT_ASSERT(samples == clip.get_data());
    return (1);
}

FT_TEST(test_dumb_sound_clip_load_wav_rejects_truncated_or_unsupported_reloads)
{
    ft_sound_clip clip;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clip.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_dumb_load_wave(clip, PCM_WAVE, sizeof(PCM_WAVE)));

    char invalid[sizeof(PCM_WAVE)];
    (void)ft_memcpy(invalid, PCM_WAVE, sizeof(invalid));
    invalid[54] = 100;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, test_dumb_load_wave(clip, invalid, sizeof(invalid)));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, test_dumb_load_wave(clip, PCM_WAVE, sizeof(PCM_WAVE) - 1));
    (void)ft_memcpy(invalid, PCM_WAVE, sizeof(invalid));
    invalid[48] = 8;
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE, test_dumb_load_wave(clip, invalid, sizeof(invalid)));
    FT_ASSERT_EQ(11025, clip.get_spec()->freq);
    FT_ASSERT_EQ(1, clip.get_spec()->channels);
    FT_ASSERT_EQ(4U, clip.get_size());
    FT_ASSERT_EQ(2U, clip.get_data()[2]);
    return (1);
}

FT_TEST(test_dumb_sound_clip_skips_metadata_larger_than_allocation_limit)
{
    constexpr ft_size_t METADATA_SIZE = 4096;
    constexpr ft_size_t ORIGINAL_FORMAT_OFFSET = 26;
    constexpr ft_size_t FORMAT_OFFSET = 20 + METADATA_SIZE;
    char wave[FORMAT_OFFSET + sizeof(PCM_WAVE) - ORIGINAL_FORMAT_OFFSET] = {};
    (void)ft_memcpy(wave, PCM_WAVE, 20);
    wave[4] = 48;
    wave[5] = 16;
    wave[16] = 0;
    wave[17] = 16;
    (void)ft_memcpy(wave + FORMAT_OFFSET, PCM_WAVE + ORIGINAL_FORMAT_OFFSET,
        sizeof(PCM_WAVE) - ORIGINAL_FORMAT_OFFSET);

    ft_sound_clip clip;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clip.initialize());
    cma_set_alloc_limit(128);
    const int32_t result = test_dumb_load_wave(clip, wave, sizeof(wave));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(4U, clip.get_size());
    FT_ASSERT_EQ(2U, clip.get_data()[2]);
    return (1);
}
