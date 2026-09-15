#include <zlib.h>
#include <limits>
#include <new>
#include <cstdio>
#include <cerrno>
#include "../System_utils/system_utils.hpp"
#include "../Printf/printf.hpp"
#include "compression.hpp"
#include "compression_stream_test_hooks.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

static const std::size_t   g_compress_stream_default_buffer_size = 4096;
static const std::size_t   g_compress_stream_speed_buffer_size = 16384;
static const std::size_t   g_compress_stream_ratio_buffer_size = 32768;
static const int           g_compress_stream_min_window_bits = 8;
static const int           g_compress_stream_min_memory_level = 1;
static const int           g_compress_stream_max_memory_level = MAX_MEM_LEVEL;
static const int           g_compress_stream_default_memory_level = MAX_MEM_LEVEL - 1;

struct s_compress_stream_options_snapshot
{
    std::size_t                            input_buffer_size;
    std::size_t                            output_buffer_size;
    t_compress_stream_progress_callback    progress_callback;
    t_compress_stream_cancel_callback      cancel_callback;
    void                                   *callback_user_data;
    int                                     compression_level;
    int                                     window_bits;
    int                                     memory_level;
    int                                     strategy;
};

static void compression_stream_init_snapshot(struct s_compress_stream_options_snapshot *snapshot)
{
    if (snapshot == ft_nullptr)
        return ;
    snapshot->input_buffer_size = 0;
    snapshot->output_buffer_size = 0;
    snapshot->progress_callback = ft_nullptr;
    snapshot->cancel_callback = ft_nullptr;
    snapshot->callback_user_data = ft_nullptr;
    snapshot->compression_level = Z_DEFAULT_COMPRESSION;
    snapshot->window_bits = 0;
    snapshot->memory_level = 0;
    snapshot->strategy = Z_DEFAULT_STRATEGY;
    return ;
}

void t_compress_stream_options::abort_lifecycle_error(const char *method_name,
    const char *reason) const
{
    if (method_name == ft_nullptr)
        method_name = "unknown";
    if (reason == ft_nullptr)
        reason = "unknown";
    ft_fprintf(stderr, "t_compress_stream_options::%s lifecycle error: %s\n",
        method_name, reason);
    su_abort();
}

void t_compress_stream_options::abort_if_not_initialised(const char *method_name) const
{
    if (this->_initialised_state != this->_state_initialised)
        this->abort_lifecycle_error(method_name, "object is not initialised");
}

