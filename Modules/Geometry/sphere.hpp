#ifndef GEOMETRY_SPHERE_HPP
# define GEOMETRY_SPHERE_HPP

#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"

class vector3;
struct geometry_aabb3;
struct geometry_sweep_hit;

class sphere
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        double                    _center_x;
        double                    _center_y;
        double                    _center_z;
        double                    _radius;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                   _initialised_state;

        ft_bool snapshot(double (&center)[3], double &radius) const noexcept;
        friend ft_bool geometry_sphere_intersect_aabb3(const sphere &shape,
                    const geometry_aabb3 &box);
        friend ft_bool geometry_swept_sphere_intersect_aabb3(const sphere &shape,
                    const vector3 &velocity, const geometry_aabb3 &box,
                    geometry_sweep_hit *hit);
        uint32_t release_mutex() noexcept;

    public:
        sphere() noexcept;
        sphere(const sphere &other) noexcept = delete;
        sphere(sphere &&other) noexcept = delete;
        ~sphere() noexcept;

        sphere &operator=(const sphere &other) = delete;
        sphere &operator=(sphere &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(double center_x, double center_y, double center_z,
                double radius) noexcept;
        int32_t initialize(const sphere &other) noexcept;
        int32_t initialize(sphere &&other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(sphere &other) noexcept;

        uint32_t set_center(double center_x, double center_y,
                double center_z);
        uint32_t set_center_x(double center_x);
        uint32_t set_center_y(double center_y);
        uint32_t set_center_z(double center_z);
        uint32_t set_radius(double radius);
        double  get_center_x() const;
        double  get_center_y() const;
        double  get_center_z() const;
        double  get_radius() const;
        uint32_t enable_thread_safety() noexcept;
        uint32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        friend ft_bool intersect_sphere(const sphere &first,
                const sphere &second);
};

#endif
