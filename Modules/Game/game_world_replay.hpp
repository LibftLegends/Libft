#ifndef GAME_WORLD_REPLAY_HPP
# define GAME_WORLD_REPLAY_HPP

#include "game_world.hpp"
#include "game_character.hpp"
#include "game_inventory.hpp"
#include "game_pathfinding.hpp"
#include "../Template/shared_ptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Template/vector.hpp"
#include "../Template/function.hpp"
#include <stdint.h>

class game_world_replay_session
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_string   _snapshot_payload;
        ft_vector<ft_function<void(game_world&, game_event&)> > _event_callbacks;
        uint8_t     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_world_replay_session() noexcept;
        ~game_world_replay_session() noexcept;
        game_world_replay_session(const game_world_replay_session &other) noexcept = delete;
        game_world_replay_session &operator=(const game_world_replay_session &other) noexcept = delete;
        game_world_replay_session(game_world_replay_session &&other) noexcept = delete;
        game_world_replay_session &operator=(game_world_replay_session &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t move(game_world_replay_session &other) noexcept;
        int32_t destroy() noexcept;
        int32_t capture_snapshot(game_world &world, const game_character &character, const game_inventory &inventory) noexcept;
        int32_t restore_snapshot(ft_sharedptr<game_world> &world_ptr, game_character &character, game_inventory &inventory) noexcept;
        int32_t replay_ticks(ft_sharedptr<game_world> &world_ptr, game_character &character, game_inventory &inventory, int32_t ticks,
            const char *log_file_path = ft_nullptr, ft_string *log_buffer = ft_nullptr) noexcept;
        int32_t plan_route(game_world &world, const game_map3d &grid,
            ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
            ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
            ft_vector<game_path_step> &path) noexcept;
        int32_t import_snapshot(const ft_string &snapshot_payload) noexcept;
        int32_t export_snapshot(ft_string &out_snapshot) const noexcept;
        void clear_snapshot() noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
