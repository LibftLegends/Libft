#include "rng_stream.hpp"
#include "../PThread/pthread_internal.hpp"
#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Math/math.hpp"
#include "../PThread/pthread.hpp"
#include <climits>
#include <cmath>
#include <limits>
#include <new>
#include "../Basic/limits.hpp"
#include "../Math/math_interval.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Basic/basic.hpp"

rng_stream::rng_stream() noexcept
    : _engine()
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

rng_stream::~rng_stream() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t rng_stream::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t rng_stream::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool rng_stream::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t rng_stream::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "rng_stream::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    ft_init_random_engine();
    const int32_t lock_error = rng_lock_random_engine_mutex();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_engine = g_random_engine;
    (void)rng_unlock_random_engine_mutex();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t rng_stream::initialize(const rng_stream &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "rng_stream::initialize(const rng_stream &) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_engine = other._engine;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t rng_stream::initialize(rng_stream &&other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "rng_stream::initialize(rng_stream &&) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_engine = other._engine;
    other._engine.seed();
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t rng_stream::move(rng_stream &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(
            static_cast<rng_stream &&>(other))));
}

int32_t rng_stream::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t rng_stream::reseed(uint32_t seed_value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::reseed");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_engine.seed(static_cast<std::mt19937::result_type>(seed_value));
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t rng_stream::reseed_from_string(const char *seed_string) noexcept
{
    uint32_t derived_seed;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::reseed_from_string");
    if (seed_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    derived_seed = ft_random_seed(seed_string);
    return (this->reseed(derived_seed));
}

int32_t rng_stream::random_int_unlocked()
{
    std::uniform_int_distribution<int32_t> distribution(0, std::numeric_limits<int32_t>::max());
    int32_t random_value;

    random_value = distribution(this->_engine);
    return (random_value);
}

float rng_stream::random_float_unlocked()
{
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    float random_value;

    random_value = distribution(this->_engine);
    return (random_value);
}

Pair<int32_t,  int32_t> rng_stream::random_int()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_int");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, int32_t>(lock_error, 0));
    const int32_t value = this->random_int_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, value));
}

Pair<int32_t,  int32_t> rng_stream::dice_roll(int32_t number, int32_t faces)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::dice_roll");
    if (faces == 0 && number == 0)
        return (Pair<int32_t,  int32_t>(FT_ERR_SUCCESS, 0));
    if (faces < 1 || number < 1)
        return (Pair<int32_t,  int32_t>(FT_ERR_INVALID_ARGUMENT, 0));
    if (faces == 1)
        return (Pair<int32_t,  int32_t>(FT_ERR_SUCCESS, number));
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, int32_t>(lock_error, 0));
    std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);
    int32_t result = 0;
    int32_t index = 0;
    while (index < number)
    {
        const int32_t roll = (distribution(this->_engine) % faces) + 1;
        if (result > INT32_MAX - roll)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return (Pair<int32_t, int32_t>(FT_ERR_OUT_OF_RANGE, 0));
        }
        result += roll;
        index += 1;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, result));
}

Pair<int32_t,  float> rng_stream::random_float()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_float");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, float>(lock_error, 0.0f));
    const float value = this->random_float_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, float>(FT_ERR_SUCCESS, value));
}

Pair<int32_t, float> rng_stream::random_normal()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_normal");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, float>(lock_error, 0.0f));
    float uniform_one = this->random_float_unlocked();
    const float uniform_two = this->random_float_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);

    constexpr float MIN_UNIFORM = 0.0000000001f;
    constexpr float PI_VALUE = 3.14159265358979323846f;
    if (uniform_one < MIN_UNIFORM)
        uniform_one = MIN_UNIFORM;
    const float radius = static_cast<float>(math_sqrt(-2.0 * math_log(uniform_one)));
    const float angle = 2.0f * PI_VALUE * uniform_two;
    return (Pair<int32_t, float>(FT_ERR_SUCCESS, radius * static_cast<float>(math_cos(angle))));
}

Pair<int32_t, float> rng_stream::random_exponential(float lambda_value)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_exponential");
    if (lambda_value <= 0.0f)
        return (Pair<int32_t, float>(FT_ERR_INVALID_ARGUMENT, 0.0f));
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, float>(lock_error, 0.0f));
    float uniform_value = this->random_float_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);

    constexpr float MIN_UNIFORM = 0.0000000001f;
    if (uniform_value < MIN_UNIFORM)
        uniform_value = MIN_UNIFORM;
    const float result = static_cast<float>(-math_log(uniform_value)) / lambda_value;
    return (Pair<int32_t, float>(FT_ERR_SUCCESS, result));
}

