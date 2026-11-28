#ifndef GAME_STATE_HPP
# define GAME_STATE_HPP

#include "game_world.hpp"
#include "game_character.hpp"
#include "game_item.hpp"
#include "game_event.hpp"
#include "../Template/vector.hpp"
#include "../Template/map.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_hooks;

class game_state
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_vector<ft_sharedptr<game_world> >     _worlds;
        ft_vector<ft_sharedptr<game_character> > _characters;
        ft_map<ft_string, ft_string>           _variables;
        ft_sharedptr<game_hooks>            _hooks;
        mutable pt_recursive_mutex                       *_mutex;
        uint8_t                                _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        ft_bool has_active_lock() const noexcept;
        int32_t transfer_storage(game_state &other, ft_bool transfer_worlds,
            ft_bool transfer_characters) noexcept;
        template <typename State, typename Visit>
        static void visit_hooks(State &state, Visit visit) noexcept;

    public:
        game_state() noexcept;
        ~game_state() noexcept;
        game_state(const game_state &other) noexcept = delete;
        game_state &operator=(const game_state &other) noexcept = delete;
        game_state(game_state &&other) noexcept = delete;
        game_state &operator=(game_state &&other) noexcept = delete;

        int32_t initialize() noexcept;
        // Moving requires exclusive access to both states and their collections.
        int32_t move(game_state &other) noexcept;
        int32_t destroy() noexcept;

        // Callers must synchronize direct access through these collection references.
        ft_vector<ft_sharedptr<game_world> > &get_worlds() noexcept;

        ft_vector<ft_sharedptr<game_character> > &get_characters() noexcept;

        void set_variable(const ft_string &key, const ft_string &value) noexcept;
        // Borrowed value. Callers must prevent concurrent variable mutations while using it.
        const ft_string *get_variable(const ft_string &key) const noexcept;
        void remove_variable(const ft_string &key) noexcept;
        void clear_variables() noexcept;

        int32_t add_character(const ft_sharedptr<game_character> &character) noexcept;
        void remove_character(ft_size_t index) noexcept;

        void set_hooks(const ft_sharedptr<game_hooks> &hooks) noexcept;
        ft_sharedptr<game_hooks> *get_hooks() const noexcept;
        void reset_hooks() noexcept;

        void dispatch_item_crafted(game_character &character, game_item &item) const noexcept;
        void dispatch_character_damaged(game_character &character, int32_t damage, uint8_t type) const noexcept;
        void dispatch_event_triggered(game_world &world, game_event &event) const noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
