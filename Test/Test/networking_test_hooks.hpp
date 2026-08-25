#ifndef NETWORKING_TEST_HOOKS_HPP
#define NETWORKING_TEST_HOOKS_HPP

#include "../../Modules/Errno/errno.hpp"
#include <cstdint>

enum networking_test_failure_point : uint8_t
{
    NETWORKING_TEST_CONNECTION_ALLOCATE = 0U,
    NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE = 1U,
    NETWORKING_TEST_SENT_PACKET_ALLOCATE = 2U,
    NETWORKING_TEST_REASSEMBLY_ALLOCATE = 3U,
    NETWORKING_TEST_RECEIVED_MESSAGE_ALLOCATE = 4U,
    NETWORKING_TEST_EVENT_ENQUEUE = 5U,
    NETWORKING_TEST_DATAGRAM_SEND = 6U,
    NETWORKING_TEST_MUTEX_ALLOCATE = 7U,
    NETWORKING_TEST_COMMAND_ENQUEUE = 8U,
    NETWORKING_TEST_ACK_RANGE_GROWTH = 9U,
    NETWORKING_TEST_HANDSHAKE_STATE = 10U,
    NETWORKING_TEST_SIMULATOR_QUEUE = 11U,
    NETWORKING_TEST_NAT_CANDIDATE = 12U,
    NETWORKING_TEST_NAT_PROBE = 13U,
    NETWORKING_TEST_RELAY_RECORD = 14U,
    NETWORKING_TEST_CALLBACK_COPY = 15U,
    NETWORKING_TEST_WORKER_CREATE = 16U,
    NETWORKING_TEST_WORKER_WAKEUP = 17U,
    NETWORKING_TEST_FAILURE_POINT_COUNT = 18U
};

int32_t networking_test_failure_initialize() noexcept;
int32_t networking_test_failure_begin() noexcept;
int32_t networking_test_failure_end() noexcept;
int32_t networking_test_failure_fail_next(
    networking_test_failure_point point) noexcept;
int32_t networking_test_failure_fail_after(
    networking_test_failure_point point, uint64_t successful_calls) noexcept;
uint64_t networking_test_failure_attempt_count(
    networking_test_failure_point point) noexcept;
ft_bool networking_test_failure_should_fail(
    networking_test_failure_point point) noexcept;

#ifdef LIBFT_TEST_BUILD
# define NETWORKING_TEST_SHOULD_FAIL(point) \
    networking_test_failure_should_fail(point)
#else
# define NETWORKING_TEST_SHOULD_FAIL(point) FT_FALSE
#endif

#endif
