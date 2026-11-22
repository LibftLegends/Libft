#include "../test_internal.hpp"
#include "../../Modules/Template/function.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <memory>

#include "../../Modules/Basic/class_nullptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_function_default_empty_callable)
{
    ft_function<int(int)> empty_function;

    FT_ASSERT_EQ(false, static_cast<bool>(empty_function));
    FT_ASSERT_EQ(0, empty_function(7));
    FT_ASSERT_EQ(false, static_cast<bool>(empty_function));
    return (1);
}

FT_TEST(test_ft_function_invokes_callable)
{
    ft_function<int(int)> callable([](int value) -> int {
        return (value * 2);
    });

    FT_ASSERT_EQ(true, static_cast<bool>(callable));
    FT_ASSERT_EQ(10, callable(5));
    return (1);
}

FT_TEST(test_ft_function_dispatch_does_not_copy_the_argument_twice)
{
    std::shared_ptr<uint32_t> owner = std::make_shared<uint32_t>(17U);
    ft_function<int64_t(std::shared_ptr<uint32_t>)> function(
        [](const std::shared_ptr<uint32_t> &argument)
        {
            return (static_cast<int64_t>(argument.use_count()));
        });
    FT_ASSERT_EQ(2, function(owner));
    FT_ASSERT_EQ(1, owner.use_count());
    return (1);
}

FT_TEST(test_ft_function_dispatch_preserves_an_explicit_rvalue_reference)
{
    auto owner = std::make_unique<uint32_t>(17U);
    ft_function<uint32_t(std::unique_ptr<uint32_t> &&)> function(
        [](std::unique_ptr<uint32_t> &&argument)
        {
            auto owned_value = ft_move(argument);
            return (*owned_value);
        });
    FT_ASSERT_EQ(17U, function(ft_move(owner)));
    FT_ASSERT(owner == nullptr);
    return (1);
}
