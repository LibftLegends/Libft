#ifndef GAME_EXPERIENCE_TABLE_HPP
# define GAME_EXPERIENCE_TABLE_HPP

#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <cstdint>

class game_experience_table
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t             *_levels;
        int32_t             _count;
        int32_t             _invalid_pairs;
        static thread_local int32_t _last_error;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t count_invalid_pairs(int32_t count) const noexcept;
        int32_t resize_locked(int32_t new_count,
                          ft_bool retain_levels = FT_TRUE) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        friend class game_character;

    public:
        game_experience_table() noexcept;
        ~game_experience_table() noexcept;
        game_experience_table(const game_experience_table &other) noexcept = delete;
        game_experience_table &operator=(const game_experience_table &other) noexcept = delete;
        game_experience_table(game_experience_table &&other) noexcept = delete;
        game_experience_table &operator=(game_experience_table &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_experience_table &other) noexcept;
        int32_t  get_count() const noexcept;
        int32_t  get_level(int32_t experience) const noexcept;
        int32_t  get_value(int32_t index) const noexcept;
        void set_value(int32_t index, int32_t value) noexcept;
        int32_t  set_levels(const int32_t *levels, int32_t count) noexcept;
        int32_t  generate_levels_total(int32_t count, int32_t base,
                                   double multiplier) noexcept;
        int32_t  generate_levels_scaled(int32_t count, int32_t base,
                                    double multiplier) noexcept;
        int32_t  resize(int32_t new_count) noexcept;
        int32_t  enable_thread_safety() noexcept;
        int32_t  disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t  check_for_error() const noexcept;
        int32_t  get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
