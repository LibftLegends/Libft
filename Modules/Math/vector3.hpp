#ifndef VECTOR3_HPP
# define VECTOR3_HPP

#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cstdint>

class matrix3;

class vector3
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        double _x_component;
        double _y_component;
        double _z_component;
        pt_recursive_mutex *_mutex = ft_nullptr;
        uint8_t _initialised_state = FT_CLASS_STATE_UNINITIALISED;
        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;

        friend class matrix3;
        void abort_lifecycle_error(const char *method_name,
                const char *reason) const noexcept;
        void abort_if_not_initialised(const char *method_name) const noexcept;

        static int32_t lock_pair(const vector3 &first, const vector3 &second,
                const vector3 *&lower, const vector3 *&upper);
        static void unlock_pair(const vector3 *lower, const vector3 *upper);

    public:
        vector3() noexcept;
        vector3(double x_component, double y_component, double z_component) noexcept;
        vector3(const vector3 &other) noexcept = delete;
        vector3 &operator=(const vector3 &other) = delete;
        vector3(vector3 &&other) noexcept = delete;
        vector3 &operator=(vector3 &&other) = delete;
        ~vector3() noexcept;
        int32_t initialize() noexcept;
        int32_t initialize(double x_component, double y_component, double z_component) noexcept;
        int32_t initialize(const vector3 &other) noexcept;
        int32_t initialize(vector3 &&other) noexcept;
        uint32_t destroy() noexcept;
        int32_t move(vector3 &other) noexcept;
        double  get_x() const;
        double  get_y() const;
        double  get_z() const;
        int32_t get_components(double &x, double &y, double &z) const noexcept;
        vector3 add(const vector3 &other) const;
        vector3 subtract(const vector3 &other) const;
        double  dot(const vector3 &other) const;
        vector3 cross(const vector3 &other) const;
        double  length() const;
        vector3 normalize() const;
        uint32_t enable_thread_safety() noexcept;
        uint32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