int t_compress_stream_options::enable_thread_safety()
{
    std::lock_guard<std::mutex> transition_lock(
        this->_thread_safety_transition_mutex);

    this->abort_if_not_initialised("enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->_thread_safety_enabled.store(FT_TRUE, std::memory_order_release);
        return (FT_ERR_SUCCESS);
    }
    pt_recursive_mutex *mutex_pointer = ft_nullptr;
    int mutex_error;

    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    this->_thread_safety_enabled.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::lock_for_access() const
{
    int lock_error;

    this->_thread_safety_transition_mutex.lock();
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        this->_thread_safety_transition_mutex.unlock();
    return (lock_error);
}

int t_compress_stream_options::unlock_for_access() const
{
    int unlock_error;

    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->_thread_safety_transition_mutex.unlock();
    return (unlock_error);
}

int t_compress_stream_options::disable_thread_safety()
{
    std::lock_guard<std::mutex> transition_lock(
        this->_thread_safety_transition_mutex);

    this->abort_if_not_initialised("disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_thread_safety_enabled.store(FT_FALSE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

bool t_compress_stream_options::is_thread_safe() const
{
    std::lock_guard<std::mutex> transition_lock(
        this->_thread_safety_transition_mutex);

    this->abort_if_not_initialised("is_thread_safe");
    return (this->_mutex != ft_nullptr
        && this->_thread_safety_enabled.load(std::memory_order_acquire)
            != FT_FALSE);
}

#define COMPRESSION_STREAM_OPTIONS_LOCK() this->lock_for_access()
#define COMPRESSION_STREAM_OPTIONS_UNLOCK() this->unlock_for_access()
#define pt_recursive_mutex_lock_if_not_null(mutex) this->lock_for_access()
#define pt_recursive_mutex_unlock_if_not_null(mutex) this->unlock_for_access()

t_compress_stream_options::t_compress_stream_options(void)
{
    this->_input_buffer_size = 0;
    this->_output_buffer_size = 0;
    this->_progress_callback = ft_nullptr;
    this->_cancel_callback = ft_nullptr;
    this->_callback_user_data = ft_nullptr;
    this->_compression_level = Z_BEST_COMPRESSION;
    this->_window_bits = 0;
    this->_memory_level = 0;
    this->_strategy = Z_DEFAULT_STRATEGY;
    this->_mutex = ft_nullptr;
    this->_thread_safety_enabled.store(FT_FALSE, std::memory_order_relaxed);
    this->_initialised_state = this->_state_uninitialised;
    return ;
}

t_compress_stream_options::~t_compress_stream_options(void)
{
    if (this->_initialised_state == this->_state_initialised)
        (void)this->destroy();
    return ;
}

int t_compress_stream_options::initialize(void)
{
    if (this->_initialised_state == this->_state_initialised)
        return (FT_ERR_INVALID_STATE);
    this->_initialised_state = this->_state_initialised;
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::destroy(void)
{
    int destroy_error;
    std::lock_guard<std::mutex> transition_lock(
        this->_thread_safety_transition_mutex);

    if (this->_initialised_state != this->_state_initialised)
        return (FT_ERR_INVALID_STATE);
    this->_thread_safety_enabled.store(FT_FALSE, std::memory_order_release);
    destroy_error = FT_ERR_SUCCESS;
    if (this->_mutex != ft_nullptr)
    {
        destroy_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
    }
    this->_initialised_state = this->_state_destroyed;
    return (destroy_error);
}

int t_compress_stream_options::reset(void)
{
    int lock_error;
    int unlock_error;

    lock_error = this->lock_for_access();    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_input_buffer_size = 0;
    this->_output_buffer_size = 0;
    this->_progress_callback = ft_nullptr;
    this->_cancel_callback = ft_nullptr;
    this->_callback_user_data = ft_nullptr;
    this->_compression_level = Z_BEST_COMPRESSION;
    this->_window_bits = 0;
    this->_memory_level = 0;
    this->_strategy = Z_DEFAULT_STRATEGY;
    unlock_error = this->unlock_for_access();    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_input_buffer_size(std::size_t input_buffer_size)
{
    int lock_error;
    int unlock_error;

    lock_error = COMPRESSION_STREAM_OPTIONS_LOCK();    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_input_buffer_size = input_buffer_size;
    unlock_error = COMPRESSION_STREAM_OPTIONS_UNLOCK();    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_output_buffer_size(std::size_t output_buffer_size)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_output_buffer_size = output_buffer_size;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_progress_callback(t_compress_stream_progress_callback callback, void *user_data)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_progress_callback = callback;
    this->_callback_user_data = user_data;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_cancel_callback(t_compress_stream_cancel_callback callback, void *user_data)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_cancel_callback = callback;
    this->_callback_user_data = user_data;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_callbacks(t_compress_stream_progress_callback progress_callback,
        t_compress_stream_cancel_callback cancel_callback,
        void *user_data)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_progress_callback = progress_callback;
    this->_cancel_callback = cancel_callback;
    this->_callback_user_data = user_data;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_compression_level(int compression_level)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_compression_level = compression_level;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_window_bits(int window_bits)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_window_bits = window_bits;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_memory_level(int memory_level)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_memory_level = memory_level;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

int t_compress_stream_options::set_strategy(int strategy)
{
    int lock_error;
    int unlock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    this->_strategy = strategy;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
}

Pair<int, std::size_t> t_compress_stream_options::get_input_buffer_size() const
{
    int lock_error;
    int unlock_error;
    std::size_t input_buffer_size;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, std::size_t>(lock_error, 0));
    input_buffer_size = this->_input_buffer_size;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, std::size_t>(unlock_error, 0));
    return (Pair<int, std::size_t>(FT_ERR_SUCCESS, input_buffer_size));
}

Pair<int, std::size_t> t_compress_stream_options::get_output_buffer_size() const
{
    int lock_error;
    int unlock_error;
    std::size_t output_buffer_size;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, std::size_t>(lock_error, 0));
    output_buffer_size = this->_output_buffer_size;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, std::size_t>(unlock_error, 0));
    return (Pair<int, std::size_t>(FT_ERR_SUCCESS, output_buffer_size));
}

Pair<int, t_compress_stream_progress_callback> t_compress_stream_options::get_progress_callback() const
{
    int lock_error;
    int unlock_error;
    t_compress_stream_progress_callback progress_callback;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, t_compress_stream_progress_callback>(lock_error, ft_nullptr));
    progress_callback = this->_progress_callback;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, t_compress_stream_progress_callback>(unlock_error, ft_nullptr));
    return (Pair<int, t_compress_stream_progress_callback>(FT_ERR_SUCCESS, progress_callback));
}

