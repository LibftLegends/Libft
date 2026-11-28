#ifndef GAME_PROGRESS_TRACKER_HPP
# define GAME_PROGRESS_TRACKER_HPP

#include "game_achievement.hpp"
#include "game_quest.hpp"
#include "../Template/map.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_progress_tracker
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, game_achievement> _achievements;
        ft_map<int32_t, game_quest>       _quests;
        pt_recursive_mutex                   *_mutex;
        static thread_local int32_t _last_error;
        uint8_t                     _initialised_state;


        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        template <typename Map, typename Visit>
        int32_t visit_live(Map &map, int32_t id, Visit visit) const noexcept;

    public:
        game_progress_tracker() noexcept;
        ~game_progress_tracker() noexcept;
        game_progress_tracker(const game_progress_tracker &other) noexcept = delete;
        game_progress_tracker &operator=(const game_progress_tracker &other) noexcept = delete;
        game_progress_tracker(game_progress_tracker &&other) noexcept = delete;
        game_progress_tracker &operator=(game_progress_tracker &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_progress_tracker &other) noexcept;
        int32_t initialize(game_progress_tracker &&other) noexcept;
        int32_t move(game_progress_tracker &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        ft_map<int32_t, game_achievement> &get_achievements() noexcept;
        const ft_map<int32_t, game_achievement> &get_achievements() const noexcept;
        ft_map<int32_t, game_quest> &get_quests() noexcept;
        const ft_map<int32_t, game_quest> &get_quests() const noexcept;

        // A failed replacement leaves existing entries intact and sets get_error().
        void set_achievements(const ft_map<int32_t, game_achievement> &achievements) noexcept;
        void set_quests(const ft_map<int32_t, game_quest> &quests) noexcept;

        int32_t register_achievement(const game_achievement &achievement) noexcept;
        int32_t register_quest(const game_quest &quest) noexcept;

        int32_t update_goal_target(int32_t achievement_id, int32_t goal_id, int32_t target) noexcept;
        int32_t update_goal_progress(int32_t achievement_id, int32_t goal_id, int32_t progress) noexcept;
        int32_t add_goal_progress(int32_t achievement_id, int32_t goal_id, int32_t value) noexcept;
        ft_bool is_achievement_complete(int32_t achievement_id) const noexcept;

        int32_t set_quest_phase(int32_t quest_id, int32_t phase) noexcept;
        int32_t advance_quest_phase(int32_t quest_id) noexcept;
        ft_bool is_quest_complete(int32_t quest_id) const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
