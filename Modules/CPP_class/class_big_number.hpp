#ifndef FT_BIG_NUMBER_HPP
#define FT_BIG_NUMBER_HPP

#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"

class ft_big_number_proxy;

class ft_big_number
{
    friend class ft_big_number_proxy;
    friend ft_string *big_number_to_hex_string(const ft_big_number &number) noexcept;
    friend ft_big_number *big_number_from_hex_string(const char *hex_digits) noexcept;

#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        char*           _digits;
        ft_size_t       _size;
        ft_size_t       _capacity;
        ft_bool            _is_negative;
        mutable pt_recursive_mutex *_mutex;
        mutable uint8_t            _initialised_state;
        int32_t             _operation_error;
        static thread_local int32_t _last_error;
        static thread_local uint8_t _last_initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;

        void    reserve(ft_size_t new_capacity) noexcept;
        void    shrink_capacity() noexcept;
        static int32_t  lock_pair(const ft_big_number &first, const ft_big_number &second,
                const ft_big_number *&lower,
                const ft_big_number *&upper) noexcept;
        static int32_t  unlock_pair(const ft_big_number *lower, const ft_big_number *upper) noexcept;
        static void sleep_backoff() noexcept;
        void    clear_unlocked() noexcept;
        ft_size_t append_position() const noexcept;
        int32_t reserve_append(ft_size_t position, ft_size_t count) noexcept;
        void store_appended_digit(ft_size_t position, char digit) noexcept;
        int32_t append_digit_unlocked(char digit) noexcept;
        int32_t append_unlocked(const char *digits) noexcept;
        int32_t append_unsigned_unlocked(uint64_t value) noexcept;
        void    trim_leading_zeros_unlocked(ft_bool shrink_storage = FT_TRUE) noexcept;
        void    reduce_to_unlocked(ft_size_t new_size) noexcept;
        ft_bool    is_zero_value() const noexcept;
        int32_t     compare_magnitude(const ft_big_number& other) const noexcept;
        int32_t compare_value(const ft_big_number &other, int32_t &comparison) const noexcept;
        // Magnitude helpers write into an initialized, empty result.
        int32_t copy_magnitude(ft_big_number &result) const noexcept;
        int32_t copy_value(const ft_big_number &other) noexcept;
        int32_t add_magnitude(const ft_big_number &other, ft_big_number &result) const noexcept;
        int32_t subtract_magnitude(const ft_big_number &other, ft_big_number &result) const noexcept;
        ft_big_number_proxy sum_or_difference(const ft_big_number &other,
            ft_bool subtract) const noexcept;
        int32_t multiply_magnitude(const ft_big_number &other, ft_big_number &result) const noexcept;
        int32_t multiply_grouped_magnitude(const ft_big_number &other,
            ft_big_number &result) const noexcept;
        int32_t multiply_magnitude_by_digit(uint32_t digit, ft_big_number &result) const noexcept;
        int32_t divide_by_digit(uint32_t divisor, ft_big_number *quotient,
            ft_big_number &remainder) const noexcept;
        int32_t divide_magnitude(const ft_big_number &divisor, ft_big_number *quotient,
            ft_big_number &remainder) const noexcept;
        ft_big_number_proxy quotient_or_remainder(const ft_big_number &divisor,
            ft_bool remainder_only) const noexcept;
        int32_t assign_base_digits(const char *digits, uint32_t base, ft_bool negative) noexcept;
        int32_t assign_base_magnitude(const char *digits, uint32_t base, ft_bool negative) noexcept;
        ft_string *format_base(int32_t base) const noexcept;
        ft_bool halve_exponent() noexcept;
        int32_t mod_pow_value(const ft_big_number &exponent, const ft_big_number &modulus,
            ft_big_number &result) const noexcept;