Pair<int, t_compress_stream_cancel_callback> t_compress_stream_options::get_cancel_callback() const
{
    int lock_error;
    int unlock_error;
    t_compress_stream_cancel_callback cancel_callback;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, t_compress_stream_cancel_callback>(lock_error, ft_nullptr));
    cancel_callback = this->_cancel_callback;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, t_compress_stream_cancel_callback>(unlock_error, ft_nullptr));
    return (Pair<int, t_compress_stream_cancel_callback>(FT_ERR_SUCCESS, cancel_callback));
}

Pair<int, void *> t_compress_stream_options::get_callback_user_data() const
{
    int lock_error;
    int unlock_error;
    void *user_data;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, void *>(lock_error, ft_nullptr));
    user_data = this->_callback_user_data;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, void *>(unlock_error, ft_nullptr));
    return (Pair<int, void *>(FT_ERR_SUCCESS, user_data));
}

Pair<int, int> t_compress_stream_options::get_compression_level() const
{
    int lock_error;
    int unlock_error;
    int compression_level;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(lock_error, Z_BEST_COMPRESSION));
    compression_level = this->_compression_level;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(unlock_error, Z_BEST_COMPRESSION));
    return (Pair<int, int>(FT_ERR_SUCCESS, compression_level));
}

Pair<int, int> t_compress_stream_options::get_window_bits() const
{
    int lock_error;
    int unlock_error;
    int window_bits;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(lock_error, 0));
    window_bits = this->_window_bits;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(unlock_error, 0));
    return (Pair<int, int>(FT_ERR_SUCCESS, window_bits));
}

Pair<int, int> t_compress_stream_options::get_memory_level() const
{
    int lock_error;
    int unlock_error;
    int memory_level;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(lock_error, 0));
    memory_level = this->_memory_level;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(unlock_error, 0));
    return (Pair<int, int>(FT_ERR_SUCCESS, memory_level));
}

Pair<int, int> t_compress_stream_options::get_strategy() const
{
    int lock_error;
    int unlock_error;
    int strategy;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(lock_error, Z_DEFAULT_STRATEGY));
    strategy = this->_strategy;
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
        return (Pair<int, int>(unlock_error, Z_DEFAULT_STRATEGY));
    return (Pair<int, int>(FT_ERR_SUCCESS, strategy));
}

int t_compress_stream_options::snapshot(struct s_compress_stream_options_snapshot *snapshot) const
{
    int lock_error;
    int unlock_error;
    int result;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    if (snapshot == ft_nullptr)
        result = FT_ERR_INVALID_ARGUMENT;
    else
    {
        snapshot->input_buffer_size = this->_input_buffer_size;
        snapshot->output_buffer_size = this->_output_buffer_size;
        snapshot->progress_callback = this->_progress_callback;
        snapshot->cancel_callback = this->_cancel_callback;
        snapshot->callback_user_data = this->_callback_user_data;
        snapshot->compression_level = this->_compression_level;
        snapshot->window_bits = this->_window_bits;
        snapshot->memory_level = this->_memory_level;
        snapshot->strategy = this->_strategy;
        result = FT_ERR_SUCCESS;
    }
    unlock_error = pt_recursive_mutex_unlock_if_not_null(this->_mutex);    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (result);
}

