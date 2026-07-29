#ifndef GAME_WORLD_HPP
# define GAME_WORLD_HPP

#include "game_event_scheduler.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "game_pathfinding.hpp"
#include "../CPP_class/class_string.hpp"
#include "game_world_replay.hpp"
#include "game_economy_table.hpp"
#include "game_crafting.hpp"
#include "game_dialogue_table.hpp"
#include "game_world_region.hpp"
#include "game_quest.hpp"
#include "game_vendor_profile.hpp"
#include "game_world_registry.hpp"
#include "game_upgrade.hpp"
#include <stdint.h>

class game_character;
class game_inventory;
class kv_store;
class game_world_registry;
class game_world_replay_session;
class game_economy_table;
class game_crafting;
class game_dialogue_table;
class game_world_region;
class game_quest;
class game_vendor_profile;
class game_upgrade;

class game_world
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_sharedptr<game_event_scheduler> _event_scheduler;
        ft_sharedptr<game_world_registry> _world_registry;
        ft_sharedptr<game_world_replay_session> _replay_session;
        ft_sharedptr<game_economy_table> _economy_table;
        ft_sharedptr<game_crafting> _crafting;
        ft_sharedptr<game_dialogue_table> _dialogue_table;
        ft_sharedptr<game_world_region> _world_region;
        ft_sharedptr<game_quest> _quest;
        ft_sharedptr<game_vendor_profile> _vendor_profile;
        ft_sharedptr<game_upgrade> _upgrade;
        static thread_local int32_t _last_error;
        uint8_t _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        ft_bool propagate_scheduler_state_error() const noexcept;
        ft_bool propagate_registry_state_error() const noexcept;
        ft_bool propagate_replay_state_error() const noexcept;
        ft_bool propagate_economy_state_error() const noexcept;
        ft_bool propagate_crafting_state_error() const noexcept;
        ft_bool propagate_dialogue_state_error() const noexcept;
        ft_bool propagate_region_state_error() const noexcept;
        ft_bool propagate_quest_state_error() const noexcept;
        ft_bool propagate_vendor_profile_state_error() const noexcept;
        ft_bool propagate_upgrade_state_error() const noexcept;
        json_group *build_snapshot_groups(const game_character &character,
            const game_inventory &inventory, int32_t &error_code) const noexcept;
        int32_t restore_from_groups(json_group *groups, game_character &character,
            game_inventory &inventory) noexcept;

    public:
        game_world() noexcept;
        game_world(const game_world &other) noexcept = delete;
        game_world(game_world &&other) noexcept = delete;
        virtual ~game_world() noexcept;
        game_world &operator=(const game_world &other) noexcept = delete;
        game_world &operator=(game_world &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_world &other) noexcept;
        void schedule_event(const ft_sharedptr<game_event> &event) noexcept;
        void update_events(ft_sharedptr<game_world> &self, int32_t ticks, const char *log_file_path = ft_nullptr, ft_string *log_buffer = ft_nullptr) noexcept;

        ft_sharedptr<game_event_scheduler>       &get_event_scheduler() noexcept;
        const ft_sharedptr<game_event_scheduler> &get_event_scheduler() const noexcept;
        ft_sharedptr<game_world_registry>       &get_world_registry() noexcept;
        const ft_sharedptr<game_world_registry> &get_world_registry() const noexcept;
        ft_sharedptr<game_world_replay_session>       &get_replay_session() noexcept;
        const ft_sharedptr<game_world_replay_session> &get_replay_session() const noexcept;
        ft_sharedptr<game_economy_table>       &get_economy_table() noexcept;
        const ft_sharedptr<game_economy_table> &get_economy_table() const noexcept;
        ft_sharedptr<game_crafting>       &get_crafting() noexcept;
        const ft_sharedptr<game_crafting> &get_crafting() const noexcept;
        ft_sharedptr<game_dialogue_table>       &get_dialogue_table() noexcept;
        const ft_sharedptr<game_dialogue_table> &get_dialogue_table() const noexcept;
        ft_sharedptr<game_world_region>       &get_world_region() noexcept;
        const ft_sharedptr<game_world_region> &get_world_region() const noexcept;
        ft_sharedptr<game_quest>       &get_quest() noexcept;
        const ft_sharedptr<game_quest> &get_quest() const noexcept;
        ft_sharedptr<game_vendor_profile>       &get_vendor_profile() noexcept;
        const ft_sharedptr<game_vendor_profile> &get_vendor_profile() const noexcept;
        ft_sharedptr<game_upgrade>       &get_upgrade() noexcept;
        const ft_sharedptr<game_upgrade> &get_upgrade() const noexcept;

        int32_t save_to_file(const char *file_path, const game_character &character, const game_inventory &inventory) const noexcept;
        int32_t load_from_file(const char *file_path, game_character &character, game_inventory &inventory) noexcept;
        int32_t save_to_store(kv_store &store, const char *slot_key, const game_character &character, const game_inventory &inventory) const noexcept;
        int32_t load_from_store(kv_store &store, const char *slot_key, game_character &character, game_inventory &inventory) noexcept;
        int32_t save_to_buffer(ft_string &out_buffer, const game_character &character, const game_inventory &inventory) const noexcept;
        int32_t load_from_buffer(const char *buffer, game_character &character, game_inventory &inventory) noexcept;

        int32_t plan_route(const game_map3d &grid,
            ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
            ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
            ft_vector<game_path_step> &path) const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
