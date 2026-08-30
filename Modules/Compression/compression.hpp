#ifndef COMPRESSION_HPP
# define COMPRESSION_HPP

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include "../CPP_class/class_string.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"

static const std::size_t   compression_max_size = static_cast<std::size_t>(UINT32_MAX);

unsigned char    *compress_buffer(const unsigned char *input_buffer, std::size_t input_size, std::size_t *compressed_size);
unsigned char    *decompress_buffer(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decompressed_size);

unsigned char    *ft_compress(const unsigned char *input_buffer, std::size_t input_size, std::size_t *compressed_size);
unsigned char    *ft_decompress(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decompressed_size);

typedef struct s_compress_stream_progress
{
    std::size_t    total_bytes_read;
    std::size_t    total_bytes_written;
}   t_compress_stream_progress;

typedef int (*t_compress_stream_progress_callback)(const t_compress_stream_progress *progress, void *user_data);
typedef int (*t_compress_stream_cancel_callback)(const t_compress_stream_progress *progress, void *user_data);

struct s_compress_stream_options_snapshot;

class t_compress_stream_options
{
    private:
        std::size_t                            _input_buffer_size;
        std::size_t                            _output_buffer_size;
        t_compress_stream_progress_callback    _progress_callback;
        t_compress_stream_cancel_callback      _cancel_callback;
        void                                   *_callback_user_data;
        int                                     _compression_level;
        int                                     _window_bits;
        int                                     _memory_level;
        int                                     _strategy;
        mutable pt_recursive_mutex              *_mutex;
        std::atomic<ft_bool>                    _thread_safety_enabled;
        mutable std::mutex                      _thread_safety_transition_mutex;
        uint8_t                                 _initialised_state;
        static const uint8_t                    _state_uninitialised = 0;
        static const uint8_t                    _state_destroyed = 1;
        static const uint8_t                    _state_initialised = 2;

        void    abort_lifecycle_error(const char *method_name,
                    const char *reason) const;
        void    abort_if_not_initialised(const char *method_name) const;
        int     lock_for_access() const;
        int     unlock_for_access() const;

        t_compress_stream_options(const t_compress_stream_options &other) = delete;
        t_compress_stream_options(t_compress_stream_options &&other) = delete;
        t_compress_stream_options &operator=(const t_compress_stream_options &other) = delete;
        t_compress_stream_options &operator=(t_compress_stream_options &&other) = delete;

    public:
        t_compress_stream_options(void);
        ~t_compress_stream_options(void);
        int     initialize(void);
        int     destroy(void);
        int     enable_thread_safety();
        int     disable_thread_safety();
        bool    is_thread_safe() const;

        int         reset(void);
        int         set_input_buffer_size(std::size_t input_buffer_size);
        int         set_output_buffer_size(std::size_t output_buffer_size);
        int         set_progress_callback(t_compress_stream_progress_callback callback, void *user_data);
        int         set_cancel_callback(t_compress_stream_cancel_callback callback, void *user_data);
        int         set_callbacks(t_compress_stream_progress_callback progress_callback,
                        t_compress_stream_cancel_callback cancel_callback,
                        void *user_data);
        int         set_compression_level(int compression_level);
        int         set_window_bits(int window_bits);
        int         set_memory_level(int memory_level);
        int         set_strategy(int strategy);
        Pair<int, std::size_t> get_input_buffer_size() const;
        Pair<int, std::size_t> get_output_buffer_size() const;
        Pair<int, t_compress_stream_progress_callback> get_progress_callback() const;
        Pair<int, t_compress_stream_cancel_callback> get_cancel_callback() const;
        Pair<int, void *> get_callback_user_data() const;
        Pair<int, int> get_compression_level() const;
        Pair<int, int> get_window_bits() const;
        Pair<int, int> get_memory_level() const;
        Pair<int, int> get_strategy() const;
        int         snapshot(struct s_compress_stream_options_snapshot *snapshot) const;
#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const;
#endif
};

int              ft_compress_stream_with_options(int input_file_descriptor, int output_file_descriptor, const t_compress_stream_options *options);
int              ft_decompress_stream_with_options(int input_file_descriptor, int output_file_descriptor, const t_compress_stream_options *options);
int              ft_compress_stream(int input_file_descriptor, int output_file_descriptor);
int              ft_decompress_stream(int input_file_descriptor, int output_file_descriptor);
void             ft_compress_stream_apply_speed_preset(t_compress_stream_options *options);
void             ft_compress_stream_apply_ratio_preset(t_compress_stream_options *options);
unsigned char    *ft_base64_encode(const unsigned char *input_buffer, std::size_t input_size, std::size_t *encoded_size);
unsigned char    *ft_base64_decode(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decoded_size);
int              ft_compress_string_to_vector(const ft_string &input, ft_vector<unsigned char> &output);
int              ft_compress_vector_to_vector(const ft_vector<unsigned char> &input, ft_vector<unsigned char> &output);
int              ft_decompress_vector_to_string(const ft_vector<unsigned char> &input, ft_string &output);
int              ft_decompress_vector_to_vector(const ft_vector<unsigned char> &input, ft_vector<unsigned char> &output);

#endif