#ifdef LIBFT_TEST_BUILD
pt_recursive_mutex *t_compress_stream_options::get_mutex_for_validation() const
{
    return (this->_mutex);
}
#endif

#undef pt_recursive_mutex_lock_if_not_null
#undef pt_recursive_mutex_unlock_if_not_null
#undef COMPRESSION_STREAM_OPTIONS_LOCK
#undef COMPRESSION_STREAM_OPTIONS_UNLOCK

void    ft_compress_stream_apply_speed_preset(t_compress_stream_options *options)
{
    int memory_level;
    int initialize_error;

    if (options == ft_nullptr)
        return ;
    initialize_error = options->initialize();
    if (initialize_error != FT_ERR_SUCCESS && initialize_error != FT_ERR_INVALID_STATE)
        return ;
    if (options->set_input_buffer_size(g_compress_stream_speed_buffer_size) != FT_ERR_SUCCESS)
        return ;
    if (options->set_output_buffer_size(g_compress_stream_speed_buffer_size) != FT_ERR_SUCCESS)
        return ;
    if (options->set_compression_level(Z_BEST_SPEED) != FT_ERR_SUCCESS)
        return ;
    if (options->set_window_bits(MAX_WBITS - 2) != FT_ERR_SUCCESS)
        return ;
    memory_level = g_compress_stream_default_memory_level - 1;
    if (memory_level < g_compress_stream_min_memory_level)
        memory_level = g_compress_stream_min_memory_level;
    if (options->set_memory_level(memory_level) != FT_ERR_SUCCESS)
        return ;
    if (options->set_strategy(Z_DEFAULT_STRATEGY) != FT_ERR_SUCCESS)
        return ;
    return ;
}

void    ft_compress_stream_apply_ratio_preset(t_compress_stream_options *options)
{
    int memory_level;
    int initialize_error;

    if (options == ft_nullptr)
        return ;
    initialize_error = options->initialize();
    if (initialize_error != FT_ERR_SUCCESS && initialize_error != FT_ERR_INVALID_STATE)
        return ;
    if (options->set_input_buffer_size(g_compress_stream_ratio_buffer_size) != FT_ERR_SUCCESS)
        return ;
    if (options->set_output_buffer_size(g_compress_stream_ratio_buffer_size) != FT_ERR_SUCCESS)
        return ;
    if (options->set_compression_level(Z_BEST_COMPRESSION) != FT_ERR_SUCCESS)
        return ;
    if (options->set_window_bits(MAX_WBITS) != FT_ERR_SUCCESS)
        return ;
    memory_level = g_compress_stream_default_memory_level + 1;
    if (memory_level > g_compress_stream_max_memory_level)
        memory_level = g_compress_stream_max_memory_level;
    if (options->set_memory_level(memory_level) != FT_ERR_SUCCESS)
        return ;
    if (options->set_strategy(Z_DEFAULT_STRATEGY) != FT_ERR_SUCCESS)
        return ;
    return ;
}

static int  compression_stream_validate_tuning(const t_compress_stream_options *options)
{
    s_compress_stream_options_snapshot snapshot;

    compression_stream_init_snapshot(&snapshot);
    if (options == ft_nullptr)
        return (0);
    if (options->snapshot(&snapshot) != FT_ERR_SUCCESS)
        return (1);
    if (snapshot.compression_level < Z_DEFAULT_COMPRESSION || snapshot.compression_level > Z_BEST_COMPRESSION)
    {
        return (1);
    }
    if (snapshot.window_bits != 0
        && (snapshot.window_bits < g_compress_stream_min_window_bits || snapshot.window_bits > MAX_WBITS))
    {
        return (1);
    }
    if (snapshot.memory_level != 0
        && (snapshot.memory_level < g_compress_stream_min_memory_level
            || snapshot.memory_level > g_compress_stream_max_memory_level))
    {
        return (1);
    }
    if (snapshot.strategy != Z_DEFAULT_STRATEGY
        && snapshot.strategy != Z_FILTERED
        && snapshot.strategy != Z_HUFFMAN_ONLY
        && snapshot.strategy != Z_RLE
        && snapshot.strategy != Z_FIXED)
    {
        return (1);
    }
    return (0);
}

