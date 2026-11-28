#ifndef MATH_AUTODIFF_HPP
#define MATH_AUTODIFF_HPP

#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/move.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

class ft_dual_number_proxy;
class ft_dual_number;

typedef ft_dual_number *(*math_autodiff_univariate_function)(const ft_dual_number &input, void *user_data);
typedef ft_dual_number *(*math_autodiff_multivariate_function)(const ft_vector<ft_dual_number> &input, void *user_data);



class ft_dual_number
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        double          _value;
        double          _derivative;
        mutable pt_recursive_mutex *_mutex = ft_nullptr;
        uint8_t         _initialised_state = FT_CLASS_STATE_UNINITIALISED;
        int32_t             _operation_error = FT_ERR_SUCCESS;
        static thread_local int32_t _last_error;

        static int32_t      lock_pair(const ft_dual_number &first,
                            const ft_dual_number &second,
                            const ft_dual_number *&lower,
                            const ft_dual_number *&upper);
        static void     unlock_pair(const ft_dual_number *lower,
                            const ft_dual_number *upper);
        void            abort_lifecycle_error(const char *method_name,
                            const char *reason) const noexcept;
        void            abort_if_not_initialised(const char *method_name) const noexcept;
        static int32_t      set_error(int32_t error_code) noexcept;
        template <char Operation>
        ft_dual_number_proxy apply_binary(const ft_dual_number &other,
            const char *method_name) const noexcept;
        template <typename Operation>
        ft_dual_number *apply_unary(Operation operation, const char *method_name) const noexcept;
        int32_t read_result(double &value, double &derivative,
            const char *method_name = "ft_dual_number::read_result") const noexcept;
        friend int32_t math_autodiff_univariate(math_autodiff_univariate_function function,
            double point, double *value, double *derivative, void *user_data) noexcept;
        friend int32_t math_autodiff_gradient(math_autodiff_multivariate_function function,
            const ft_vector<double> &point, ft_vector<double> &gradient,
            double *value, void *user_data) noexcept;
        friend class ft_dual_number_proxy;

    public:
        ft_dual_number() noexcept;
        ft_dual_number(double value, double derivative) noexcept;
        ft_dual_number(const ft_dual_number &other) noexcept = delete;
        ft_dual_number(ft_dual_number &&other) noexcept = delete;
        ~ft_dual_number() noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(double value, double derivative) noexcept;
        int32_t initialize(const ft_dual_number &other) noexcept;
        int32_t initialize(ft_dual_number &&other) noexcept;
        int32_t             destroy() noexcept;
        int32_t             move(ft_dual_number &other) noexcept;

        ft_dual_number &operator=(const ft_dual_number &other) noexcept = delete;
        ft_dual_number &operator=(ft_dual_number &&other) noexcept = delete;

        static ft_dual_number   *constant(double value) noexcept;
        static ft_dual_number   *variable(double value) noexcept;

        double  value() const noexcept;
        double  derivative() const noexcept;

        ft_dual_number_proxy operator+(const ft_dual_number &other) const noexcept;
        ft_dual_number_proxy operator-(const ft_dual_number &other) const noexcept;
        ft_dual_number_proxy operator*(const ft_dual_number &other) const noexcept;
        ft_dual_number_proxy operator/(const ft_dual_number &other) const noexcept;

        ft_dual_number   *apply_sin() const noexcept;
        ft_dual_number   *apply_cos() const noexcept;
        ft_dual_number   *apply_exp() const noexcept;
        ft_dual_number   *apply_log() const noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

class ft_dual_number_proxy
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        friend class ft_dual_number;
        ft_dual_number_proxy(double value, double derivative) noexcept;
        ft_dual_number  _value;
        int32_t             _operation_error;

    public:
        ft_dual_number_proxy() noexcept;
        explicit ft_dual_number_proxy(int32_t error_code) noexcept;
        ft_dual_number_proxy(const ft_dual_number &value, int32_t error_code) noexcept;
        ft_dual_number_proxy(const ft_dual_number_proxy &other) noexcept;
        ft_dual_number_proxy(ft_dual_number_proxy &&other) noexcept;
        ~ft_dual_number_proxy();

        ft_dual_number_proxy &operator=(const ft_dual_number_proxy &other) noexcept = delete;
        ft_dual_number_proxy &operator=(ft_dual_number_proxy &&other) noexcept = delete;

        ft_dual_number_proxy operator+(const ft_dual_number &other) const noexcept;
        ft_dual_number_proxy operator-(const ft_dual_number &other) const noexcept;
        ft_dual_number_proxy operator*(const ft_dual_number &other) const noexcept;
        ft_dual_number_proxy operator/(const ft_dual_number &other) const noexcept;

        operator ft_dual_number *() const noexcept;
        int32_t get_error() const noexcept;
};

int32_t     math_autodiff_univariate(math_autodiff_univariate_function function,
            double point, double *value, double *derivative, void *user_data) noexcept;
int32_t     math_autodiff_gradient(math_autodiff_multivariate_function function,
            const ft_vector<double> &point, ft_vector<double> &gradient,
            double *value, void *user_data) noexcept;

#endif
