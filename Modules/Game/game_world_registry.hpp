#ifndef GAME_WORLD_REGISTRY_HPP
# define GAME_WORLD_REGISTRY_HPP

#include "game_region_definition.hpp"
#include "game_world_region.hpp"
#include "../Template/map.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_world_registry
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, game_region_definition> _regions;
        ft_map<int32_t, game_world_region>      _world_regions;
        pt_recursive_mutex                         *_mutex;
        uint8_t                           _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        ft_bool has_active_lock() const noexcept;

    public:
        game_world_registry() noexcept;
        virtual ~game_world_registry() noexcept;
        game_world_registry(const game_world_registry &other) noexcept = delete;
        game_world_registry &operator=(const game_world_registry &other) noexcept = delete;
        game_world_registry(game_world_registry &&other) noexcept = delete;
        game_world_registry &operator=(game_world_registry &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_world_registry &other) noexcept;
        int32_t initialize(game_world_registry &&other) noexcept;
        // Moving requires exclusive access to both registries and their maps.
        int32_t move(game_world_registry &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t register_region(const game_region_definition &region) noexcept;
        int32_t register_world(const game_world_region &world_region) noexcept;

        int32_t fetch_region(int32_t region_id, game_region_definition &out_region) const noexcept;
        int32_t fetch_world(int32_t world_id, game_world_region &out_world) const noexcept;

        // Callers must synchronize direct access through these map references.
        ft_map<int32_t, game_region_definition> &get_regions() noexcept;
        const ft_map<int32_t, game_region_definition> &get_regions() const noexcept;
        void set_regions(const ft_map<int32_t, game_region_definition> &regions) noexcept;

        ft_map<int32_t, game_world_region> &get_world_regions() noexcept;
        const ft_map<int32_t, game_world_region> &get_world_regions() const noexcept;
        void set_world_regions(const ft_map<int32_t, game_world_region> &world_regions) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
