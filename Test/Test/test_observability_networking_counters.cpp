#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Observability/observability.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static int32_t test_observability_socket_hook(int32_t domain_value,
    int32_t type_value, int32_t protocol_value)
{
    (void)domain_value;
    (void)type_value;
    (void)protocol_value;
    return (42);
}

FT_TEST(test_observability_networking_socket_counter)
{
    ft_observability_counters counters;
    int32_t socket_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        observability_reset_counters(FT_OBSERVABILITY_MODULE_NETWORKING));
    nw_set_socket_hook(&test_observability_socket_hook);
    socket_result = nw_socket(0, 0, 0);
    nw_set_socket_hook(ft_nullptr);

    FT_ASSERT_EQ(42, socket_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        observability_get_counters(FT_OBSERVABILITY_MODULE_NETWORKING, &counters));
    FT_ASSERT_EQ(1, counters.operations);
    FT_ASSERT_EQ(1, counters.successes);
    FT_ASSERT_EQ(0, counters.failures);
    return (1);
}

FT_TEST(test_observability_networking_counters_record_zero_and_nonzero_bytes)
{
    constexpr ft_observability_module MODULE = FT_OBSERVABILITY_MODULE_NETWORKING;
    ft_observability_counters counters{};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_reset_counters(MODULE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_record_operation(MODULE, FT_TRUE, 0, 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_record_operation(MODULE, FT_TRUE, 7, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_record_operation(MODULE, FT_FALSE, 0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_record_operation(MODULE, FT_TRUE, 11, 13));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_get_counters(MODULE, &counters));
    FT_ASSERT_EQ(4, counters.operations);
    FT_ASSERT_EQ(3, counters.successes);
    FT_ASSERT_EQ(1, counters.failures);
    FT_ASSERT_EQ(18, counters.bytes_read);
    FT_ASSERT_EQ(18, counters.bytes_written);
    return (1);
}