Pair<int32_t,  int32_t> rng_stream::random_poisson(double lambda_value)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_poisson");
    const int32_t rate_error = rng_poisson_rate_error(lambda_value);
    if (rate_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, int32_t>(rate_error, 0));
    if (lambda_value > RNG_POISSON_PRODUCT_MAX_RATE)
    {
        std::poisson_distribution<int64_t> distribution(lambda_value);
        const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (Pair<int32_t, int32_t>(lock_error, 0));
        const int64_t sample = distribution(this->_engine);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        if (sample > INT32_MAX)
            return (Pair<int32_t, int32_t>(FT_ERR_OUT_OF_RANGE, 0));
        return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, static_cast<int32_t>(sample)));
    }
    const double limit_value = math_exp(-lambda_value);
    if (limit_value >= 1.0)
        return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, 0));
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, int32_t>(lock_error, 0));
    double product_value = 1.0;
    int32_t sample = -1;
    while (product_value > limit_value)
    {
        product_value *= static_cast<double>(this->random_float_unlocked());
        sample += 1;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, sample));
}

Pair<int32_t, int32_t> rng_stream::random_binomial(int32_t trial_count, double success_probability)
{
    constexpr double PROBABILITY_EPSILON = std::numeric_limits<double>::epsilon();
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_binomial");
    if (trial_count < 0 || success_probability < 0.0)
        return (Pair<int32_t, int32_t>(FT_ERR_INVALID_ARGUMENT, 0));
    if (trial_count == 0)
        return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, 0));
    if (success_probability > 1.0)
        return (Pair<int32_t, int32_t>(FT_ERR_INVALID_ARGUMENT, 0));
    if (success_probability <= PROBABILITY_EPSILON)
        return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, 0));
    if ((1.0 - success_probability) <= PROBABILITY_EPSILON)
        return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, trial_count));
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, int32_t>(lock_error, 0));
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    int32_t trial_index = 0;
    int32_t success_count = 0;
    while (trial_index < trial_count)
    {
        if (static_cast<double>(distribution(this->_engine)) < success_probability)
            success_count += 1;
        trial_index += 1;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, success_count));
}

Pair<int32_t, int32_t> rng_stream::random_geometric(double success_probability)
{
    constexpr double PROBABILITY_EPSILON = std::numeric_limits<double>::epsilon();
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_geometric");
    if (!(success_probability > 0.0 && success_probability <= 1.0))
        return (Pair<int32_t, int32_t>(FT_ERR_INVALID_ARGUMENT, 0));
    if ((1.0 - success_probability) <= PROBABILITY_EPSILON)
        return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, 1));
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, int32_t>(lock_error, 0));
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    int32_t trial_count = 0;
    while (trial_count < INT32_MAX)
    {
        trial_count += 1;
        if (static_cast<double>(distribution(this->_engine)) < success_probability)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return (Pair<int32_t, int32_t>(FT_ERR_SUCCESS, trial_count));
        }
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, int32_t>(FT_ERR_OUT_OF_RANGE, 0));
}

Pair<int32_t,  float> rng_stream::random_gamma(float shape, float scale)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_gamma");
    if (!(shape > 0.0f && scale > 0.0f) || !std::isfinite(shape) || !std::isfinite(scale))
        return (Pair<int32_t, float>(FT_ERR_INVALID_ARGUMENT, 0.0f));
    std::gamma_distribution<float> distribution(shape, scale);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, float>(lock_error, 0.0f));
    const float sample_value = distribution(this->_engine);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (Pair<int32_t, float>(FT_ERR_SUCCESS, sample_value));
}

Pair<int32_t,  float> rng_stream::random_beta(float alpha, float beta)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_beta");
    if (!(alpha > 0.0f && beta > 0.0f) || !std::isfinite(alpha) || !std::isfinite(beta))
        return (Pair<int32_t, float>(FT_ERR_INVALID_ARGUMENT, 0.0f));
    std::gamma_distribution<float> alpha_distribution(alpha, 1.0f);
    std::gamma_distribution<float> beta_distribution(beta, 1.0f);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, float>(lock_error, 0.0f));
    const float alpha_sample = alpha_distribution(this->_engine);
    const float beta_sample = beta_distribution(this->_engine);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    const float sum = alpha_sample + beta_sample;
    if (sum <= 0.0f)
        return (Pair<int32_t, float>(FT_ERR_INVALID_STATE, 0.0f));
    float result = alpha_sample / sum;
    if (result < 0.0f)
        result = 0.0f;
    if (result > 1.0f)
        result = 1.0f;
    return (Pair<int32_t, float>(FT_ERR_SUCCESS, result));
}

Pair<int32_t,  float> rng_stream::random_chi_squared(float degrees_of_freedom)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "rng_stream::random_chi_squared");
    Pair<int32_t, float> result = this->random_gamma(degrees_of_freedom * 0.5f, 2.0f);
    if (result.value < 0.0f)
        result.value = 0.0f;
    return (result);
}

#ifdef LIBFT_TEST_BUILD
pt_recursive_mutex *rng_stream::get_mutex_for_validation() const
{
    return (this->_mutex);
}
#endif
