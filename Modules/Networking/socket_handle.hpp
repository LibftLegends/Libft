#ifndef NETWORKING_SOCKET_HANDLE_HPP
#define NETWORKING_SOCKET_HANDLE_HPP

#include "../Errno/errno.hpp"
#include <cstdint>

int32_t ft_socket_runtime_acquire();
void ft_socket_runtime_release();

class ft_socket_handle
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        int32_t _socket_fd;

    public:
        ft_socket_handle() noexcept;
        ft_socket_handle(const ft_socket_handle &other) noexcept = delete;
        ft_socket_handle(ft_socket_handle &&other) noexcept = delete;
        ~ft_socket_handle() noexcept;
        ft_socket_handle &operator=(const ft_socket_handle &other) = delete;
        ft_socket_handle &operator=(ft_socket_handle &&other) noexcept = delete;
        int32_t move(ft_socket_handle &other) noexcept;

        int32_t initialize() noexcept;
        int32_t initialize_duplicate(int32_t descriptor) noexcept;
        // Copying creates a separately owned descriptor for the same socket.
        int32_t initialize(const ft_socket_handle &other) noexcept;
        int32_t initialize(ft_socket_handle &&other) noexcept;
        int32_t destroy() noexcept;

        ft_bool reset(int32_t socket_fd);
        ft_bool close();
        ft_bool is_valid() const;
        int32_t get() const;
};

#endif
