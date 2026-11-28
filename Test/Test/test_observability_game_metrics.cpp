#include "../test_internal.hpp"
#include "../../Modules/Observability/observability_game_metrics.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include <atomic>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

static ft_game_observability_sample g_game_last_sample;
static int g_game_export_count = 0;

static bool observability_game_strings_equal(const char *left, const char *right)
{
    if (left == ft_nullptr && right == ft_nullptr)
        return (true);
    if (left == ft_nullptr || right == ft_nullptr)
        return (false);
    return (ft_strncmp(left, right, ft_strlen(left) + 1) == 0);
}

static void observability_game_reset_last_sample(void)
{
    g_game_last_sample.labels.event_name = ft_nullptr;
    g_game_last_sample.labels.entity = ft_nullptr;
    g_game_last_sample.labels.attribute = ft_nullptr;
    g_game_last_sample.delta_value = 0;
    g_game_last_sample.total_value = 0;
    g_game_last_sample.unit = ft_nullptr;
    g_game_last_sample.error_code = 0;
    g_game_last_sample.error_tag = ft_nullptr;
    g_game_last_sample.success = false;
    return ;
}

static void observability_game_test_exporter(const ft_game_observability_sample &sample)
{
    g_game_export_count = g_game_export_count + 1;
    g_game_last_sample = sample;
    return ;
}

static int32_t g_game_exporter_shutdown_result;

static void observability_game_shutdown_exporter(const ft_game_observability_sample &sample)
{
    observability_game_test_exporter(sample);
    g_game_exporter_shutdown_result = observability_game_metrics_shutdown();
    return ;
}

FT_TEST(test_observability_game_exporter_can_shut_down_reentrantly)
{
    const ft_game_observability_sample sample{{"event", "entity", "attribute"}, 1, 1,
        "unit", FT_ERR_SUCCESS, ft_nullptr, FT_TRUE};
    g_game_export_count = 0;
    g_game_exporter_shutdown_result = FT_ERR_INTERNAL;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_initialize(observability_game_shutdown_exporter));
    FT_ASSERT_EQ(FT_TRUE, observability_game_metrics_is_thread_safe());
    observability_game_metrics_record(sample);
    observability_game_metrics_record(sample);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, g_game_exporter_shutdown_result);
    FT_ASSERT_EQ(FT_FALSE, observability_game_metrics_is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_initialize(observability_game_test_exporter));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, observability_game_metrics_is_thread_safe());
    observability_game_metrics_record(sample);
    FT_ASSERT_EQ(2, g_game_export_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_shutdown());
    return (1);
}

static std::atomic<uint32_t> g_concurrent_game_samples{0};

static void count_concurrent_game_sample(const ft_game_observability_sample &)
{
    g_concurrent_game_samples.fetch_add(1, std::memory_order_relaxed);
    return ;
}

FT_TEST(test_observability_game_records_during_exporter_reconfiguration)
{
    const ft_game_observability_sample sample{{"event", "entity", "attribute"}, 1, 1,
        "unit", FT_ERR_SUCCESS, ft_nullptr, FT_TRUE};
    g_concurrent_game_samples.store(0, std::memory_order_relaxed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_initialize(count_concurrent_game_sample));
    std::atomic<ft_bool> started{FT_FALSE};
    std::atomic<ft_bool> stopped{FT_FALSE};
    std::thread reader([&]()
    {
        observability_game_metrics_record(sample);
        started.store(FT_TRUE, std::memory_order_release);
        while (!stopped.load(std::memory_order_acquire))
            observability_game_metrics_record(sample);
        return ;
    });
    while (!started.load(std::memory_order_acquire))
        std::this_thread::yield();
    uint32_t iteration = 0;
    int32_t result = FT_ERR_SUCCESS;
    while (iteration < 1000 && result == FT_ERR_SUCCESS)
    {
        result = observability_game_metrics_shutdown();
        if (result == FT_ERR_SUCCESS)
            result = observability_game_metrics_initialize(count_concurrent_game_sample);
        ++iteration;
    }
    stopped.store(FT_TRUE, std::memory_order_release);
    reader.join();
    const int32_t shutdown = observability_game_metrics_shutdown();
    const uint32_t count = g_concurrent_game_samples.load(std::memory_order_relaxed);
    observability_game_metrics_record(sample);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shutdown);
    FT_ASSERT(count != 0);
    FT_ASSERT_EQ(count, g_concurrent_game_samples.load(std::memory_order_relaxed));
    return (1);
}

FT_TEST(test_observability_game_initialize_rejects_null_exporter)
{
    observability_game_metrics_shutdown();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        observability_game_metrics_initialize(ft_nullptr));
    return (1);
}

FT_TEST(test_observability_game_record_without_initialize_does_not_emit)
{
    ft_game_observability_sample sample;

    observability_game_metrics_shutdown();
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    sample.labels.event_name = "test_event";
    sample.labels.entity = "test_entity";
    sample.labels.attribute = "test_attribute";
    sample.delta_value = 5;
    sample.total_value = 10;
    sample.unit = "unit";
    sample.error_code = FT_ERR_SUCCESS;
    sample.error_tag = ft_nullptr;
    sample.success = true;
    observability_game_metrics_record(sample);
    FT_ASSERT_EQ(0, g_game_export_count);
    return (1);
}

