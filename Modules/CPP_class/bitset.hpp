#ifndef FT_BITSET_HPP
#define FT_BITSET_HPP

#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Template/move.hpp"
#include <cstddef>
#include <climits>

class ft_bitset
{
    private:
        static constexpr ft_size_t BITS_PER_BLOCK = sizeof(ft_size_t) * CHAR_BIT;

        ft_size_t                    _size;
        ft_size_t                    _configured_bits;
        ft_size_t                    _block_count;
        ft_size_t                    *_data;
        mutable pt_recursive_mutex   *_mutex;
        uint8_t                      _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code);
        ft_size_t block_index(ft_size_t position) const;
        ft_size_t bit_mask(ft_size_t position) const;
        int32_t initialize_data(const ft_size_t *source_data);
        void destroy_data();
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;

    public:
        explicit ft_bitset(ft_size_t bits = 0);
        ft_bitset(const ft_bitset &other) = delete;
        ft_bitset(ft_bitset &&other) = delete;
        ~ft_bitset();

        ft_bitset& operator=(const ft_bitset&) = delete;
        ft_bitset& operator=(ft_bitset&& other) = delete;

        int32_t initialize();
        int32_t initialize(ft_size_t bits);
        int32_t initialize(const ft_bitset &other);
        int32_t initialize(ft_bitset &&other);
        int32_t destroy();
        int32_t move(ft_bitset &other);

        void set(ft_size_t position);
        void reset(ft_size_t position);
        void flip(ft_size_t position);
        ft_bool test(ft_size_t position) const;
        ft_size_t size() const;
        void clear();

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const;
        const char *get_error_str() const;
};

#endif
