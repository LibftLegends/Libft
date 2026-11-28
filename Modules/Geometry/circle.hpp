#ifndef GEOMETRY_CIRCLE_HPP
# define GEOMETRY_CIRCLE_HPP

#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"

class circle
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        double                    _center_x;
        double                    _center_y;
        double                    _radius;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                   _initialised_state;

        uint32_t release_mutex() noexcept;

    public:
        circle() noexcept;
        circle(const circle &other) noexcept = delete;
        circle(circle &&other) noexcept = delete;
        ~circle() noexcept;

        circle &operator=(const circle &other) = delete;
        circle &operator=(circle &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(double center_x, double center_y,
                double radius) noexcept;
        int32_t initialize(const circle &other) noexcept;
        int32_t initialize(circle &&other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(circle &other) noexcept;

        uint32_t set_center(double center_x, double center_y);
        uint32_t set_center_x(double center_x);
        uint32_t set_center_y(double center_y);
        uint32_t set_radius(double radius);
        double  get_center_x() const;
        double  get_center_y() const;
        double  get_radius() const;
        uint32_t enable_thread_safety() noexcept;
        uint32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        friend ft_bool intersect_circle(const circle &first,
                const circle &second);
};

#endif
