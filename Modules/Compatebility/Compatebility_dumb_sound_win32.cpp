#if defined(_WIN32)

#include "compatebility_internal.hpp"
#include "compatebility_sound_device_win32.hpp"
#include <new>

static constexpr uint32_t WAVE_BUFFER_COUNT = 2;

ft_sound_device_win32::ft_sound_device_win32(void)
    : _wave_out(nullptr), _headers(nullptr), _buffer(nullptr), _buffer_capacity(0),
      _prepared_headers(0), _queued{}, _completion_event(nullptr), _stop_event(nullptr),
      _worker(nullptr), _closing(FT_FALSE),
      _samples(nullptr), _sample_size(0), _playback_pos(0)
{
    InitializeSRWLock(&this->_playback_lock);
    return ;
}

ft_sound_device_win32::ft_sound_device_win32(const ft_sound_device_win32 &other)
    : ft_sound_device_win32()
{
    (void)other;
    return ;
}

ft_sound_device_win32::ft_sound_device_win32(ft_sound_device_win32 &&other)
    : ft_sound_device_win32()
{
    (void)other;
    return ;
}

ft_sound_device_win32::~ft_sound_device_win32(void)
{
    this->close();
    return ;
}

DWORD WINAPI ft_sound_device_win32::playback_worker(void *argument)
{
    auto *device = static_cast<ft_sound_device_win32 *>(argument);
    const HANDLE events[] = {device->_stop_event, device->_completion_event};
    while (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1)
    {
        AcquireSRWLockExclusive(&device->_playback_lock);
        ResetEvent(device->_completion_event);
        if (!device->_closing)
            device->refill_completed_buffers();
        ReleaseSRWLockExclusive(&device->_playback_lock);
    }
    return (0);
}

void ft_sound_device_win32::refill_completed_buffers(void)
{
    uint32_t index = 0;
    while (index < WAVE_BUFFER_COUNT)
    {
        if (this->_queued[index] && (this->_headers[index].dwFlags & WHDR_DONE) != 0)
        {
            this->_queued[index] = FT_FALSE;
            this->submit_buffer(index);
        }
        ++index;
    }
    return ;
}

void ft_sound_device_win32::submit_buffer(uint32_t index)
{
    if (this->_samples == ft_nullptr)
        return ;
    WAVEHDR &header = this->_headers[index];
    ft_size_t copy_size = this->_sample_size - this->_playback_pos;
    if (copy_size > this->_buffer_capacity)
        copy_size = this->_buffer_capacity;
    (void)ft_memcpy(header.lpData, this->_samples + this->_playback_pos, copy_size);
    header.dwBufferLength = static_cast<DWORD>(copy_size);
    if (waveOutWrite(this->_wave_out, &header, sizeof(header)) != MMSYSERR_NOERROR)
    {
        this->_samples = ft_nullptr;
        return ;
    }
    this->_queued[index] = FT_TRUE;
    this->_playback_pos += copy_size;
    if (this->_playback_pos == this->_sample_size)
        this->_samples = ft_nullptr;
    return ;
}

