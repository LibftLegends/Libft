#ifndef FT_DIALOGUE_SCRIPT_HPP
# define FT_DIALOGUE_SCRIPT_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_dialogue_line.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_dialogue_script
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t _script_id;
        ft_string _title;
        ft_string _summary;
        int32_t _start_line_id;
        ft_vector<ft_sharedptr<game_dialogue_line> > _lines;
        pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        static int32_t clone_line(const game_dialogue_line *line,
            ft_sharedptr<game_dialogue_line> &owned_line) noexcept;
        template <typename LineType>
        static int32_t stage_lines(const ft_vector<LineType> &source,
            ft_vector<ft_sharedptr<game_dialogue_line>> &staged) noexcept;

    public:
        game_dialogue_script() noexcept;
        virtual ~game_dialogue_script() noexcept;
        game_dialogue_script(const game_dialogue_script &other) noexcept = delete;
        game_dialogue_script &operator=(const game_dialogue_script &other) noexcept = delete;
        game_dialogue_script(game_dialogue_script &&other) noexcept = delete;
        game_dialogue_script &operator=(game_dialogue_script &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_dialogue_script &other) noexcept;
        int32_t initialize(game_dialogue_script &&other) noexcept;
        int32_t move(game_dialogue_script &other) noexcept;
        int32_t initialize(int32_t script_id, const ft_string &title,
            const ft_string &summary, int32_t start_line_id,
            const ft_vector<game_dialogue_line> &lines) noexcept;
        int32_t destroy() noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_script_id() const noexcept;
        void set_script_id(int32_t script_id) noexcept;

        const ft_string &get_title() const noexcept;
        void set_title(const ft_string &title) noexcept;

        const ft_string &get_summary() const noexcept;
        void set_summary(const ft_string &summary) noexcept;

        int32_t get_start_line_id() const noexcept;
        void set_start_line_id(int32_t start_line_id) noexcept;

        const ft_vector<ft_sharedptr<game_dialogue_line> > &get_lines() const noexcept;
        ft_vector<ft_sharedptr<game_dialogue_line> > &get_lines() noexcept;
        void set_lines(const ft_vector<ft_sharedptr<game_dialogue_line> > &lines) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
