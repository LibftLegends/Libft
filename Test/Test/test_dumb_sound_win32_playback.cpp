#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/DUMB/sound_device.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if defined(_WIN32)
# include <atomic>
# include <mmsystem.h>

static std::atomic<uint32_t> g_test_wave_writes;
static DWORD g_test_wave_lengths[8];
static uint8_t g_test_wave_first_byte[8];
static uint32_t g_test_wave_prepares;
static uint32_t g_test_wave_unprepares;
static uint32_t g_test_wave_closes;
static uint32_t g_test_wave_resets;
static ft_bool g_test_wave_fail_second_prepare;

static void test_wave_reset_fixture()
{
    g_test_wave_writes.store(0, std::memory_order_relaxed);
    g_test_wave_prepares = 0;
    g_test_wave_unprepares = 0;
    g_test_wave_closes = 0;
    g_test_wave_resets = 0;
    g_test_wave_fail_second_prepare = FT_FALSE;
    return ;
}

static MMRESULT test_wave_open(HWAVEOUT *device, UINT_PTR, const WAVEFORMATEX *,
    DWORD_PTR event, DWORD_PTR, DWORD flags)
{
    if (event == 0 || flags != CALLBACK_EVENT)
        return (MMSYSERR_INVALFLAG);
    *device = reinterpret_cast<HWAVEOUT>(&g_test_wave_prepares);
    return (MMSYSERR_NOERROR);
}

static MMRESULT test_wave_prepare(HWAVEOUT, WAVEHDR *header, UINT)
{
    ++g_test_wave_prepares;
    if (g_test_wave_fail_second_prepare && g_test_wave_prepares == 2)
        return (MMSYSERR_NOMEM);
    header->dwFlags |= WHDR_PREPARED;
    return (MMSYSERR_NOERROR);
}

static MMRESULT test_wave_write(HWAVEOUT, WAVEHDR *header, UINT)
{
    const uint32_t index = g_test_wave_writes.load(std::memory_order_relaxed);
    if (index >= 8 || header->dwBufferLength == 0)
        return (MMSYSERR_INVALPARAM);
    g_test_wave_lengths[index] = header->dwBufferLength;
    g_test_wave_first_byte[index] = static_cast<uint8_t>(header->lpData[0]);
    header->dwFlags &= ~WHDR_DONE;
    g_test_wave_writes.store(index + 1, std::memory_order_release);
    return (MMSYSERR_NOERROR);
}

static MMRESULT test_wave_reset(HWAVEOUT)
{
    ++g_test_wave_resets;
    return (MMSYSERR_NOERROR);
}

static MMRESULT test_wave_unprepare(HWAVEOUT, WAVEHDR *, UINT)
{
    ++g_test_wave_unprepares;
    return (MMSYSERR_NOERROR);
}

static MMRESULT test_wave_close(HWAVEOUT)
{
    ++g_test_wave_closes;
    return (MMSYSERR_NOERROR);
}

# define waveOutOpen test_wave_open
# define waveOutPrepareHeader test_wave_prepare
# define waveOutWrite test_wave_write
# define waveOutReset test_wave_reset
# define waveOutUnprepareHeader test_wave_unprepare
# define waveOutClose test_wave_close
# define cmp_create_sound_device test_wave_create_device
# define ft_sound_device_win32 test_dumb_sound_device_win32
# include "../../Modules/Compatebility/Compatebility_dumb_sound_win32.cpp"
# undef waveOutOpen
# undef waveOutPrepareHeader
# undef waveOutWrite
# undef waveOutReset
# undef waveOutUnprepareHeader
# undef waveOutClose
# undef cmp_create_sound_device
# undef ft_sound_device_win32

static void test_wave_complete_buffer(test_dumb_sound_device_win32 &device, uint32_t index)
{
    AcquireSRWLockExclusive(&device._playback_lock);
    device._headers[index].dwFlags |= WHDR_DONE;
    SetEvent(device._completion_event);
    ReleaseSRWLockExclusive(&device._playback_lock);
    return ;
}

static ft_bool test_wave_wait_for_writes(uint32_t count)
{
    const uint64_t started = GetTickCount64();
    while (g_test_wave_writes.load(std::memory_order_acquire) < count)
    {
        if (GetTickCount64() - started >= 1000)
            return (FT_FALSE);
        Sleep(1);
    }
    return (FT_TRUE);
}

static ft_bool test_wave_wait_for_buffer_release(
    test_dumb_sound_device_win32 &device, uint32_t index)
{
    const uint64_t started = GetTickCount64();
    while (GetTickCount64() - started < 1000)
    {
        AcquireSRWLockExclusive(&device._playback_lock);
        const ft_bool queued = device._queued[index];
        ReleaseSRWLockExclusive(&device._playback_lock);
        if (!queued)
            return (FT_TRUE);
        Sleep(1);
    }
    return (FT_FALSE);
}

FT_TEST(test_dumb_win32_audio_refills_from_completion_event_and_reuses_capacity)
{
    test_wave_reset_fixture();
    ft_sound_clip clip;
    test_dumb_sound_device_win32 device;
    const ft_sound_spec spec = {44100, 1, 8, nullptr, nullptr};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clip.initialize());
    clip._data.resize(18);
    uint32_t index = 0;
    while (index < clip.get_size())
    {
        clip._data[index] = static_cast<uint8_t>(index);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, device.open(&spec));
    device.play(&clip);
    FT_ASSERT(test_wave_wait_for_writes(2));
    test_wave_complete_buffer(device, 0);
    FT_ASSERT(test_wave_wait_for_writes(3));
    FT_ASSERT_EQ(8U, g_test_wave_lengths[0]);
    FT_ASSERT_EQ(8U, g_test_wave_lengths[1]);
    FT_ASSERT_EQ(2U, g_test_wave_lengths[2]);
    FT_ASSERT_EQ(16U, g_test_wave_first_byte[2]);
    test_wave_complete_buffer(device, 1);
    FT_ASSERT(test_wave_wait_for_buffer_release(device, 1));
    FT_ASSERT_EQ(0U, g_test_wave_resets);

    device.stop();
    device.play(&clip);
    FT_ASSERT(test_wave_wait_for_writes(5));
    FT_ASSERT_EQ(8U, g_test_wave_lengths[3]);
    FT_ASSERT_EQ(0U, g_test_wave_first_byte[3]);
    device.close();
    FT_ASSERT_EQ(2U, g_test_wave_unprepares);
    FT_ASSERT_EQ(1U, g_test_wave_closes);
    return (1);
}

FT_TEST(test_dumb_win32_audio_prepare_failure_releases_prepared_header)
{
    test_wave_reset_fixture();
    g_test_wave_fail_second_prepare = FT_TRUE;
    test_dumb_sound_device_win32 device;
    const ft_sound_spec spec = {44100, 1, 8, nullptr, nullptr};
    FT_ASSERT_EQ(FT_ERR_IO, device.open(&spec));
    FT_ASSERT_EQ(2U, g_test_wave_prepares);
    FT_ASSERT_EQ(1U, g_test_wave_unprepares);
    FT_ASSERT_EQ(1U, g_test_wave_closes);
    FT_ASSERT(device._headers == ft_nullptr);
    FT_ASSERT(device._completion_event == ft_nullptr);
    return (1);
}
#endif
