#ifndef GAME_EVENT_HPP
# define GAME_EVENT_HPP

#include "../Template/function.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

class game_world;

class game_event
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t _id;
        int32_t _duration;
        int32_t _modifier1;
        int32_t _modifier2;
        int32_t _modifier3;
        int32_t _modifier4;
        ft_function<void(game_world&, game_event&)> _callback;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        void subtract_duration(int32_t duration, int32_t *remaining_duration) noexcept;
        int32_t read_modifier(const int32_t &field) const noexcept;
        void write_modifier(int32_t &field, int32_t value) noexcept;
        void adjust_modifier(int32_t &field, int64_t adjustment) noexcept;

        friend struct game_event_compare_ptr;
        friend class game_event_scheduler;

    public:
        game_event() noexcept;
        virtual ~game_event() noexcept;
        int32_t initialize() noexcept;
        int32_t move(game_event &other) noexcept;
        int32_t destroy() noexcept;
        game_event(const game_event &other) noexcept = delete;
        game_event &operator=(const game_event &other) noexcept = delete;
        game_event(game_event &&other) noexcept = delete;
        game_event &operator=(game_event &&other) noexcept = delete;

        int32_t get_id() const noexcept;
        void set_id(int32_t id) noexcept;

        int32_t get_duration() const noexcept;
        int32_t get_schedule_values(int32_t &id, int32_t &duration) const noexcept;
        void set_duration(int32_t duration) noexcept;
        int32_t add_duration(int32_t duration) noexcept;
        void sub_duration(int32_t duration) noexcept;

        // Modifier arithmetic leaves the value unchanged on overflow and reports FT_ERR_OUT_OF_RANGE.
        int32_t get_modifier1() const noexcept;
        void set_modifier1(int32_t mod) noexcept;
        void add_modifier1(int32_t mod) noexcept;
        void sub_modifier1(int32_t mod) noexcept;

        int32_t get_modifier2() const noexcept;
        void set_modifier2(int32_t mod) noexcept;
        void add_modifier2(int32_t mod) noexcept;
        void sub_modifier2(int32_t mod) noexcept;

        int32_t get_modifier3() const noexcept;
        void set_modifier3(int32_t mod) noexcept;
        void add_modifier3(int32_t mod) noexcept;
        void sub_modifier3(int32_t mod) noexcept;

        int32_t get_modifier4() const noexcept;
        void set_modifier4(int32_t mod) noexcept;
        void add_modifier4(int32_t mod) noexcept;
        void sub_modifier4(int32_t mod) noexcept;

        // Configure callbacks before scheduling. Synchronize replacements and access
        // to the returned reference externally through the end of invocation.
        const ft_function<void(game_world&, game_event&)> &get_callback() const noexcept;
        void set_callback(ft_function<void(game_world&, game_event&)> &&callback) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
