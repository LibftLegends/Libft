#ifndef FILE_HPP
# define FILE_HPP

#ifndef DEBUG
# define DEBUG 0
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <cstddef>
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"

class ft_file
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        int32_t _file_descriptor;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t close_descriptor() noexcept;
        int32_t replace_descriptor(int32_t new_descriptor) noexcept;

    public:
        ft_file() noexcept;
        ft_file(const ft_file& other) noexcept = delete;
        ft_file(ft_file&& other) noexcept = delete;
        ~ft_file() noexcept;

        ft_file &operator=(const ft_file&) = delete;
        ft_file& operator=(ft_file&& other) = delete;

        int32_t            initialize() noexcept;
        int32_t             destroy() noexcept;
        int32_t             move(ft_file &other) noexcept;
        int32_t            get_file_descriptor() const;

        int32_t            open(const char* filename, int32_t flags, mode_t mode) noexcept;
        int32_t            open(const char* filename, int32_t flags) noexcept;
        ssize_t        write(const char *string) noexcept;
        ssize_t        write_buffer(const char *buffer, ft_size_t length) noexcept;
        void        close() noexcept;
        int32_t            seek(off_t offset, int32_t whence) noexcept;
        ssize_t        read(char *buffer, int32_t count) noexcept;
        int32_t            printf(const char *format, ...)
                        __attribute__((format(printf, 2, 3), hot));
        int32_t            copy_to(const char *destination_path) noexcept;
        int32_t            copy_to_with_buffer(const char *destination_path, ft_size_t buffer_size) noexcept;
        int32_t            enable_thread_safety(void) noexcept;
        int32_t            disable_thread_safety(void) noexcept;
        ft_bool           is_thread_safe(void) const noexcept;
        int32_t            get_error() const noexcept;
        const char         *get_error_str() const noexcept;
};

#endif
