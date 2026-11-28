#ifndef GNL_STREAM_HPP
# define GNL_STREAM_HPP

#include <cstdio>
#include <cstdint>

#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Basic/class_nullptr.hpp"

class gnl_stream
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        typedef int64_t (*gnl_stream_read_callback)(void *user_data,
                char *buffer, ft_size_t max_size) noexcept;

        gnl_stream_read_callback _read_callback;
        void *_user_data;
        int32_t _file_descriptor;
        FILE *_file_handle;
        ft_bool _close_on_reset;
        uint8_t _initialised_state;
        mutable pt_recursive_mutex *_mutex = ft_nullptr;

        void reset_source(FILE *retained_file) noexcept;
        void abort_lifecycle_error(const char *method_name,
                    const char *reason) const noexcept;

    public:
        gnl_stream() noexcept;
        gnl_stream(const gnl_stream &other) noexcept = delete;
        gnl_stream(gnl_stream &&other) noexcept = delete;
        ~gnl_stream() noexcept;

        gnl_stream &operator=(const gnl_stream &other) = delete;
        gnl_stream &operator=(gnl_stream &&other) = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        uint32_t move(gnl_stream &other) noexcept;
        int32_t enable_thread_safety(void) noexcept;
        int32_t disable_thread_safety(void) noexcept;
        ft_bool is_thread_safe(void) const noexcept;
        int32_t init_from_fd(int32_t file_descriptor) noexcept;
        int32_t init_from_file(FILE *file_handle, ft_bool close_on_reset) noexcept;
        int32_t init_from_callback(int64_t (*callback)(void *user_data,
                char *buffer, ft_size_t max_size) noexcept,
                void *user_data) noexcept;
        void reset() noexcept;
        int64_t read(char *buffer, ft_size_t max_size) noexcept;

};

#endif
