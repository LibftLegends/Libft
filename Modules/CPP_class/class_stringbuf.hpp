#ifndef FT_STRINGBUF_HPP
#define FT_STRINGBUF_HPP

#include "class_string.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstdint>
#include <cstddef>

class ft_stringbuf
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _storage;
        ft_size_t _position;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        ft_stringbuf() noexcept;
        ft_stringbuf(const ft_stringbuf &other) noexcept = delete;
        ft_stringbuf(ft_stringbuf &&other) noexcept = delete;
        ~ft_stringbuf() noexcept;

        ft_stringbuf &operator=(const ft_stringbuf &other) noexcept = delete;
        ft_stringbuf &operator=(ft_stringbuf &&other) noexcept = delete;

        int32_t initialize(const ft_string &string) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_stringbuf &other) noexcept;
        int64_t read(char *buffer, ft_size_t count) noexcept;
        ft_bool is_valid() const noexcept;
        int32_t get_string(ft_string &value) const noexcept;
        int32_t enable_thread_safety(void) noexcept;
        int32_t disable_thread_safety(void) noexcept;
        ft_bool is_thread_safe(void) const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