int32_t ft_sound_device_win32::open_device(const ft_sound_spec &spec, DWORD buffer_capacity)
{
    this->_completion_event = CreateEventA(ft_nullptr, TRUE, FALSE, ft_nullptr);
    this->_stop_event = CreateEventA(ft_nullptr, TRUE, FALSE, ft_nullptr);
    if (this->_completion_event == ft_nullptr || this->_stop_event == ft_nullptr)
        return (FT_ERR_IO);
    this->_headers = new (std::nothrow) WAVEHDR[WAVE_BUFFER_COUNT]();
    this->_buffer = new (std::nothrow) uint8_t[static_cast<ft_size_t>(buffer_capacity) * WAVE_BUFFER_COUNT];
    if (this->_headers == ft_nullptr || this->_buffer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    this->_buffer_capacity = buffer_capacity;

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(spec.channels);
    format.nSamplesPerSec = static_cast<DWORD>(spec.freq);
    format.nBlockAlign = static_cast<WORD>(spec.channels * 2);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.wBitsPerSample = 16;
    HWAVEOUT wave_out = ft_nullptr;
    if (waveOutOpen(&wave_out, WAVE_MAPPER, &format,
            reinterpret_cast<DWORD_PTR>(this->_completion_event), 0, CALLBACK_EVENT)
        != MMSYSERR_NOERROR)
        return (FT_ERR_IO);
    this->_wave_out = wave_out;
    while (this->_prepared_headers < WAVE_BUFFER_COUNT)
    {
        WAVEHDR &header = this->_headers[this->_prepared_headers];
        header.lpData = reinterpret_cast<LPSTR>(this->_buffer
            + static_cast<ft_size_t>(this->_prepared_headers) * this->_buffer_capacity);
        header.dwBufferLength = this->_buffer_capacity;
        if (waveOutPrepareHeader(this->_wave_out, &header, sizeof(header)) != MMSYSERR_NOERROR)
            return (FT_ERR_IO);
        ++this->_prepared_headers;
    }
    ResetEvent(this->_completion_event);
    this->_worker = CreateThread(ft_nullptr, 0, playback_worker, this, 0, ft_nullptr);
    if (this->_worker == ft_nullptr)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

int32_t ft_sound_device_win32::open(const ft_sound_spec *spec)
{
    if (spec == ft_nullptr || spec->freq <= 0 || spec->channels <= 0 || spec->samples <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t frame_bytes = static_cast<ft_size_t>(spec->channels) * 2;
    const ft_size_t frames = (static_cast<ft_size_t>(spec->samples) + 1) / WAVE_BUFFER_COUNT;
    const ft_size_t buffer_capacity = frame_bytes * frames;
    if (frame_bytes > UINT16_MAX || frame_bytes * static_cast<ft_size_t>(spec->freq) > UINT32_MAX
        || buffer_capacity > UINT32_MAX || buffer_capacity > SIZE_MAX / WAVE_BUFFER_COUNT)
        return (FT_ERR_OUT_OF_RANGE);

    AcquireSRWLockExclusive(&this->_playback_lock);
    int32_t status = FT_ERR_INVALID_STATE;
    if (this->_wave_out == ft_nullptr && !this->_closing)
    {
        status = this->open_device(*spec, static_cast<DWORD>(buffer_capacity));
        if (status != FT_ERR_SUCCESS)
            this->release_device();
    }
    ReleaseSRWLockExclusive(&this->_playback_lock);
    return (status);
}

ft_bool ft_sound_device_win32::reset_playback(void)
{
    this->_samples = ft_nullptr;
    this->_sample_size = 0;
    this->_playback_pos = 0;
    if (!this->_queued[0] && !this->_queued[1])
        return (FT_TRUE);
    if (waveOutReset(this->_wave_out) != MMSYSERR_NOERROR)
        return (FT_FALSE);
    this->_queued[0] = FT_FALSE;
    this->_queued[1] = FT_FALSE;
    return (FT_TRUE);
}

void ft_sound_device_win32::release_device(void)
{
    if (this->_wave_out != ft_nullptr)
    {
        if (!this->reset_playback())
            return ;
        while (this->_prepared_headers != 0)
        {
            WAVEHDR &header = this->_headers[this->_prepared_headers - 1];
            if (waveOutUnprepareHeader(this->_wave_out, &header, sizeof(header))
                != MMSYSERR_NOERROR)
                return ;
            --this->_prepared_headers;
        }
        if (waveOutClose(this->_wave_out) != MMSYSERR_NOERROR)
            return ;
        this->_wave_out = ft_nullptr;
    }
    if (this->_stop_event != ft_nullptr)
    {
        CloseHandle(this->_stop_event);
        this->_stop_event = ft_nullptr;
    }
    if (this->_completion_event != ft_nullptr)
    {
        CloseHandle(this->_completion_event);
        this->_completion_event = ft_nullptr;
    }
    delete[] this->_buffer;
    delete[] this->_headers;
    this->_buffer = ft_nullptr;
    this->_headers = ft_nullptr;
    this->_buffer_capacity = 0;
    return ;
}

void ft_sound_device_win32::close(void)
{
    AcquireSRWLockExclusive(&this->_playback_lock);
    if (this->_closing)
    {
        ReleaseSRWLockExclusive(&this->_playback_lock);
        return ;
    }
    this->_closing = FT_TRUE;
    const HANDLE worker = this->_worker;
    if (worker != ft_nullptr)
        SetEvent(this->_stop_event);
    ReleaseSRWLockExclusive(&this->_playback_lock);

    if (worker != ft_nullptr)
        WaitForSingleObject(worker, INFINITE);
    AcquireSRWLockExclusive(&this->_playback_lock);
    if (worker != ft_nullptr)
        CloseHandle(worker);
    this->_worker = ft_nullptr;
    this->release_device();
    this->_closing = FT_FALSE;
    ReleaseSRWLockExclusive(&this->_playback_lock);
    return ;
}

void ft_sound_device_win32::pause(int32_t pause_on)
{
    AcquireSRWLockExclusive(&this->_playback_lock);
    if (this->_worker != ft_nullptr && !this->_closing)
    {
        if (pause_on)
            waveOutPause(this->_wave_out);
        else
            waveOutRestart(this->_wave_out);
    }
    ReleaseSRWLockExclusive(&this->_playback_lock);
    return ;
}

void ft_sound_device_win32::play(const ft_sound_clip *clip)
{
    if (clip == ft_nullptr)
        return ;
    AcquireSRWLockExclusive(&this->_playback_lock);
    if (this->_worker != ft_nullptr && !this->_closing && this->reset_playback())
    {
        this->_sample_size = clip->get_size();
        if (this->_sample_size != 0)
            this->_samples = clip->get_data();
        this->submit_buffer(0);
        this->submit_buffer(1);
    }
    ReleaseSRWLockExclusive(&this->_playback_lock);
    return ;
}

void ft_sound_device_win32::stop(void)
{
    AcquireSRWLockExclusive(&this->_playback_lock);
    if (this->_worker != ft_nullptr && !this->_closing)
        (void)this->reset_playback();
    ReleaseSRWLockExclusive(&this->_playback_lock);
    return ;
}

ft_sound_device *cmp_create_sound_device(void)
{
    return (new (std::nothrow) ft_sound_device_win32());
}

#endif
