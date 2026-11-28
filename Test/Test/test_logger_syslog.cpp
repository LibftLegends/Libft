#include "../test_internal.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_size_t logger_syslog_registration_count()
{
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return (FT_SYSTEM_SIZE_MAX);
    const ft_size_t count = g_sinks.size();
    ft_size_t index = 0U;
    ft_size_t syslog_count = 0U;
    while (index < count)
    {
        if (g_sinks[index].function == ft_syslog_sink)
            syslog_count += 1U;
        index += 1U;
    }
    (void)logger_unlock_sinks();
    return (syslog_count);
}

#if defined(_WIN32) || defined(_WIN64)
FT_TEST(test_logger_syslog_unsupported_platform_preserves_registry)
{
    const ft_size_t before_count = logger_syslog_registration_count();
    FT_ASSERT_EQ(FT_ERR_INTERNAL, ft_log_set_syslog("libft"));
    FT_ASSERT_EQ(before_count, logger_syslog_registration_count());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_syslog_sink(ft_nullptr, ft_nullptr));
    return (1);
}
#else
FT_TEST(test_logger_syslog_owns_configured_identifier)
{
    char identifier[] = "libft-owned";
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_set_syslog(identifier));
    identifier[0] = 'X';
    const int32_t write_error = ft_syslog_sink("verify owned syslog identifier", ft_nullptr);
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_error);
    return (1);
}

FT_TEST(test_logger_syslog_reconfigures_existing_sink)
{
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_set_syslog("libft"));
    const int32_t repeat_error = ft_log_set_syslog("libft");
    const int32_t replacement_error = ft_log_set_syslog("libft-reconfigured");
    const int32_t before_write = ft_syslog_sink("verify reconfigured syslog identifier", ft_nullptr);
    const int32_t default_error = ft_log_set_syslog(ft_nullptr);
    const int32_t after_write = ft_syslog_sink("verify default syslog identifier", ft_nullptr);
    const ft_size_t active_count = logger_syslog_registration_count();
    ft_log_remove_sink(ft_syslog_sink, ft_nullptr);
    const ft_size_t removed_count = logger_syslog_registration_count();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, repeat_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, before_write);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, default_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, after_write);
    FT_ASSERT_EQ(1U, active_count);
    FT_ASSERT_EQ(0U, removed_count);
    return (1);
}

FT_TEST(test_logger_retired_syslog_sink_cannot_close_replacement)
{
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_set_syslog("libft-retiring"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_lock_sinks());
    s_log_sink retired = g_sinks[0];
    retired.registration->references.fetch_add(1U, std::memory_order_relaxed);
    (void)logger_unlock_sinks();

    ft_log_close();
    const int32_t replacement_error = ft_log_set_syslog("libft-replacement");
    logger_release_sink(retired);
    const int32_t write_error = ft_syslog_sink("verify surviving syslog identifier", ft_nullptr);
    const int32_t repeat_error = ft_log_set_syslog("libft-replacement");
    const ft_size_t active_count = logger_syslog_registration_count();
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, repeat_error);
    FT_ASSERT_EQ(1U, active_count);
    return (1);
}
#endif
