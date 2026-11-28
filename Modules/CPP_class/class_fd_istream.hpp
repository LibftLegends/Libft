#ifndef FT_FD_ISTREAM_HPP
#define FT_FD_ISTREAM_HPP

#include "class_istream.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstdint>

class ft_fd_istream : public ft_istream
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        int32_t _file_descriptor;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;

    public:
        ft_fd_istream() noexcept;
        ft_fd_istream(const ft_fd_istream &other) noexcept = delete;
        ft_fd_istream(ft_fd_istream &&other) noexcept = delete;
        ~ft_fd_istream() noexcept;

        ft_fd_istream &operator=(const ft_fd_istream &other) noexcept = delete;
        ft_fd_istream &operator=(ft_fd_istream &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        // The caller retains ownership of the descriptor.
        void set_file_descriptor(int32_t file_descriptor) noexcept;
        int32_t move(ft_fd_istream &other) noexcept;
        int32_t get_file_descriptor() const noexcept;
        int32_t enable_thread_safety(void) noexcept;
        int32_t disable_thread_safety(void) noexcept;
        ft_bool is_thread_safe(void) const noexcept;

    protected:
        ssize_t do_read(char *buffer, ft_size_t count);
};

#endif
