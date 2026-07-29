#ifndef GAME_PATHFINDING_HPP
# define GAME_PATHFINDING_HPP

#include "game_map3d.hpp"
#include "../Template/vector.hpp"
#include "../Template/graph.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_path_step
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_size_t              _x;
        ft_size_t              _y;
        ft_size_t              _z;
        pt_recursive_mutex *_mutex;
        uint8_t             _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t     lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t     unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_path_step() noexcept;
        game_path_step(const game_path_step &other) noexcept = delete;
        game_path_step(game_path_step &&other) noexcept = delete;
        ~game_path_step() noexcept;
        game_path_step &operator=(const game_path_step &other) noexcept = delete;
        game_path_step &operator=(game_path_step &&other) noexcept = delete;

        int32_t     initialize() noexcept;
        int32_t     initialize(const game_path_step &other) noexcept;
        int32_t     initialize(game_path_step &&other) noexcept;
        int32_t     move(game_path_step &other) noexcept;
        int32_t     destroy() noexcept;
        int32_t     enable_thread_safety() noexcept;
        int32_t     disable_thread_safety() noexcept;
        ft_bool    is_thread_safe() const noexcept;

        int32_t     set_coordinates(ft_size_t x, ft_size_t y, ft_size_t z) noexcept;
        int32_t     set_x(ft_size_t x) noexcept;
        int32_t     set_y(ft_size_t y) noexcept;
        int32_t     set_z(ft_size_t z) noexcept;
        ft_size_t  get_x() const noexcept;
        ft_size_t  get_y() const noexcept;
        ft_size_t  get_z() const noexcept;
        int32_t     get_error() const noexcept;
        const char *get_error_str() const noexcept;

#ifdef LIBFT_TEST_BUILD
        friend class game_path_step_test_helper;
#endif
};

#ifdef LIBFT_TEST_BUILD

class game_path_step_test_helper
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        static int32_t ensure_thread_safe(game_path_step &step) noexcept;

    public:
        static int32_t lock(game_path_step &step) noexcept;
        static int32_t unlock(game_path_step &step) noexcept;
        static ft_bool is_locked(const game_path_step &step) noexcept;
        static ft_bool is_owned_by_thread(const game_path_step &step,
            pt_thread_id_type thread_id) noexcept;
        static int32_t get_mutex_error(const game_path_step &step) noexcept;
};

#endif

class game_pathfinding
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_vector<game_path_step> _current_path;
        ft_bool                    _needs_replan;
        pt_recursive_mutex               *_mutex;
        uint8_t                 _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t     lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t     unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_pathfinding() noexcept;
        game_pathfinding(const game_pathfinding &other) = delete;
        game_pathfinding(game_pathfinding &&other) = delete;
        ~game_pathfinding() noexcept;
        game_pathfinding &operator=(const game_pathfinding &other) = delete;
        game_pathfinding &operator=(game_pathfinding &&other) = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_pathfinding &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t astar_grid(const game_map3d &grid,
            ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
            ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
            ft_vector<game_path_step> &out_path) const noexcept;

        int32_t dijkstra_graph(const ft_graph<int32_t> &graph,
            ft_size_t start_vertex, ft_size_t goal_vertex,
            ft_vector<ft_size_t> &out_path) const noexcept;

        void    update_obstacle(ft_size_t x, ft_size_t y, ft_size_t z, int32_t value) noexcept;
        int32_t     recalculate_path(const game_map3d &grid,
            ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
            ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
            ft_vector<game_path_step> &out_path) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
