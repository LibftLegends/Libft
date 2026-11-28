#ifndef FT_STRING_HPP
#define FT_STRING_HPP

#include <cstring>
#include <climits>
#include <cstdint>
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Basic/basic.hpp"

class ft_string_proxy;

class ft_string
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        char*                    _data;
        ft_size_t                _length;
        ft_size_t                _capacity;
        mutable pt_recursive_mutex *_mutex;
        mutable uint8_t            _initialised_state;
        mutable int32_t            _last_error;

        int32_t set_error(int32_t error_code) const noexcept;
        friend class ft_string_proxy;
        friend int32_t ft_string_take_owned_buffer(ft_string &output, char *buffer,
            ft_size_t length, char **previous_buffer) noexcept;


        int32_t     resize(ft_size_t new_capacity) noexcept;
        static void sleep_backoff() noexcept;
        int32_t     resize_buffer(ft_size_t new_capacity) noexcept;
        int32_t     append_char_buffer(char character) noexcept;
        int32_t     append_buffer(const char *string, ft_size_t length) noexcept;
        int32_t     clear_buffer() noexcept;
        int32_t     ensure_empty_buffer() noexcept;
        int32_t     assign_buffer(ft_size_t count, char character) noexcept;
        int32_t     assign_buffer(const char *string, ft_size_t length) noexcept;
        int32_t     erase_buffer(ft_size_t index, ft_size_t count) noexcept;
        int32_t     resize_length_buffer(ft_size_t new_length) noexcept;
        int32_t     move_buffer(ft_string &other) noexcept;

    public:
        ft_string() noexcept;
        ft_string(const ft_string &other) noexcept = delete;
        ft_string(ft_string &&other) noexcept = delete;
        ~ft_string();

        ft_string &operator=(const ft_string &other) noexcept;
        ft_string &operator=(ft_string &&other) noexcept;
        ft_string &operator=(const ft_string_proxy &other) noexcept;
        ft_string &operator=(const char *string) noexcept;
        ft_string &operator=(char character) noexcept;

        ft_string &operator+=(const ft_string &string) noexcept;
        ft_string &operator+=(const char *string) noexcept;
        ft_string &operator+=(char character) noexcept;

        ft_bool        operator==(const ft_string &other) const noexcept;
        ft_bool        operator==(const char *string) const noexcept;
        ft_bool        operator!=(const ft_string &other) const noexcept;
        ft_bool        operator!=(const char *string) const noexcept;
        char        operator[](ft_size_t index) const noexcept;
        operator const char*() const noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const char *initial_string) noexcept;
        int32_t initialize(ft_size_t count, char character) noexcept;
        int32_t initialize(const ft_string &other) noexcept;
        int32_t initialize(ft_string &&other) noexcept;
        int32_t destroy() noexcept;

        int32_t     enable_thread_safety(void) noexcept;
        int32_t     disable_thread_safety(void) noexcept;
        ft_bool     is_thread_safe(void) const noexcept;
        int32_t      append(char character) noexcept;
        int32_t      append(const char *string) noexcept;
        int32_t      append(const char *string, ft_size_t length) noexcept;
        int32_t      append(const ft_string &string) noexcept;
        int32_t      assign(ft_size_t count, char character) noexcept;
        int32_t      clear() noexcept;
        int32_t      assign(const char *string, ft_size_t length) noexcept;
        int32_t      assign(const ft_string &other) noexcept;
        const char    *at(ft_size_t index) const noexcept;
        const char    *c_str() const noexcept;
        char        *data() noexcept;
        const char  *data() const noexcept;
        char*       print() noexcept;
        ft_size_t   size() const noexcept;
        ft_bool        empty() const noexcept;
        ft_bool        is_initialised() const noexcept;
        int32_t     move(ft_string& other) noexcept;
        int32_t     erase(ft_size_t index, ft_size_t count) noexcept;
        int32_t     push_back(char character) noexcept;
        char        back() noexcept;
        ft_size_t   find(const ft_string &substring) const noexcept;
        ft_size_t   find(const char *substring) const noexcept;
        int32_t     resize_length(ft_size_t new_length) noexcept;
        ft_string   *substr(ft_size_t index, ft_size_t count = npos) const noexcept;
        static ft_string *from_error(int32_t error_code) noexcept;

        static const ft_size_t npos = static_cast<ft_size_t>(-1);
        int32_t get_error() const noexcept;
        const char  *get_error_str() const noexcept;

};

// Internal. Transfer a CMA buffer with length bytes followed by a terminator.
// The caller retains ownership if the function fails.
int32_t ft_string_take_owned_buffer(ft_string &output, char *buffer, ft_size_t length) noexcept;
// On success, writes the displaced buffer to previous_buffer for the caller to free.
int32_t ft_string_take_owned_buffer(ft_string &output, char *buffer, ft_size_t length,
    char **previous_buffer) noexcept;

class ft_string_proxy
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        class error_restorer
        {
            private:
                const int32_t *_error_pointer;

            public:
                error_restorer() noexcept;
                error_restorer(const error_restorer &other) noexcept;
                ~error_restorer() noexcept;

                error_restorer &operator=(const error_restorer &other) noexcept = delete;
                error_restorer &operator=(error_restorer &&other) noexcept = delete;

                void set_error_pointer(const int32_t *error_pointer) noexcept;
        };

        error_restorer _error_restorer;
        ft_string   _value;
        int32_t     _last_error;

    public:
        ft_string_proxy() noexcept;
        explicit ft_string_proxy(int32_t error_code) noexcept;
        ft_string_proxy(const ft_string &value, int32_t error_code) noexcept;
        ft_string_proxy(const ft_string_proxy &other) noexcept;
        ft_string_proxy(ft_string_proxy &&other) noexcept;
        ~ft_string_proxy();

        ft_string_proxy &operator=(const ft_string_proxy &other) noexcept = delete;
        ft_string_proxy &operator=(ft_string_proxy &&other) noexcept = delete;

        ft_string_proxy operator+(const ft_string &right) const noexcept;
        ft_string_proxy operator+(const char *right) const noexcept;
        ft_string_proxy operator+(char right) const noexcept;

        operator ft_string *() const noexcept;
        int32_t get_error() const noexcept;
};

ft_string_proxy operator+(const ft_string &left, const ft_string &right) noexcept;
ft_string_proxy operator+(const ft_string &left, const char *right) noexcept;
ft_string_proxy operator+(const char *left, const ft_string &right) noexcept;
ft_string_proxy operator+(const ft_string &left, char right) noexcept;
ft_string_proxy operator+(char left, const ft_string &right) noexcept;
ft_string_proxy operator+(const ft_string_proxy &left, const ft_string &right) noexcept;
ft_string_proxy operator+(const ft_string_proxy &left, const char *right) noexcept;
ft_string_proxy operator+(const ft_string_proxy &left, char right) noexcept;
ft_bool        operator==(const char *left, const ft_string &right) noexcept;
ft_bool        operator!=(const char *left, const ft_string &right) noexcept;

#endif
