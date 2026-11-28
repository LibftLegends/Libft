#ifndef FT_DIALOGUE_LINE_HPP
# define FT_DIALOGUE_LINE_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <cstdint>

class game_dialogue_line
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t _line_id;
        ft_string _speaker;
        ft_string _text;
        ft_vector<int32_t> _next_line_ids;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t replace_values(int32_t line_id, const ft_string &speaker,
            const ft_string &text, const ft_vector<int32_t> &next_line_ids) noexcept;
        int32_t copy_values(const game_dialogue_line &source) noexcept;
        friend class game_dialogue_script;
        friend class game_dialogue_table;

    public:
        game_dialogue_line() noexcept;
        virtual ~game_dialogue_line() noexcept;
        game_dialogue_line(const game_dialogue_line &other) noexcept = delete;
        game_dialogue_line &operator=(const game_dialogue_line &other) noexcept = delete;
        game_dialogue_line(game_dialogue_line &&other) noexcept = delete;
        game_dialogue_line &operator=(game_dialogue_line &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t line_id, const ft_string &speaker,
            const ft_string &text, const ft_vector<int32_t> &next_line_ids) noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_dialogue_line &other) noexcept;

        int32_t get_line_id() const noexcept;
        void set_line_id(int32_t line_id) noexcept;

        const ft_string &get_speaker() const noexcept;
        void set_speaker(const ft_string &speaker) noexcept;

        const ft_string &get_text() const noexcept;
        void set_text(const ft_string &text) noexcept;

        const ft_vector<int32_t> &get_next_line_ids() const noexcept;
        ft_vector<int32_t> &get_next_line_ids() noexcept;
        void set_next_line_ids(const ft_vector<int32_t> &next_line_ids) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
        ft_bool is_initialised() const noexcept;

};

#endif
