#include "../test_internal.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/Game/game_price_definition.hpp"
#include "../../Modules/Game/game_rarity_band.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_currency_rate.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <new>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int register_all_records(game_economy_table &table)
{
    game_price_definition definition;
    game_rarity_band band;
    game_vendor_profile vendor;
    game_currency_rate rate;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.initialize(4, 2, 900, 400, 1200));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, band.initialize(7, 1.15));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vendor.initialize(11, 1.30, 0.55, 0.08));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rate.initialize(5, 2.50, 4));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_price_definition(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_rarity_band(band));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_vendor_profile(vendor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_currency_rate(rate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    return (1);
}

static int assert_price_definition(const game_price_definition &definition,
        int item_id, int rarity, int base_value, int minimum_value, int maximum_value)
{
    FT_ASSERT_EQ(item_id, definition.get_item_id());
    FT_ASSERT_EQ(rarity, definition.get_rarity());
    FT_ASSERT_EQ(base_value, definition.get_base_value());
    FT_ASSERT_EQ(minimum_value, definition.get_minimum_value());
    FT_ASSERT_EQ(maximum_value, definition.get_maximum_value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.get_error());
    return (1);
}

static int assert_vendor_profile_values(const game_vendor_profile &profile,
        int vendor_id, double buy_markup, double sell_multiplier, double tax_rate)
{
    FT_ASSERT_EQ(vendor_id, profile.get_vendor_id());
    FT_ASSERT_DOUBLE_EQ(buy_markup, profile.get_buy_markup());
    FT_ASSERT_DOUBLE_EQ(sell_multiplier, profile.get_sell_multiplier());
    FT_ASSERT_DOUBLE_EQ(tax_rate, profile.get_tax_rate());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    return (1);
}

static int assert_currency_rate_values(const game_currency_rate &rate,
        int currency_id, double buy_rate, int precision)
{
    FT_ASSERT_EQ(currency_id, rate.get_currency_id());
    FT_ASSERT_DOUBLE_EQ(buy_rate, rate.get_rate_to_base());
    FT_ASSERT_EQ(precision, rate.get_display_precision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rate.get_error());
    return (1);
}

static volatile sig_atomic_t g_economy_table_signal_caught = 0;
static sigjmp_buf g_economy_table_signal_jump_buffer;
static struct sigaction g_economy_table_old_action_abort;
static struct sigaction g_economy_table_old_action_iot;
static struct sigaction g_economy_table_new_action_abort;
static struct sigaction g_economy_table_new_action_iot;
static void *g_economy_table_uninitialised_storage = ft_nullptr;
static sigset_t g_economy_table_signal_mask;
static int g_economy_table_uninitialised_operation_error = FT_ERR_SUCCESS;

static void economy_table_signal_handler(int signal_value)
{
    g_economy_table_signal_caught = signal_value;
    siglongjmp(g_economy_table_signal_jump_buffer, 1);
}

static int economy_table_expect_sigabrt_uninitialised(void (*operation)(game_economy_table &))
{
    int32_t saved_stderr;
    int32_t pipe_read_descriptor;
    int32_t pipe_write_descriptor;
    char output_buffer[8192];

    std::memset(&g_economy_table_old_action_abort, 0,
        sizeof(g_economy_table_old_action_abort));
    std::memset(&g_economy_table_old_action_iot, 0,
        sizeof(g_economy_table_old_action_iot));
    std::memset(&g_economy_table_new_action_abort, 0,
        sizeof(g_economy_table_new_action_abort));
    std::memset(&g_economy_table_new_action_iot, 0,
        sizeof(g_economy_table_new_action_iot));
    g_economy_table_new_action_abort.sa_handler = &economy_table_signal_handler;
    g_economy_table_new_action_iot.sa_handler = &economy_table_signal_handler;
    sigemptyset(&g_economy_table_new_action_abort.sa_mask);
    sigemptyset(&g_economy_table_new_action_iot.sa_mask);

    sigemptyset(&g_economy_table_signal_mask);
    sigaddset(&g_economy_table_signal_mask, SIGABRT);
    sigaddset(&g_economy_table_signal_mask, SIGIOT);
    if (pthread_sigmask(SIG_UNBLOCK, &g_economy_table_signal_mask, ft_nullptr) != 0)
    {
    }

    if (sigaction(SIGABRT, &g_economy_table_new_action_abort,
            &g_economy_table_old_action_abort) != 0)
    {
        return (0);
    }
    if (SIGIOT != SIGABRT
        && sigaction(SIGIOT, &g_economy_table_new_action_iot,
            &g_economy_table_old_action_iot) != 0)
    {
        if (sigaction(SIGABRT, &g_economy_table_old_action_abort, ft_nullptr) != 0)
            return (0);
        return (0);
    }
    if (test_capture_abort_output_begin(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor) == 0)
    {
        if (sigaction(SIGABRT, &g_economy_table_old_action_abort, ft_nullptr) != 0)
            return (0);
        if (SIGIOT != SIGABRT)
        {
            if (sigaction(SIGIOT, &g_economy_table_old_action_iot, ft_nullptr) != 0)
                return (0);
        }
        return (0);
    }

    g_economy_table_uninitialised_storage = std::malloc(sizeof(game_economy_table));
    if (g_economy_table_uninitialised_storage == ft_nullptr)
    {
        if (test_capture_abort_output_end(saved_stderr, pipe_read_descriptor,
                pipe_write_descriptor, output_buffer, sizeof(output_buffer)) == 0)
            return (0);
        if (sigaction(SIGABRT, &g_economy_table_old_action_abort, ft_nullptr) != 0)
            return (0);
        if (SIGIOT != SIGABRT)
        {
            if (sigaction(SIGIOT, &g_economy_table_old_action_iot, ft_nullptr) != 0)
                return (0);
        }
        return (0);
    }
    g_economy_table_signal_caught = 0;
    g_economy_table_uninitialised_operation_error = FT_ERR_SUCCESS;
    if (sigsetjmp(g_economy_table_signal_jump_buffer, 1) == 0)
    {
        game_economy_table *table_pointer;

        table_pointer = new (g_economy_table_uninitialised_storage) game_economy_table();
        table_pointer->initialize();
        operation(*table_pointer);
        table_pointer->~game_economy_table();
    }
    if (g_economy_table_uninitialised_storage != ft_nullptr)
    {
        std::free(g_economy_table_uninitialised_storage);
        g_economy_table_uninitialised_storage = ft_nullptr;
    }
    if (test_capture_abort_output_end(saved_stderr, pipe_read_descriptor,
            pipe_write_descriptor, output_buffer, sizeof(output_buffer)) == 0)
        return (0);
    if (sigaction(SIGABRT, &g_economy_table_old_action_abort, ft_nullptr) != 0)
        return (0);
    if (SIGIOT != SIGABRT)
    {
        if (sigaction(SIGIOT, &g_economy_table_old_action_iot, ft_nullptr) != 0)
            return (0);
    }

    if (g_economy_table_signal_caught == SIGABRT)
        return (1);
    if (g_economy_table_signal_caught == SIGIOT)
        return (1);
    return (g_economy_table_uninitialised_operation_error == FT_ERR_SUCCESS);
}

static void economy_table_call_copy_from_uninitialised(game_economy_table &source)
{
    void *storage;
    game_economy_table *destination;
    int result;

    storage = std::malloc(sizeof(game_economy_table));
    if (storage == ft_nullptr)
    {
        g_economy_table_uninitialised_operation_error = FT_ERR_NO_MEMORY;
        return ;
    }
    destination = new (storage) game_economy_table();
    result = destination->initialize(source);
    g_economy_table_uninitialised_operation_error = result;
    destination->~game_economy_table();
    std::free(storage);
    return ;
}

static void economy_table_call_move_from_uninitialised(game_economy_table &source)
{
    void *storage;
    game_economy_table *destination;
    int result;

    storage = std::malloc(sizeof(game_economy_table));
    if (storage == ft_nullptr)
    {
        g_economy_table_uninitialised_operation_error = FT_ERR_NO_MEMORY;
        return ;
    }
    destination = new (storage) game_economy_table();
    result = destination->initialize(ft_move(source));
    g_economy_table_uninitialised_operation_error = result;
    destination->~game_economy_table();
    std::free(storage);
    return ;
}

FT_TEST(test_economy_table_initialize_copy)
{
    game_economy_table source;
    game_economy_table duplicate;
    game_price_definition price;
    game_price_definition source_price;
    game_rarity_band rarity;
    game_rarity_band source_rarity;
    game_vendor_profile vendor;
    game_vendor_profile source_vendor;
    game_currency_rate currency;
    game_currency_rate source_currency;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    register_all_records(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.initialize(source));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, price.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rarity.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, currency.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_price_definition(4, price));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_rarity_band(7, rarity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_vendor_profile(11, vendor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_currency_rate(5, currency));
    assert_price_definition(price, 4, 2, 900, 400, 1200);
    assert_vendor_profile_values(vendor, 11, 1.30, 0.55, 0.08);
    assert_currency_rate_values(currency, 5, 2.50, 4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_price.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_rarity.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_currency.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.fetch_price_definition(4, source_price));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.fetch_rarity_band(7, source_rarity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.fetch_vendor_profile(11, source_vendor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.fetch_currency_rate(5, source_currency));
    assert_price_definition(source_price, 4, 2, 900, 400, 1200);
    assert_vendor_profile_values(source_vendor, 11, 1.30, 0.55, 0.08);
    assert_currency_rate_values(source_currency, 5, 2.50, 4);
    return (1);
}

FT_TEST(test_economy_table_initialize_copy_uninitialised_destination)
{
    game_economy_table source;
    game_economy_table duplicate;
    game_price_definition price;
    game_rarity_band rarity;
    game_vendor_profile vendor;
    game_currency_rate currency;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    register_all_records(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.initialize(source));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, price.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rarity.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, currency.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_price_definition(4, price));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_rarity_band(7, rarity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_vendor_profile(11, vendor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.fetch_currency_rate(5, currency));
    return (1);
}

FT_TEST(test_economy_table_initialize_overwrites_existing_entries)
{
    game_economy_table source;
    game_economy_table destination;
    game_price_definition price;
    game_rarity_band rarity;
    game_vendor_profile vendor;
    game_currency_rate currency;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    register_all_records(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    register_all_records(destination);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, price.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rarity.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, currency.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_price_definition(4, price));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_rarity_band(7, rarity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_vendor_profile(11, vendor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_currency_rate(5, currency));
    assert_price_definition(price, 4, 2, 900, 400, 1200);
    assert_vendor_profile_values(vendor, 11, 1.30, 0.55, 0.08);
    assert_currency_rate_values(currency, 5, 2.50, 4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    return (1);
}

FT_TEST(test_economy_table_initialize_copy_cleans_up_after_catalog_failure)
{
    game_economy_table source;
    game_economy_table destination;
    game_vendor_profile vendor;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(1, register_all_records(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_vendor_profiles().enable_thread_safety());

    int32_t vendor_id = 100;
    while (vendor_id < 228)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, vendor.initialize(vendor_id, 1.30, 0.55, 0.08));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_vendor_profile(vendor));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, vendor.destroy());
        ++vendor_id;
    }

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(2048U));
    const int32_t result = destination.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination.get_error());
    FT_ASSERT_EQ(129U, source.get_vendor_profiles().size());
    FT_ASSERT_EQ(1U, source.get_price_definitions().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(0U, destination.get_price_definitions().size());
    FT_ASSERT_EQ(0U, destination.get_vendor_profiles().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(129U, destination.get_vendor_profiles().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_vendor_profile(227, vendor));
    assert_vendor_profile_values(vendor, 227, 1.30, 0.55, 0.08);
    return (1);
}

FT_TEST(test_economy_table_initialize_move_transfers_entries)
{
    game_economy_table source;
    game_economy_table moved;
    game_price_definition price;
    game_rarity_band rarity;
    game_vendor_profile vendor;
    game_currency_rate currency;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(1, register_all_records(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_price_definitions().enable_thread_safety());
    const auto *price_entry = source.get_price_definitions().find(4);
    const auto *rarity_entry = source.get_rarity_bands().find(7);
    const auto *vendor_entry = source.get_vendor_profiles().find(11);
    const auto *currency_entry = source.get_currency_rates().find(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = moved.initialize(ft_move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(moved.get_price_definitions().find(4) == price_entry);
    FT_ASSERT(moved.get_rarity_bands().find(7) == rarity_entry);
    FT_ASSERT(moved.get_vendor_profiles().find(11) == vendor_entry);
    FT_ASSERT(moved.get_currency_rates().find(5) == currency_entry);
    FT_ASSERT_EQ(FT_FALSE, moved.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, moved.get_price_definitions().is_thread_safe());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, price.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rarity.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, currency.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.fetch_price_definition(4, price));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.fetch_rarity_band(7, rarity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.fetch_vendor_profile(11, vendor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.fetch_currency_rate(5, currency));
    assert_price_definition(price, 4, 2, 900, 400, 1200);
    assert_vendor_profile_values(vendor, 11, 1.30, 0.55, 0.08);
    assert_currency_rate_values(currency, 5, 2.50, 4);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_price_definitions().size());
    return (1);
}

FT_TEST(test_economy_table_move_keeps_source_when_catalog_lock_fails)
{
    game_economy_table source;
    game_economy_table destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(1, register_all_records(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_currency_rates().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(1, register_all_records(destination));

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(1U, source.get_price_definitions().size());
    FT_ASSERT_EQ(1U, source.get_rarity_bands().size());
    FT_ASSERT_EQ(1U, source.get_vendor_profiles().size());
    FT_ASSERT_EQ(1U, source.get_currency_rates().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(1U, destination.get_currency_rates().size());
    return (1);
}

FT_TEST(test_economy_table_initialize_copy_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, economy_table_expect_sigabrt_uninitialised(
        economy_table_call_copy_from_uninitialised));
    return (1);
}

FT_TEST(test_economy_table_initialize_move_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, economy_table_expect_sigabrt_uninitialised(
        economy_table_call_move_from_uninitialised));
    return (1);
}
