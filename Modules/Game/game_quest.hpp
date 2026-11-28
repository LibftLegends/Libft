#ifndef GAME_QUEST_HPP
# define GAME_QUEST_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "game_item.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_quest
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t                             _id;
        int32_t                             _phases;
        int32_t                             _current_phase;
        ft_string                       _description;
        ft_string                       _objective;
        int32_t                             _reward_experience;
        ft_vector<ft_sharedptr<game_item> > _reward_items;
        mutable pt_recursive_mutex               *_mutex;
        static thread_local int32_t _last_error;
        uint8_t                         _initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t advance_phase_internal(ft_bool require_incomplete) noexcept;

        friend class game_progress_tracker;

    public:
        game_quest() noexcept;
        game_quest(const game_quest &other) noexcept = delete;
        game_quest(game_quest &&other) noexcept = delete;
        virtual ~game_quest() noexcept;
        game_quest &operator=(const game_quest &other) = delete;
        game_quest &operator=(game_quest &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_quest &other) noexcept;
        int32_t initialize(game_quest &&other) noexcept;
        int32_t move(game_quest &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_id() const noexcept;
        void set_id(int32_t id) noexcept;

        int32_t get_phases() const noexcept;
        void set_phases(int32_t phases) noexcept;

        int32_t get_current_phase() const noexcept;
        void set_current_phase(int32_t phase) noexcept;

        const ft_string &get_description() const noexcept;
        void set_description(const ft_string &description) noexcept;

        const ft_string &get_objective() const noexcept;
        void set_objective(const ft_string &objective) noexcept;

        int32_t get_reward_experience() const noexcept;
        void set_reward_experience(int32_t experience) noexcept;

        ft_vector<ft_sharedptr<game_item> >       &get_reward_items() noexcept;
        const ft_vector<ft_sharedptr<game_item> > &get_reward_items() const noexcept;
        void set_reward_items(const ft_vector<ft_sharedptr<game_item> > &items) noexcept;

        ft_bool is_complete() const noexcept;
        void advance_phase() noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
