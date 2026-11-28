#include "sound_clip.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "sound_device.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdio>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static constexpr ft_sound_spec DEFAULT_SOUND_CLIP_SPEC = {44100, 2, 4096, nullptr, nullptr};

static int32_t create_recursive_mutex(pt_recursive_mutex **mutex_pointer)
{
    pt_recursive_mutex *created_mutex;
    int32_t initialize_error;

    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    created_mutex = new (std::nothrow) pt_recursive_mutex();
    if (created_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = created_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete created_mutex;
        return (initialize_error);
    }
    *mutex_pointer = created_mutex;
    return (FT_ERR_SUCCESS);
}

static uint32_t destroy_recursive_mutex(pt_recursive_mutex **mutex_pointer)
{
    uint32_t destroy_error;

    if (mutex_pointer == ft_nullptr || *mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = (*mutex_pointer)->destroy();
    delete *mutex_pointer;
    *mutex_pointer = ft_nullptr;
    return (destroy_error);
}

static int32_t lock_ordered_mutexes(pt_recursive_mutex *mutex_left,
    pt_recursive_mutex *mutex_right, pt_recursive_mutex **first_mutex,
    pt_recursive_mutex **second_mutex)
{
    int32_t lock_error;

    if (reinterpret_cast<uintptr_t>(mutex_left)
        <= reinterpret_cast<uintptr_t>(mutex_right))
    {
        *first_mutex = mutex_left;
        *second_mutex = mutex_right;
    }
    else
    {
        *first_mutex = mutex_right;
        *second_mutex = mutex_left;
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(*first_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (*second_mutex == *first_mutex)
        return (FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(*second_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(*first_mutex);
        return (lock_error);
    }
    return (FT_ERR_SUCCESS);
}

static void unlock_ordered_mutexes(pt_recursive_mutex *first_mutex,
    pt_recursive_mutex *second_mutex)
{
    if (second_mutex != first_mutex)
        (void)pt_recursive_mutex_unlock_if_not_null(second_mutex);
    (void)pt_recursive_mutex_unlock_if_not_null(first_mutex);
    return ;
}

ft_sound_clip::ft_sound_clip(void)
    : _spec(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_sound_clip::~ft_sound_clip(void)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t ft_sound_clip::initialize(void)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_sound_clip::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_data.clear();
    this->_spec = new (std::nothrow) ft_sound_spec(DEFAULT_SOUND_CLIP_SPEC);
    if (this->_spec == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_sound_clip::initialize(const ft_sound_clip &other)
{
    int32_t lock_error;
    uint32_t destroy_error;
    pt_recursive_mutex *first_mutex;
    pt_recursive_mutex *second_mutex;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_sound_clip::initialize(const ft_sound_clip &) source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (destroy_error);
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_sound_clip::initialize(const ft_sound_clip &) source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    const int32_t initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    lock_error = lock_ordered_mutexes(this->_mutex, other._mutex,
        &first_mutex, &second_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }
    try
    {
        this->_data = other._data;
    }
    catch (const std::bad_alloc &)
    {
        unlock_ordered_mutexes(first_mutex, second_mutex);
        (void)this->destroy();
        return (FT_ERR_NO_MEMORY);
    }
    if (other._spec != ft_nullptr && this->_spec != ft_nullptr)
        *this->_spec = *other._spec;
    unlock_ordered_mutexes(first_mutex, second_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_sound_clip::initialize(ft_sound_clip &&other)
{
    return (this->move(other));
}

int32_t ft_sound_clip::destroy(void)
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const uint32_t disable_error = this->disable_thread_safety();
    std::vector<uint8_t>().swap(this->_data);
    delete this->_spec;
    this->_spec = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t ft_sound_clip::move(ft_sound_clip &other)
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_sound_clip::move source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
            return (this->destroy());
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_sound_clip::move source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const uint32_t disable_error = this->disable_thread_safety();
        if (disable_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (disable_error);
        }
        std::vector<uint8_t>().swap(this->_data);
    }
    else
    {
        const int32_t initialize_error = this->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }

    this->_data.swap(other._data);
    *this->_spec = *other._spec;
    *other._spec = DEFAULT_SOUND_CLIP_SPEC;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_sound_clip::prepare_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sound_clip::prepare_thread_safety");
    if (this->_mutex)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex_pointer = ft_nullptr;
    int32_t mutex_error;

    mutex_error = create_recursive_mutex(&mutex_pointer);
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

uint32_t ft_sound_clip::teardown_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sound_clip::teardown_thread_safety");
    return (destroy_recursive_mutex(&this->_mutex));
}

struct dumb_wav_payload
{
    int32_t sample_rate;
    int32_t channels;
    ft_size_t sample_offset;
    ft_size_t sample_size;
};

static uint32_t dumb_wav_read_u16(const uint8_t *bytes)
{
    return (static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8U));
}

static uint32_t dumb_wav_read_u32(const uint8_t *bytes)
{
    return (dumb_wav_read_u16(bytes) | (dumb_wav_read_u16(bytes + 2) << 16U));
}

static ft_bool dumb_wav_read_at(FILE *file, ft_size_t &position,
    ft_size_t offset, uint8_t *bytes, ft_size_t count)
{
    if (position != offset && cmp_file_stream_seek_absolute(file, offset) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (std::fread(bytes, 1, FT_UINT64_TO_NATIVE_SIZE_CAST(count), file) != count)
        return (FT_FALSE);
    position = offset + count;
    return (FT_TRUE);
}

static int32_t dumb_wav_find_payload(FILE *file, ft_size_t size,
    ft_size_t &position, dumb_wav_payload &payload)
{
    constexpr ft_size_t RIFF_HEADER_SIZE = 12;
    constexpr ft_size_t CHUNK_HEADER_SIZE = 8;
    constexpr uint32_t PCM_FORMAT = 1;
    constexpr uint32_t PCM_BITS_PER_SAMPLE = 16;
    uint8_t riff_header[RIFF_HEADER_SIZE];
    if (size < RIFF_HEADER_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!dumb_wav_read_at(file, position, 0, riff_header, sizeof(riff_header)))
        return (FT_ERR_IO);
    if (ft_memcmp(riff_header, "RIFF", 4) != 0 || ft_memcmp(riff_header + 8, "WAVE", 4) != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t riff_end = CHUNK_HEADER_SIZE + dumb_wav_read_u32(riff_header + 4);
    if (riff_end < RIFF_HEADER_SIZE || riff_end > size)
        return (FT_ERR_INVALID_ARGUMENT);

    uint32_t frame_size = 0;
    ft_size_t offset = RIFF_HEADER_SIZE;
    while (offset < riff_end)
    {
        if (riff_end - offset < CHUNK_HEADER_SIZE)
            return (FT_ERR_INVALID_ARGUMENT);
        uint8_t chunk[CHUNK_HEADER_SIZE];
        if (!dumb_wav_read_at(file, position, offset, chunk, sizeof(chunk)))
            return (FT_ERR_IO);
        const ft_size_t chunk_size = dumb_wav_read_u32(chunk + 4);
        const ft_size_t padded_size = chunk_size + (chunk_size & 1U);
        offset += CHUNK_HEADER_SIZE;
        if (padded_size > riff_end - offset)
            return (FT_ERR_INVALID_ARGUMENT);
        if (frame_size == 0 && ft_memcmp(chunk, "fmt ", 4) == 0)
        {
            uint8_t format[16];
            if (chunk_size < sizeof(format))
                return (FT_ERR_INVALID_ARGUMENT);
            if (!dumb_wav_read_at(file, position, offset, format, sizeof(format)))
                return (FT_ERR_IO);
            if (dumb_wav_read_u16(format) != PCM_FORMAT
                || dumb_wav_read_u16(format + 14) != PCM_BITS_PER_SAMPLE)
                return (FT_ERR_UNSUPPORTED_TYPE);
            const uint32_t channels = dumb_wav_read_u16(format + 2);
            const uint32_t sample_rate = dumb_wav_read_u32(format + 4);
            frame_size = dumb_wav_read_u16(format + 12);
            if (channels == 0 || frame_size != channels * (PCM_BITS_PER_SAMPLE / 8)
                || sample_rate == 0 || sample_rate > FT_INT32_MAX / frame_size)
                return (FT_ERR_INVALID_ARGUMENT);
            payload.channels = static_cast<int32_t>(channels);
            payload.sample_rate = static_cast<int32_t>(sample_rate);
        }
        else if (payload.sample_offset == 0 && ft_memcmp(chunk, "data", 4) == 0)
        {
            payload.sample_offset = offset;
            payload.sample_size = chunk_size;
        }
        offset += padded_size;
    }
    if (frame_size == 0 || payload.sample_offset == 0
        || payload.sample_size % frame_size != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

int32_t ft_sound_clip::load_wav(const char *file_path)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_sound_clip::load_wav");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);

    FILE *file = ft_nullptr;
    if (file_path != ft_nullptr)
        file = std::fopen(file_path, "rb");
    if (file == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_IO);
    }

    ft_size_t size = 0;
    ft_size_t position = 0;
    dumb_wav_payload payload{};
    std::vector<uint8_t> samples;
    int32_t result = cmp_file_stream_size(file, &size);
    if (result == FT_ERR_SUCCESS)
        result = dumb_wav_find_payload(file, size, position, payload);
    if (result == FT_ERR_SUCCESS)
    {
        if (payload.sample_size > samples.max_size())
            result = FT_ERR_OUT_OF_RANGE;
        else
        {
            try
            {
                samples.resize(FT_UINT64_TO_NATIVE_SIZE_CAST(payload.sample_size));
                if (!samples.empty() && !dumb_wav_read_at(file, position,
                        payload.sample_offset, samples.data(), payload.sample_size))
                    result = FT_ERR_IO;
            }
            catch (const std::bad_alloc &)
            {
                result = FT_ERR_NO_MEMORY;
            }
        }
    }
    if (std::fclose(file) != 0 && result == FT_ERR_SUCCESS)
        result = FT_ERR_IO;
    if (result == FT_ERR_SUCCESS)
    {
        if (samples.size() <= this->_data.capacity())
            this->_data.assign(samples.begin(), samples.end());
        else
            this->_data.swap(samples);
        this->_spec->channels = payload.channels;
        this->_spec->freq = payload.sample_rate;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

uint32_t ft_sound_clip::enable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sound_clip::enable_thread_safety");
    return (this->prepare_thread_safety());
}

uint32_t ft_sound_clip::disable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sound_clip::disable_thread_safety");
    return (this->teardown_thread_safety());
}

ft_bool ft_sound_clip::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sound_clip::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

const uint8_t *ft_sound_clip::get_data(void) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_sound_clip::get_data");
    return (this->_data.data());
}

ft_size_t ft_sound_clip::get_size(void) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_sound_clip::get_size");
    return (this->_data.size());
}

const ft_sound_spec *ft_sound_clip::get_spec(void) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_sound_clip::get_spec");
    return (this->_spec);
}
