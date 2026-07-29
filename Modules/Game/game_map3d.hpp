#ifndef GAME_MAP3D_HPP
# define GAME_MAP3D_HPP

#include <cstddef>
#include <stdint.h>
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

class game_pathfinding;

class game_map3d
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t       ***_data;
        ft_size_t      _width;
        ft_size_t      _height;
        ft_size_t      _depth;
        int32_t         _initial_value;
        pt_recursive_mutex   *_mutex;
        uint8_t     _initialised_state;
        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t     lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t     unlock_internal(ft_bool lock_acquired) const noexcept;

        int32_t allocate(ft_size_t width, ft_size_t height, ft_size_t depth, int32_t value);
        void    deallocate();

    public:
        game_map3d();
        game_map3d(const game_map3d &other) = delete;
        game_map3d(game_map3d &&other) = delete;
        ~game_map3d() noexcept;
        game_map3d &operator=(const game_map3d &other) = delete;
        game_map3d &operator=(game_map3d &&other) = delete;

        int32_t     initialize() noexcept;
        int32_t     initialize(ft_size_t width, ft_size_t height, ft_size_t depth, int32_t value) noexcept;
        int32_t     move(game_map3d &other) noexcept;
        int32_t     destroy() noexcept;
        int32_t     enable_thread_safety() noexcept;
        int32_t     disable_thread_safety() noexcept;
        ft_bool    is_thread_safe() const noexcept;
        int32_t     lock(ft_bool *lock_acquired) const noexcept;
        void    unlock(ft_bool lock_acquired) const noexcept;

        int32_t     get_error() const noexcept;
        const char *get_error_str() const noexcept;

        void    resize(ft_size_t width, ft_size_t height, ft_size_t depth, int32_t value = 0);
        int32_t     get(ft_size_t x, ft_size_t y, ft_size_t z) const;
        void    set(ft_size_t x, ft_size_t y, ft_size_t z, int32_t value);
        ft_bool    is_obstacle(ft_size_t x, ft_size_t y, ft_size_t z) const;
        void    toggle_obstacle(ft_size_t x, ft_size_t y, ft_size_t z,
                    game_pathfinding *listener = ft_nullptr);
        ft_size_t  get_width() const;
        ft_size_t  get_height() const;
        ft_size_t  get_depth() const;

};

#endif
