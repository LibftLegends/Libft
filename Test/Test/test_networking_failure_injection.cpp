#include "../test_internal.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "../../Modules/Networking/networking_nat_traversal.hpp"
#include "../../Modules/Networking/networking_simulator.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_failure_injection_rejects_invalid_schedule)
{
    networking_test_failure_point invalid_point;

    invalid_point = NETWORKING_TEST_FAILURE_POINT_COUNT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        networking_test_failure_fail_next(
            NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        networking_test_failure_fail_next(invalid_point));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        networking_test_failure_fail_after(
            NETWORKING_TEST_CONNECTION_ALLOCATE, UINT64_MAX));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_failure_injection_schedules_relative_failure)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_test_failure_fail_after(
            NETWORKING_TEST_DATAGRAM_SEND, 1U));
    FT_ASSERT(!networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT(networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT(!networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(static_cast<uint64_t>(3U),
        networking_test_failure_attempt_count(
            NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_failure_injection_metadata_and_reset)
{
    networking_test_failure_point point;
    const char *point_name;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    point = NETWORKING_TEST_CONNECTION_ALLOCATE;
    while (point < NETWORKING_TEST_FAILURE_POINT_COUNT)
    {
        point_name = networking_test_failure_point_name(point);
        FT_ASSERT(point_name != ft_nullptr);
        FT_ASSERT(point_name[0] != '\0');
        point = static_cast<networking_test_failure_point>(
            static_cast<uint8_t>(point) + 1U);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT(networking_test_failure_should_fail(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(0U, networking_test_failure_attempt_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(0U, networking_test_failure_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset_all());
    FT_ASSERT_EQ(0U, networking_test_failure_attempt_count(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(0U, networking_test_failure_count(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT(!networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_test_failure_reset(NETWORKING_TEST_FAILURE_POINT_COUNT));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        networking_test_failure_reset_all());
    return (1);
}

FT_TEST(test_networking_failure_injection_scheduling_for_every_point)
{
    networking_test_failure_point point;
    uint64_t expected_attempts;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    point = NETWORKING_TEST_CONNECTION_ALLOCATE;
    while (point < NETWORKING_TEST_FAILURE_POINT_COUNT)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            networking_test_failure_fail_next(point));
        FT_ASSERT(networking_test_failure_should_fail(point));
        FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(point));
        FT_ASSERT_EQ(1U, networking_test_failure_count(point));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset(point));

        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            networking_test_failure_fail_after(point, 2U));
        FT_ASSERT(!networking_test_failure_should_fail(point));
        FT_ASSERT(!networking_test_failure_should_fail(point));
        FT_ASSERT(networking_test_failure_should_fail(point));
        FT_ASSERT(!networking_test_failure_should_fail(point));
        expected_attempts = 4U;
        FT_ASSERT_EQ(expected_attempts,
            networking_test_failure_attempt_count(point));
        FT_ASSERT_EQ(1U, networking_test_failure_count(point));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset(point));
        FT_ASSERT_EQ(0U, networking_test_failure_attempt_count(point));
        FT_ASSERT_EQ(0U, networking_test_failure_count(point));
        point = static_cast<networking_test_failure_point>(
            static_cast<uint8_t>(point) + 1U);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_connection_allocation_fail_after_preserves_first)
{
    networking_test_memory_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection first_connection;
    networking_message_connection rejected_connection;

    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_CONNECTION_ALLOCATE, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        transport.open_connection(endpoint, first_connection));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY,
        transport.open_connection(endpoint, rejected_connection));
    FT_ASSERT_EQ(2U, networking_test_failure_attempt_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_and_nat_queues_fail_after_ordinal)
{
    networking_test_memory_datagram_io io;
    networking_simulated_datagram_io simulator;
    networking_simulator_config simulator_configuration;
    networking_nat_traversal traversal;
    networking_nat_candidate candidate;
    networking_nat_candidate second_candidate;
    networking_message_endpoint endpoint;

    networking_test_prepare_endpoint(endpoint);
    candidate.endpoint = endpoint;
    second_candidate.endpoint = endpoint;
    reinterpret_cast<sockaddr_in *>(
        &second_candidate.endpoint.address)->sin_port = htons(4501U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(io,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_SIMULATOR_QUEUE, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(1U,
        networking_simulator_script_action::SCRIPT_DROP, 0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, simulator.add_script_action(2U,
        networking_simulator_script_action::SCRIPT_DUPLICATE, 0U));
    FT_ASSERT_EQ(2U, networking_test_failure_attempt_count(
        NETWORKING_TEST_SIMULATOR_QUEUE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_SIMULATOR_QUEUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_NAT_CANDIDATE, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(candidate));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY,
        traversal.add_local_candidate(second_candidate));
    FT_ASSERT_EQ(2U, networking_test_failure_attempt_count(
        NETWORKING_TEST_NAT_CANDIDATE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_NAT_CANDIDATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_test_failure_reset(NETWORKING_TEST_NAT_CANDIDATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.add_local_candidate(second_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    return (1);
}
