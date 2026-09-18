#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <atomic>

namespace
{
    static std::atomic<ft_bool> g_active(FT_FALSE);
    static std::atomic<uint64_t> g_attempts[
        NETWORKING_TEST_FAILURE_POINT_COUNT] = {};
    static std::atomic<uint64_t> g_failure_calls[
        NETWORKING_TEST_FAILURE_POINT_COUNT] = {};
    static std::atomic<uint64_t> g_failures[
        NETWORKING_TEST_FAILURE_POINT_COUNT] = {};
    static const char *const g_failure_point_names[
        NETWORKING_TEST_FAILURE_POINT_COUNT] = {
        "connection_allocate",
        "outgoing_frame_allocate",
        "sent_packet_allocate",
        "reassembly_allocate",
        "received_message_allocate",
        "event_enqueue",
        "datagram_send",
        "mutex_allocate",
        "command_enqueue",
        "ack_range_growth",
        "handshake_state",
        "simulator_queue",
        "nat_candidate",
        "nat_probe",
        "relay_record",
        "callback_copy",
        "worker_create",
        "worker_wakeup",
        "http_server_send",
        "secure_derive_send",
        "secure_derive_receive",
        "secure_init_send",
        "secure_init_receive",
        "secure_init_previous",
        "secure_backend_swap",
        "secure_swap_send",
        "secure_swap_receive",
        "secure_swap_previous",
        "secure_move_init_send",
        "secure_move_init_receive",
        "secure_move_init_previous",
        "secure_move_swap_send",
        "secure_move_swap_receive",
        "secure_move_swap_previous"};

    static ft_bool networking_test_failure_valid_point(
        networking_test_failure_point point) noexcept
    {
        if (static_cast<uint8_t>(point)
            >= NETWORKING_TEST_FAILURE_POINT_COUNT)
            return (FT_FALSE);
        return (FT_TRUE);
    }
}

int32_t networking_test_failure_initialize() noexcept
{
    uint32_t index;

    g_active.store(FT_FALSE, std::memory_order_release);
    index = 0U;
    while (index < NETWORKING_TEST_FAILURE_POINT_COUNT)
    {
        g_attempts[index].store(0U, std::memory_order_release);
        g_failure_calls[index].store(0U, std::memory_order_release);
        g_failures[index].store(0U, std::memory_order_release);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_test_failure_begin() noexcept
{
    int32_t error_code;

    if (g_active.load(std::memory_order_acquire) != FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = networking_test_failure_initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    g_active.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t networking_test_failure_end() noexcept
{
    g_active.store(FT_FALSE, std::memory_order_release);
    return (networking_test_failure_initialize());
}

int32_t networking_test_failure_fail_next(
    networking_test_failure_point point) noexcept
{
    if (networking_test_failure_valid_point(point) == FT_FALSE
        || g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    return (networking_test_failure_fail_after(point, 0U));
}

int32_t networking_test_failure_fail_after(
    networking_test_failure_point point, uint64_t successful_calls) noexcept
{
    uint64_t attempt_count;
    uint8_t point_index;

    if (networking_test_failure_valid_point(point) == FT_FALSE
        || g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    if (successful_calls == UINT64_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    point_index = static_cast<uint8_t>(point);
    attempt_count = g_attempts[point_index].load(std::memory_order_acquire);
    if (attempt_count > UINT64_MAX - successful_calls - 1U)
        return (FT_ERR_OUT_OF_RANGE);
    g_failure_calls[point_index].store(
        attempt_count + successful_calls + 1U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t networking_test_failure_reset(
    networking_test_failure_point point) noexcept
{
    uint8_t point_index;

    if (networking_test_failure_valid_point(point) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    point_index = static_cast<uint8_t>(point);
    g_attempts[point_index].store(0U, std::memory_order_release);
    g_failure_calls[point_index].store(0U, std::memory_order_release);
    g_failures[point_index].store(0U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

const char *networking_test_failure_point_name(
    networking_test_failure_point point) noexcept
{
    if (networking_test_failure_valid_point(point) == FT_FALSE)
        return (ft_nullptr);
    return (g_failure_point_names[static_cast<uint8_t>(point)]);
}

uint64_t networking_test_failure_attempt_count(
    networking_test_failure_point point) noexcept
{
    if (networking_test_failure_valid_point(point) == FT_FALSE)
        return (0U);
    return (g_attempts[static_cast<uint8_t>(point)].load(
        std::memory_order_acquire));
}

uint64_t networking_test_failure_count(
    networking_test_failure_point point) noexcept
{
    if (networking_test_failure_valid_point(point) == FT_FALSE)
        return (0U);
    return (g_failures[static_cast<uint8_t>(point)].load(
        std::memory_order_acquire));
}

ft_bool networking_test_failure_should_fail(
    networking_test_failure_point point) noexcept
{
    uint8_t point_index;
    uint64_t call_number;
    uint64_t failure_call;

    if (networking_test_failure_valid_point(point) == FT_FALSE
        || g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_FALSE);
    point_index = static_cast<uint8_t>(point);
    call_number = g_attempts[point_index].fetch_add(
        1U, std::memory_order_acq_rel) + 1U;
    failure_call = g_failure_calls[point_index].load(
        std::memory_order_acquire);
    if (failure_call != 0U && failure_call == call_number)
    {
        g_failures[point_index].fetch_add(1U, std::memory_order_acq_rel);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}
