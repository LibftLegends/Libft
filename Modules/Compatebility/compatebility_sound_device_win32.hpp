#ifndef COMPATEBILITY_SOUND_DEVICE_WIN32_HPP
#define COMPATEBILITY_SOUND_DEVICE_WIN32_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#if defined(_WIN32)

#include "../DUMB/sound_device.hpp"
#include "../DUMB/sound_clip.hpp"
#define WIN32_LEAN_AND_MEAN
#include <mmsystem.h>
#include <windows.h>

class ft_sound_device_win32 : public ft_sound_device
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        HWAVEOUT                _wave_out;
        WAVEHDR                 *_headers;
        uint8_t                 *_buffer;
        DWORD                   _buffer_capacity;
        uint32_t                _prepared_headers;
        ft_bool                 _queued[2];
        HANDLE                  _completion_event;
        HANDLE                  _stop_event;
        HANDLE                  _worker;
        SRWLOCK                 _playback_lock;
        ft_bool                 _closing;
        const uint8_t           *_samples;
        ft_size_t               _sample_size;
        ft_size_t               _playback_pos;

        static DWORD WINAPI playback_worker(void *argument);
        int32_t open_device(const ft_sound_spec &spec, DWORD buffer_capacity);
        void refill_completed_buffers(void);
        void submit_buffer(uint32_t index);
        ft_bool reset_playback(void);
        void release_device(void);

    public:
        ft_sound_device_win32(void);
        ft_sound_device_win32(const ft_sound_device_win32 &other);
        ft_sound_device_win32(ft_sound_device_win32 &&other);
        ft_sound_device_win32 &operator=(const ft_sound_device_win32 &other) = delete;
        ft_sound_device_win32 &operator=(ft_sound_device_win32 &&other) = delete;
        ~ft_sound_device_win32(void);

        int32_t open(const ft_sound_spec *spec);
        void close(void);
        void pause(int32_t pause_on);
        void play(const ft_sound_clip *clip);
        void stop(void);
};

#endif

#endif
