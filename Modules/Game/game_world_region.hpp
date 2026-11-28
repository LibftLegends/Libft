#ifndef FT_WORLD_REGION_HPP
# define FT_WORLD_REGION_HPP

#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_world_region
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t             _world_id;
        ft_vector<int32_t>  _region_ids;
        pt_recursive_mutex       *_mutex;
        uint8_t         _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_world_region() noexcept;
        virtual ~game_world_region() noexcept;
        game_world_region(const game_world_region &other) noexcept = delete;
        game_world_region &operator=(const game_world_region &other) = delete;
        game_world_region(game_world_region &&other) noexcept = delete;
        game_world_region &operator=(game_world_region &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_world_region &other) noexcept;
        int32_t initialize(game_world_region &&other) noexcept;
        // Moving requires exclusive access to both records and their vectors.
        int32_t move(game_world_region &other) noexcept;
        int32_t initialize(int32_t world_id, const ft_vector<int32_t> &region_ids) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_world_id() const noexcept;
        void set_world_id(int32_t world_id) noexcept;

        // Callers must synchronize direct access through these vector references.
        const ft_vector<int32_t> &get_region_ids() const noexcept;
        ft_vector<int32_t> &get_region_ids() noexcept;
        void set_region_ids(const ft_vector<int32_t> &region_ids) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
