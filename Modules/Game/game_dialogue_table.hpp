#ifndef GAME_DIALOGUE_TABLE_HPP
# define GAME_DIALOGUE_TABLE_HPP

#include "game_dialogue_line.hpp"
#include "game_dialogue_script.hpp"
#include "../Template/map.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include "../Template/shared_ptr.hpp"
#include <stdint.h>

class game_dialogue_table
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, ft_sharedptr<game_dialogue_line> > _lines;
        ft_map<int32_t, game_dialogue_line *> _line_cache;
        ft_map<int32_t, game_dialogue_script> _scripts;
        pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;
        uint8_t _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        static int32_t clone_line(const game_dialogue_line &line,
            game_dialogue_line *&clone) noexcept;
        int32_t insert_line_snapshot(int32_t line_id,
            ft_sharedptr<game_dialogue_line> &stored, game_dialogue_line *cached) noexcept;

    public:
        game_dialogue_table() noexcept;
        virtual ~game_dialogue_table() noexcept;
        game_dialogue_table(const game_dialogue_table &other) noexcept = delete;
        game_dialogue_table &operator=(const game_dialogue_table &other) noexcept = delete;
        game_dialogue_table(game_dialogue_table &&other) noexcept = delete;
        game_dialogue_table &operator=(game_dialogue_table &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_dialogue_table &other) noexcept;
        int32_t initialize(game_dialogue_table &&other) noexcept;
        int32_t move(game_dialogue_table &other) noexcept;
        int32_t destroy() noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t register_line(const game_dialogue_line &line) noexcept;
        int32_t register_script(const game_dialogue_script &script) noexcept;

        int32_t fetch_line(int32_t line_id, game_dialogue_line &out_line) const noexcept;
        int32_t fetch_script(int32_t script_id, game_dialogue_script &out_script) const noexcept;

        ft_map<int32_t, ft_sharedptr<game_dialogue_line> > &get_lines() noexcept;
        const ft_map<int32_t, ft_sharedptr<game_dialogue_line> > &get_lines() const noexcept;
        void set_lines(const ft_map<int32_t, ft_sharedptr<game_dialogue_line> > &lines) noexcept;

        ft_map<int32_t, game_dialogue_script> &get_scripts() noexcept;
        const ft_map<int32_t, game_dialogue_script> &get_scripts() const noexcept;
        void set_scripts(const ft_map<int32_t, game_dialogue_script> &scripts) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
