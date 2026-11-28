#ifndef FT_ISTREAM_HPP
#define FT_ISTREAM_HPP

#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno_internal.hpp"

class ft_istream
{
    friend class ft_fd_istream;

#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_size_t _gcount;
        ft_bool _is_valid;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;

    protected:
        ft_istream() noexcept;
        virtual ssize_t do_read(char *buffer, ft_size_t count) = 0;

    public:
        virtual ~ft_istream() noexcept;

        ft_istream(const ft_istream &other) noexcept = delete;
        ft_istream(ft_istream &&other) noexcept = delete;
        ft_istream &operator=(const ft_istream &other) noexcept = delete;
        ft_istream &operator=(ft_istream &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_istream &other) noexcept;
        int32_t enable_thread_safety(void) noexcept;
        int32_t disable_thread_safety(void) noexcept;
        ft_bool is_thread_safe(void) const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
        ssize_t read(char *buffer, ft_size_t count) noexcept;
        ft_size_t gcount() const noexcept;
        ft_bool is_valid() const noexcept;
};

#endif