static t_compress_stream_deflate_init_hook compression_stream_get_deflate_init_hook(void);

static int  compression_stream_begin_deflate(z_stream *stream, const t_compress_stream_options *options)
{
    int compression_level;
    int window_bits;
    int memory_level;
    int strategy;

    compression_level = Z_BEST_COMPRESSION;
    if (options != ft_nullptr)
    {
        s_compress_stream_options_snapshot snapshot;

        compression_stream_init_snapshot(&snapshot);
        if (options->snapshot(&snapshot) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        compression_level = snapshot.compression_level;
        window_bits = snapshot.window_bits;
        memory_level = snapshot.memory_level;
        strategy = snapshot.strategy;
        if (window_bits != 0 || memory_level != 0 || strategy != Z_DEFAULT_STRATEGY)
        {
            if (window_bits == 0)
                window_bits = MAX_WBITS;
            if (memory_level == 0)
                memory_level = g_compress_stream_default_memory_level;
            return (deflateInit2(stream, compression_level, Z_DEFLATED, window_bits, memory_level, strategy));
        }
    }
    return (compression_stream_get_deflate_init_hook()(stream, compression_level));
}

static void compression_stream_release_buffers(unsigned char *input_buffer, unsigned char *output_buffer)
{
    if (input_buffer)
        cma_free(input_buffer);
    if (output_buffer)
        cma_free(output_buffer);
    return ;
}

static int compression_stream_write_all(int output_file_descriptor,
    const unsigned char *output_buffer, std::size_t output_size)
{
    std::size_t written_total;
    ssize_t written_bytes;

    if (output_size == 0)
        return (0);
    if (output_buffer == ft_nullptr)
        return (1);
    written_total = 0;
    while (written_total < output_size)
    {
        written_bytes = su_write(output_file_descriptor,
            output_buffer + written_total, output_size - written_total);
        if (written_bytes > 0)
        {
            written_total += static_cast<std::size_t>(written_bytes);
            continue ;
        }
        if (written_bytes < 0 && errno == EINTR)
            continue ;
        return (1);
    }
    return (0);
}

static int  compression_stream_allocate_buffers(const t_compress_stream_options *options,
        unsigned char **input_buffer,
        std::size_t *input_buffer_size,
        unsigned char **output_buffer,
        std::size_t *output_buffer_size)
{
    std::size_t    resolved_input_size;
    std::size_t    resolved_output_size;

    if (input_buffer == ft_nullptr || input_buffer_size == ft_nullptr
        || output_buffer == ft_nullptr || output_buffer_size == ft_nullptr)
    {
        return (1);
    }
    if (options)
    {
        s_compress_stream_options_snapshot snapshot;

        compression_stream_init_snapshot(&snapshot);
        if (options->snapshot(&snapshot) != FT_ERR_SUCCESS)
            return (1);
        resolved_input_size = snapshot.input_buffer_size;
        resolved_output_size = snapshot.output_buffer_size;
        if (resolved_input_size == 0 || resolved_output_size == 0)
        {
            return (1);
        }
    }
    else
    {
        resolved_input_size = g_compress_stream_default_buffer_size;
        resolved_output_size = g_compress_stream_default_buffer_size;
    }
    if (resolved_input_size > std::numeric_limits<unsigned int>::max()
        || resolved_output_size > std::numeric_limits<unsigned int>::max())
    {
        return (1);
    }
    *input_buffer = static_cast<unsigned char *>(cma_malloc(resolved_input_size));
    if (!*input_buffer)
    {
        return (1);
    }
    *output_buffer = static_cast<unsigned char *>(cma_malloc(resolved_output_size));
    if (!*output_buffer)
    {
        cma_free(*input_buffer);
        *input_buffer = ft_nullptr;
        return (1);
    }
    *input_buffer_size = resolved_input_size;
    *output_buffer_size = resolved_output_size;
    return (0);
}

static int compress_stream_default_deflate_init(z_stream *stream, int compression_level)
{
    return (deflateInit(stream, compression_level));
}

static int compress_stream_default_deflate(z_stream *stream, int flush_mode)
{
    return (deflate(stream, flush_mode));
}

static int decompress_stream_default_inflate_init(z_stream *stream)
{
    return (inflateInit(stream));
}

static int decompress_stream_default_inflate(z_stream *stream, int flush_mode)
{
    return (inflate(stream, flush_mode));
}

static t_compress_stream_deflate_init_hook    g_compress_stream_deflate_init_hook = compress_stream_default_deflate_init;
static t_compress_stream_deflate_hook         g_compress_stream_deflate_hook = compress_stream_default_deflate;
static t_compress_stream_read_hook            g_compress_stream_read_hook = ft_nullptr;
static t_decompress_stream_inflate_init_hook  g_decompress_stream_inflate_init_hook = decompress_stream_default_inflate_init;
static t_decompress_stream_inflate_hook       g_decompress_stream_inflate_hook = decompress_stream_default_inflate;

static t_compress_stream_deflate_init_hook compression_stream_get_deflate_init_hook(void)
{
    return (g_compress_stream_deflate_init_hook);
}

static int  compression_stream_dispatch_progress(const t_compress_stream_options *options, const t_compress_stream_progress *progress)
{
    int callback_result;
    s_compress_stream_options_snapshot snapshot;

    compression_stream_init_snapshot(&snapshot);
    if (options == ft_nullptr || progress == ft_nullptr)
        return (0);
    if (options->snapshot(&snapshot) != FT_ERR_SUCCESS)
        return (1);
    if (snapshot.progress_callback == ft_nullptr)
        return (0);
    callback_result = snapshot.progress_callback(progress, snapshot.callback_user_data);
    if (callback_result != 0)
    {
        return (1);
    }
    return (0);
}

static int  compression_stream_check_cancel(const t_compress_stream_options *options, const t_compress_stream_progress *progress)
{
    int callback_result;
    s_compress_stream_options_snapshot snapshot;

    compression_stream_init_snapshot(&snapshot);
    if (options == ft_nullptr || progress == ft_nullptr)
        return (0);
    if (options->snapshot(&snapshot) != FT_ERR_SUCCESS)
        return (1);
    if (snapshot.cancel_callback == ft_nullptr)
        return (0);
    callback_result = snapshot.cancel_callback(progress, snapshot.callback_user_data);
    if (callback_result != 0)
    {
        return (1);
    }
    return (0);
}

void ft_compress_stream_set_deflate_init_hook(t_compress_stream_deflate_init_hook hook)
{
    if (hook)
        g_compress_stream_deflate_init_hook = hook;
    else
        g_compress_stream_deflate_init_hook = compress_stream_default_deflate_init;
    return ;
}

void ft_compress_stream_set_deflate_hook(t_compress_stream_deflate_hook hook)
{
    if (hook)
        g_compress_stream_deflate_hook = hook;
    else
        g_compress_stream_deflate_hook = compress_stream_default_deflate;
    return ;
}

void ft_compress_stream_set_read_hook(t_compress_stream_read_hook hook)
{
    g_compress_stream_read_hook = hook;
    return ;
}

void ft_decompress_stream_set_inflate_init_hook(t_decompress_stream_inflate_init_hook hook)
{
    if (hook)
        g_decompress_stream_inflate_init_hook = hook;
    else
        g_decompress_stream_inflate_init_hook = decompress_stream_default_inflate_init;
    return ;
}

void ft_decompress_stream_set_inflate_hook(t_decompress_stream_inflate_hook hook)
{
    if (hook)
        g_decompress_stream_inflate_hook = hook;
    else
        g_decompress_stream_inflate_hook = decompress_stream_default_inflate;
    return ;
}

int ft_compress_stream_with_options(int input_file_descriptor, int output_file_descriptor, const t_compress_stream_options *options)
{
    z_stream        stream;
    unsigned char   *input_buffer;
    unsigned char   *output_buffer;
    std::size_t     input_buffer_size;
    std::size_t     output_buffer_size;
    int             flush_mode;
    int             deflate_status;
    ssize_t         read_bytes;
    t_compress_stream_progress    progress;

    if (input_file_descriptor < 0 || output_file_descriptor < 0)
    {
        return (1);
    }
    if (compression_stream_validate_tuning(options) != 0)
        return (1);
    input_buffer = ft_nullptr;
    output_buffer = ft_nullptr;
    input_buffer_size = 0;
    output_buffer_size = 0;
    if (compression_stream_allocate_buffers(options, &input_buffer, &input_buffer_size,
            &output_buffer, &output_buffer_size) != 0)
        return (1);
    ft_bzero(&stream, sizeof(stream));
    deflate_status = compression_stream_begin_deflate(&stream, options);
    if (deflate_status != Z_OK)
    {
        compression_stream_release_buffers(input_buffer, output_buffer);
        return (1);
    }
    flush_mode = Z_NO_FLUSH;
    ft_bzero(&progress, sizeof(progress));
    while (flush_mode != Z_FINISH)
    {
        if (compression_stream_check_cancel(options, &progress) != 0)
        {
            deflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        if (g_compress_stream_read_hook != ft_nullptr)
            read_bytes = g_compress_stream_read_hook(input_file_descriptor,
                input_buffer, input_buffer_size);
        else
            read_bytes = su_read(input_file_descriptor, input_buffer,
                input_buffer_size);
        if (read_bytes < 0)
        {
            deflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        stream.next_in = input_buffer;
        stream.avail_in = static_cast<unsigned int>(read_bytes);
        if (read_bytes > 0)
        {
            progress.total_bytes_read += static_cast<std::size_t>(read_bytes);
            if (compression_stream_dispatch_progress(options, &progress) != 0)
            {
                deflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
        }
        if (compression_stream_check_cancel(options, &progress) != 0)
        {
            deflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        if (read_bytes == 0)
            flush_mode = Z_FINISH;
        else
            flush_mode = Z_NO_FLUSH;
        do
        {
            stream.next_out = output_buffer;
            stream.avail_out = static_cast<unsigned int>(output_buffer_size);
            deflate_status = g_compress_stream_deflate_hook(&stream, flush_mode);
            if (deflate_status == Z_STREAM_ERROR || deflate_status == Z_BUF_ERROR)
            {
                deflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
            std::size_t produced_bytes;

            produced_bytes = output_buffer_size - static_cast<std::size_t>(stream.avail_out);
            if (compression_stream_write_all(output_file_descriptor,
                    output_buffer, produced_bytes) != 0)
            {
                deflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
            if (produced_bytes != 0)
            {
                progress.total_bytes_written += produced_bytes;
                if (compression_stream_dispatch_progress(options, &progress) != 0)
                {
                    deflateEnd(&stream);
                    compression_stream_release_buffers(input_buffer, output_buffer);
                    return (1);
                }
            }
            if (compression_stream_check_cancel(options, &progress) != 0)
            {
                deflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
        }
        while (stream.avail_out == 0);
    }
    deflateEnd(&stream);
    compression_stream_release_buffers(input_buffer, output_buffer);
    return (0);
}

int ft_decompress_stream_with_options(int input_file_descriptor, int output_file_descriptor, const t_compress_stream_options *options)
{
    z_stream        stream;
    unsigned char   *input_buffer;
    unsigned char   *output_buffer;
    std::size_t     input_buffer_size;
    std::size_t     output_buffer_size;
    int             inflate_status;
    ssize_t         read_bytes;
    int             flush_mode;
    int             stream_finished;
    t_compress_stream_progress    progress;

    if (input_file_descriptor < 0 || output_file_descriptor < 0)
    {
        return (1);
    }
    if (compression_stream_validate_tuning(options) != 0)
        return (1);
    input_buffer = ft_nullptr;
    output_buffer = ft_nullptr;
    input_buffer_size = 0;
    output_buffer_size = 0;
    if (compression_stream_allocate_buffers(options, &input_buffer, &input_buffer_size,
            &output_buffer, &output_buffer_size) != 0)
        return (1);
    ft_bzero(&stream, sizeof(stream));
    inflate_status = g_decompress_stream_inflate_init_hook(&stream);
    if (inflate_status != Z_OK)
    {
        compression_stream_release_buffers(input_buffer, output_buffer);
        return (1);
    }
    stream_finished = 0;
    flush_mode = Z_NO_FLUSH;
    ft_bzero(&progress, sizeof(progress));
    while (1)
    {
        if (compression_stream_check_cancel(options, &progress) != 0)
        {
            inflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        read_bytes = su_read(input_file_descriptor, input_buffer, input_buffer_size);
        if (read_bytes < 0)
        {
            inflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        stream.next_in = input_buffer;
        stream.avail_in = static_cast<unsigned int>(read_bytes);
        if (read_bytes > 0)
        {
            progress.total_bytes_read += static_cast<std::size_t>(read_bytes);
            if (compression_stream_dispatch_progress(options, &progress) != 0)
            {
                inflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
        }
        if (compression_stream_check_cancel(options, &progress) != 0)
        {
            inflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        if (read_bytes == 0)
            flush_mode = Z_FINISH;
        else
            flush_mode = Z_NO_FLUSH;
        do
        {
            stream.next_out = output_buffer;
            stream.avail_out = static_cast<unsigned int>(output_buffer_size);
            inflate_status = g_decompress_stream_inflate_hook(&stream, flush_mode);
            if (inflate_status == Z_STREAM_END)
                stream_finished = 1;
            if (inflate_status == Z_BUF_ERROR && stream_finished == 0)
            {
                inflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
            if (inflate_status == Z_NEED_DICT
                || inflate_status == Z_DATA_ERROR
                || inflate_status == Z_MEM_ERROR
                || inflate_status == Z_STREAM_ERROR
                || inflate_status == Z_VERSION_ERROR)
            {
                inflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
            std::size_t produced_bytes;

            produced_bytes = output_buffer_size - static_cast<std::size_t>(stream.avail_out);
            if (produced_bytes != 0
                && compression_stream_write_all(output_file_descriptor,
                    output_buffer, produced_bytes) != 0)
            {
                inflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
            if (produced_bytes != 0)
            {
                progress.total_bytes_written += produced_bytes;
                if (compression_stream_dispatch_progress(options, &progress) != 0)
                {
                    inflateEnd(&stream);
                    compression_stream_release_buffers(input_buffer, output_buffer);
                    return (1);
                }
            }
            if (compression_stream_check_cancel(options, &progress) != 0)
            {
                inflateEnd(&stream);
                compression_stream_release_buffers(input_buffer, output_buffer);
                return (1);
            }
        }
        while (stream.avail_out == 0 && stream_finished == 0 && stream.avail_in != 0);
        if (stream_finished != 0 && stream.avail_in != 0)
        {
            inflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
        if (stream_finished != 0)
        {
            if (flush_mode == Z_FINISH)
                break ;
            continue ;
        }
        if (flush_mode == Z_FINISH)
        {
            inflateEnd(&stream);
            compression_stream_release_buffers(input_buffer, output_buffer);
            return (1);
        }
    }
    inflateEnd(&stream);
    compression_stream_release_buffers(input_buffer, output_buffer);
    return (0);
}

int ft_compress_stream(int input_file_descriptor, int output_file_descriptor)
{
    return (ft_compress_stream_with_options(input_file_descriptor, output_file_descriptor, ft_nullptr));
}

int ft_decompress_stream(int input_file_descriptor, int output_file_descriptor)
{
    return (ft_decompress_stream_with_options(input_file_descriptor, output_file_descriptor, ft_nullptr));
}
