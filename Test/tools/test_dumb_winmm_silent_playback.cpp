#include "../../Modules/Compatebility/compatebility_sound_device_win32.hpp"
#include <cstdio>

#if !defined(_WIN32) || !defined(LIBFT_TEST_BUILD)
# error "Build this check on Windows with LIBFT_TEST_BUILD and LIBFT_INTERNAL_HEADERS."
#endif

int main(int32_t argument_count, char **arguments)
{
    if (argument_count != 2 || waveOutGetNumDevs() == 0)
        return (2);
    ft_sound_clip clip;
    if (clip.initialize() != FT_ERR_SUCCESS || clip.load_wav(arguments[1]) != FT_ERR_SUCCESS)
        return (3);
    const ft_size_t sample_size = clip.get_size();
    if (sample_size < 1024)
        return (3);
    ft_size_t index = 0;
    while (index < sample_size)
    {
        if (clip.get_data()[index] != 0)
            return (3);
        ++index;
    }

    ft_sound_device_win32 device;
    ft_sound_spec spec = *clip.get_spec();
    spec.samples = 256;
    const int32_t status = device.open(&spec);
    if (status != FT_ERR_SUCCESS)
    {
        std::printf("Silent playback open status=%d\n", status);
        return (4);
    }
    device.play(&clip);
    const uint64_t started = GetTickCount64();
    ft_bool drained = FT_FALSE;
    while (GetTickCount64() - started < 1500)
    {
        AcquireSRWLockExclusive(&device._playback_lock);
        drained = device._playback_pos == sample_size
            && !device._queued[0] && !device._queued[1];
        ReleaseSRWLockExclusive(&device._playback_lock);
        if (drained)
            break ;
        Sleep(1);
    }
    device.close();
    if (!drained || device._wave_out != ft_nullptr)
        return (5);
    std::printf("Silent WinMM clip drained and closed successfully\n");
    return (0);
}
