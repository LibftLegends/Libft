#ifndef GEOMETRY_AABB_HPP
# define GEOMETRY_AABB_HPP

#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"

class aabb
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        double                    _minimum_x;
        double                    _minimum_y;
        double                    _maximum_x;
        double                    _maximum_y;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                   _initialised_state;

        uint32_t release_mutex() noexcept;

    public:
        aabb() noexcept;
        aabb(const aabb &other) noexcept = delete;
        aabb(aabb &&other) noexcept = delete;
        ~aabb() noexcept;

        aabb &operator=(const aabb &other) = delete;
        aabb &operator=(aabb &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(double minimum_x, double minimum_y,
                double maximum_x, double maximum_y) noexcept;
        int32_t initialize(const aabb &other) noexcept;
        int32_t initialize(aabb &&other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(aabb &other) noexcept;

        uint32_t set_bounds(double minimum_x, double minimum_y,
                double maximum_x, double maximum_y);
        uint32_t set_minimum(double minimum_x, double minimum_y);
        uint32_t set_minimum_x(double minimum_x);
        uint32_t set_minimum_y(double minimum_y);
        uint32_t set_maximum(double maximum_x, double maximum_y);
        uint32_t set_maximum_x(double maximum_x);
        uint32_t set_maximum_y(double maximum_y);
        double  get_minimum_x() const;
        double  get_minimum_y() const;
        double  get_maximum_x() const;
        double  get_maximum_y() const;
        uint32_t enable_thread_safety() noexcept;
        uint32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        friend ft_bool intersect_aabb(const aabb &first, const aabb &second);
};

#endif
