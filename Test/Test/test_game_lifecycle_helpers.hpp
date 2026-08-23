#ifndef TEST_GAME_LIFECYCLE_HELPERS_HPP
#define TEST_GAME_LIFECYCLE_HELPERS_HPP

#include "../test_internal.hpp"

template <typename TypeName>
static int32_t expect_game_lifecycle_sigabrt(void (*operation)(TypeName &))
{
    return (test_expect_sigabrt_signal_uninitialised<TypeName>(operation));
}

template <typename TypeName>
static int32_t run_game_lifecycle_default(void (*operation)(TypeName &))
{
    TypeName value;

    operation(value);
    return (0);
}

#endif
