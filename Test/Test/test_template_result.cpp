#include "../test_internal.hpp"
#include "../../Modules/Template/result.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static_assert(ft_status::success().is_success() == FT_TRUE);
static_assert(ft_status::success().is_error() == FT_FALSE);
static_assert(ft_status::failure(FT_ERR_NOT_FOUND).error_code == FT_ERR_NOT_FOUND);
static_assert(ft_status::failure(FT_ERR_SUCCESS).error_code == FT_ERR_INVALID_ARGUMENT);
static_assert(ft_status::failure(FT_ERR_SUCCESS).is_error() == FT_TRUE);

class result_counter_value
{
    private:
        int32_t *_destructor_count;
        int32_t _value;

    public:
        result_counter_value() noexcept;
        result_counter_value(int32_t value, int32_t *destructor_count) noexcept;
        result_counter_value(const result_counter_value &other) noexcept;
        result_counter_value(result_counter_value &&other) noexcept;
        ~result_counter_value() noexcept;

        result_counter_value &operator=(const result_counter_value &other) noexcept = delete;
        result_counter_value &operator=(result_counter_value &&other) noexcept = delete;

        int32_t get_value() const noexcept;
};

result_counter_value::result_counter_value() noexcept
    : _destructor_count(ft_nullptr), _value(0)
{
    return ;
}

result_counter_value::result_counter_value(int32_t value,
    int32_t *destructor_count) noexcept
    : _destructor_count(destructor_count), _value(value)
{
    return ;
}

result_counter_value::result_counter_value(
    const result_counter_value &other) noexcept
    : _destructor_count(other._destructor_count), _value(other._value)
{
    return ;
}

result_counter_value::result_counter_value(result_counter_value &&other) noexcept
    : _destructor_count(other._destructor_count), _value(other._value)
{
    other._destructor_count = ft_nullptr;
    return ;
}

result_counter_value::~result_counter_value() noexcept
{
    if (this->_destructor_count != ft_nullptr)
        *this->_destructor_count += 1;
    return ;
}

int32_t result_counter_value::get_value() const noexcept
{
    return (this->_value);
}

FT_TEST(test_ft_status_success_and_failure)
{
    ft_status success_status;
    ft_status failure_status;

    success_status = ft_status::success();
    failure_status = ft_status::failure(FT_ERR_NOT_FOUND);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, success_status.error_code);
    FT_ASSERT_EQ(FT_TRUE, success_status.is_success());
    FT_ASSERT_EQ(FT_FALSE, success_status.is_error());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, failure_status.error_code);
    FT_ASSERT_EQ(FT_FALSE, failure_status.is_success());
    FT_ASSERT_EQ(FT_TRUE, failure_status.is_error());
    return (1);
}

FT_TEST(test_ft_result_value_access)
{
    ft_result<int32_t> result;
    int32_t *value_pointer;

    FT_ASSERT_EQ(FT_FALSE, result.has_value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.initialize_value(123));
    FT_ASSERT_EQ(FT_TRUE, result.has_value());
    FT_ASSERT_EQ(FT_TRUE, result.is_success());
    value_pointer = result.get_value();
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT_EQ(123, *value_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.destroy());
    return (1);
}

FT_TEST(test_ft_result_error_has_no_value)
{
    ft_result<int32_t> result;

    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, result.initialize_error(FT_ERR_NOT_FOUND));
    FT_ASSERT_EQ(FT_FALSE, result.has_value());
    FT_ASSERT_EQ(FT_TRUE, result.is_error());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, result.get_error());
    FT_ASSERT(result.get_value() == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.destroy());
    return (1);
}

FT_TEST(test_ft_result_success_factory)
{
    ft_result<int32_t> *result;

    result = ft_result<int32_t>::success(77);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, result->has_value());
    FT_ASSERT(result->get_value() != ft_nullptr);
    FT_ASSERT_EQ(77, *result->get_value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_ft_result_failure_factory_rejects_success_error)
{
    ft_result<int32_t> *result;

    result = ft_result<int32_t>::failure(FT_ERR_SUCCESS);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_FALSE, result->has_value());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_ft_result_destroy_releases_previous_value)
{
    int32_t destructor_count;

    destructor_count = 0;
    {
        ft_result<result_counter_value> result;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, result.initialize_value(
            result_counter_value(42, &destructor_count)));
        FT_ASSERT_EQ(0, destructor_count);
        FT_ASSERT(result.get_value() != ft_nullptr);
        FT_ASSERT_EQ(42, result.get_value()->get_value());
        FT_ASSERT_EQ(FT_ERR_NOT_FOUND, result.initialize_error(FT_ERR_NOT_FOUND));
        FT_ASSERT_EQ(1, destructor_count);
        FT_ASSERT_EQ(FT_FALSE, result.has_value());
    }
    FT_ASSERT_EQ(1, destructor_count);
    return (1);
}