    public:
        ft_big_number() noexcept;
        ft_big_number(const ft_big_number& other) noexcept = delete;
        ft_big_number(ft_big_number&& other) noexcept = delete;
        ~ft_big_number() noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_big_number& other) noexcept;
        int32_t initialize(ft_big_number&& other) noexcept;
        int32_t         destroy() noexcept;
        int32_t         move(ft_big_number& other) noexcept;

        ft_big_number& operator=(const ft_big_number& other) noexcept;
        ft_big_number& operator=(ft_big_number&& other) noexcept;
        ft_big_number& operator=(const ft_big_number_proxy& other) noexcept;
        ft_big_number& operator=(ft_big_number_proxy &&other) noexcept;
        ft_big_number_proxy operator+(const ft_big_number& other) const noexcept;
        ft_big_number_proxy operator-(const ft_big_number& other) const noexcept;
        ft_big_number_proxy operator*(const ft_big_number& other) const noexcept;
        ft_big_number_proxy operator/(const ft_big_number& other) const noexcept;
        ft_big_number_proxy operator%(const ft_big_number& other) const noexcept;
        ft_bool          operator==(const ft_big_number& other) const noexcept;
        ft_bool          operator!=(const ft_big_number& other) const noexcept;
        ft_bool          operator<(const ft_big_number& other) const noexcept;
        ft_bool          operator<=(const ft_big_number& other) const noexcept;
        ft_bool          operator>(const ft_big_number& other) const noexcept;
        ft_bool          operator>=(const ft_big_number& other) const noexcept;

        // Reports parse and allocation failures through get_error().
        void        assign(const char* number) noexcept;
        int32_t         assign_base(const char* digits, int32_t base) noexcept;
        void        append_digit(char digit) noexcept;
        void        append(const char* digits) noexcept;
        void        append_unsigned(uint64_t value) noexcept;
        void        trim_leading_zeros() noexcept;
        void        reduce_to(ft_size_t new_size) noexcept;
        void        clear() noexcept;

        const char* c_str() const noexcept;
        ft_size_t   size() const noexcept;
        ft_bool        empty() const noexcept;
        ft_bool        is_negative() const noexcept;
        ft_bool        is_positive() const noexcept;
        ft_string   *to_string_base(int32_t base) noexcept;
        ft_big_number *mod_pow(const ft_big_number& exponent, const ft_big_number& modulus) const noexcept;
        static const char *get_error_str() noexcept;
        static int32_t get_error() noexcept;
        int32_t     enable_thread_safety(void) noexcept;
        int32_t     disable_thread_safety(void) noexcept;
        ft_bool    is_thread_safe(void) const noexcept;

};

class ft_big_number_proxy
{
    friend class ft_big_number;

#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_big_number _value;
        int32_t _last_error;

    public:
        ft_big_number_proxy() noexcept;
        explicit ft_big_number_proxy(int32_t error_code) noexcept;
        ft_big_number_proxy(const ft_big_number &value) noexcept;
        ft_big_number_proxy(const ft_big_number &value, int32_t error_code) noexcept;
        ft_big_number_proxy(const ft_big_number_proxy &other) noexcept;
        ft_big_number_proxy(ft_big_number_proxy &&other) noexcept;
        ~ft_big_number_proxy();

        ft_big_number_proxy &operator=(const ft_big_number_proxy &other) noexcept = delete;
        ft_big_number_proxy &operator=(ft_big_number_proxy &&other) noexcept = delete;

        ft_big_number_proxy operator+(const ft_big_number &right) const noexcept;
        ft_big_number_proxy operator-(const ft_big_number &right) const noexcept;
        ft_big_number_proxy operator*(const ft_big_number &right) const noexcept;
        ft_big_number_proxy operator/(const ft_big_number &right) const noexcept;
        ft_big_number_proxy operator%(const ft_big_number &right) const noexcept;

        operator ft_big_number *() const noexcept;
        int32_t get_error() const noexcept;
};

ft_big_number_proxy operator+(const ft_big_number_proxy &left, const ft_big_number &right) noexcept;
ft_big_number_proxy operator-(const ft_big_number_proxy &left, const ft_big_number &right) noexcept;
ft_big_number_proxy operator*(const ft_big_number_proxy &left, const ft_big_number &right) noexcept;
ft_big_number_proxy operator/(const ft_big_number_proxy &left, const ft_big_number &right) noexcept;
ft_big_number_proxy operator%(const ft_big_number_proxy &left, const ft_big_number &right) noexcept;

ft_string       *big_number_to_hex_string(const ft_big_number& number) noexcept;
ft_big_number   *big_number_from_hex_string(const char* hex_digits) noexcept;

#endif