FT_TEST(test_observability_game_record_populates_defaults)
{
    ft_game_observability_sample sample;

    observability_game_metrics_shutdown();
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    FT_ASSERT_EQ(0, observability_game_metrics_initialize(observability_game_test_exporter));
    sample.labels.event_name = "test_event";
    sample.labels.entity = "test_entity";
    sample.labels.attribute = "test_attribute";
    sample.delta_value = 8;
    sample.total_value = 12;
    sample.unit = "unit";
    sample.error_code = FT_ERR_SUCCESS;
    sample.error_tag = ft_nullptr;
    sample.success = true;
    observability_game_metrics_record(sample);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, g_game_last_sample.error_code);
    FT_ASSERT(g_game_last_sample.success);
    FT_ASSERT(g_game_last_sample.error_tag != ft_nullptr);
    FT_ASSERT(observability_game_strings_equal("ok", g_game_last_sample.error_tag));
    FT_ASSERT_EQ(0, observability_game_metrics_shutdown());
    return (1);
}

FT_TEST(test_observability_game_record_propagates_error_details)
{
    ft_game_observability_sample sample;

    observability_game_metrics_shutdown();
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    FT_ASSERT_EQ(0, observability_game_metrics_initialize(observability_game_test_exporter));
    sample.labels.event_name = "test_event";
    sample.labels.entity = "test_entity";
    sample.labels.attribute = "test_attribute";
    sample.delta_value = 4;
    sample.total_value = 9;
    sample.unit = "unit";
    sample.error_code = FT_ERR_CONFIGURATION;
    sample.error_tag = "configuration";
    sample.success = false;
    observability_game_metrics_record(sample);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, g_game_last_sample.error_code);
    FT_ASSERT(observability_game_strings_equal("configuration", g_game_last_sample.error_tag));
    FT_ASSERT(g_game_last_sample.success == false);
    FT_ASSERT_EQ(0, observability_game_metrics_shutdown());
    return (1);
}

FT_TEST(test_game_character_emits_observability_metrics)
{
    game_character character;

    observability_game_metrics_shutdown();
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    FT_ASSERT_EQ(0, observability_game_metrics_initialize(observability_game_test_exporter));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    character.set_hit_points(100);
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    character.add_coins(25);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT(observability_game_strings_equal("character.coins_change", g_game_last_sample.labels.event_name));
    FT_ASSERT(observability_game_strings_equal("game_character", g_game_last_sample.labels.entity));
    FT_ASSERT(observability_game_strings_equal("coins", g_game_last_sample.labels.attribute));
    FT_ASSERT_EQ(25, g_game_last_sample.delta_value);
    FT_ASSERT_EQ(25, g_game_last_sample.total_value);
    FT_ASSERT(observability_game_strings_equal("coins", g_game_last_sample.unit));
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    character.sub_coins(10);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT(observability_game_strings_equal("character.coins_change", g_game_last_sample.labels.event_name));
    FT_ASSERT_EQ(-10, g_game_last_sample.delta_value);
    FT_ASSERT_EQ(15, g_game_last_sample.total_value);
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    character.add_experience(50);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT(observability_game_strings_equal("character.experience_change", g_game_last_sample.labels.event_name));
    FT_ASSERT_EQ(50, g_game_last_sample.delta_value);
    FT_ASSERT_EQ(50, g_game_last_sample.total_value);
    observability_game_reset_last_sample();
    g_game_export_count = 0;
    character.take_damage(30, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT(observability_game_strings_equal("character.damage_taken", g_game_last_sample.labels.event_name));
    FT_ASSERT_EQ(30, g_game_last_sample.delta_value);
    FT_ASSERT_EQ(70, g_game_last_sample.total_value);
    FT_ASSERT_EQ(0, observability_game_metrics_shutdown());
    return (1);
}

FT_TEST(test_game_character_counter_metrics_respect_signed_limits)
{
    game_character character;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_initialize(observability_game_test_exporter));
    g_game_export_count = 0;

    character.set_coins(INT32_MAX);
    character.add_coins(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, character.get_error());
    FT_ASSERT_EQ(INT32_MAX, character.get_coins());
    character.set_valor(INT32_MIN);
    character.sub_valor(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, character.get_error());
    FT_ASSERT_EQ(INT32_MIN, character.get_valor());
    character.add_experience(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    FT_ASSERT_EQ(0, g_game_export_count);

    character.set_experience(INT32_MIN);
    character.sub_experience(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    FT_ASSERT_EQ(0, character.get_experience());
    FT_ASSERT_EQ(1, g_game_export_count);
    FT_ASSERT_EQ(INT64_C(2147483648), g_game_last_sample.delta_value);
    FT_ASSERT_EQ(0, g_game_last_sample.total_value);
    FT_ASSERT(observability_game_strings_equal("character.experience_change",
        g_game_last_sample.labels.event_name));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_shutdown());
    return (1);
}
