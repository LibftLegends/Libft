#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <atomic>
#include <thread>

FT_TEST(test_networking_message_transport_serializes_concurrent_send_and_stats)
{
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    networking_message_statistics final_statistics;
    std::atomic<int32_t> send_error(FT_ERR_SUCCESS);
    std::atomic<int32_t> second_send_error(FT_ERR_SUCCESS);
    std::atomic<int32_t> statistics_error(FT_ERR_SUCCESS);
    std::atomic<int32_t> receive_error(FT_ERR_SUCCESS);
    std::atomic<int32_t> lane_error(FT_ERR_SUCCESS);
    std::atomic<uint32_t> completed_sends(0U);
    std::atomic<ft_bool> begin_operations(FT_FALSE);
    uint64_t connection_id;
    uint8_t payload[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    std::thread sender_thread;
    std::thread second_sender_thread;
    std::thread statistics_thread;
    std::thread receive_thread;
    std::thread lane_thread;

    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    connection_id = connection.get_id();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    FT_ASSERT(transport.is_worker_running());
    /* Keep the unpaired transport bounded while stressing concurrent reads. */
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    sender_thread = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 32U)
        {
            int32_t result;

            result = connection.send_message(payload, sizeof(payload), options);
            if (result != FT_ERR_SUCCESS)
            {
                send_error.store(result, std::memory_order_release);
                return ;
            }
            completed_sends.fetch_add(1U, std::memory_order_release);
            index += 1U;
        }
        return ;
    });
    second_sender_thread = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 32U)
        {
            int32_t result;

            result = connection.send_message(payload, sizeof(payload), options);
            if (result != FT_ERR_SUCCESS)
            {
                second_send_error.store(result, std::memory_order_release);
                return ;
            }
            completed_sends.fetch_add(1U, std::memory_order_release);
            index += 1U;
        }
        return ;
    });
    statistics_thread = std::thread([&]() noexcept
    {
        uint32_t index = 0U;
        uint64_t previous_messages_sent = 0U;
        uint64_t previous_lane_bytes_sent = 0U;
        uint64_t previous_packets_sent = 0U;
        uint64_t previous_bytes_sent = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 1000U)
        {
            networking_message_statistics statistics;
            int32_t result;

            result = transport.get_statistics(connection_id, statistics);
            if (result != FT_ERR_SUCCESS)
            {
                statistics_error.store(result, std::memory_order_release);
                return ;
            }
            if (statistics.messages_sent < previous_messages_sent
                || statistics.lane_bytes_sent[2U]
                    < previous_lane_bytes_sent
                || statistics.packets_sent < previous_packets_sent
                || statistics.bytes_sent < previous_bytes_sent)
            {
                statistics_error.store(FT_ERR_INVALID_STATE,
                    std::memory_order_release);
                return ;
            }
            previous_messages_sent = statistics.messages_sent;
            previous_lane_bytes_sent = statistics.lane_bytes_sent[2U];
            previous_packets_sent = statistics.packets_sent;
            previous_bytes_sent = statistics.bytes_sent;
            index += 1U;
        }
        return ;
    });
    receive_thread = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 1000U)
        {
            networking_received_message received;
            int32_t result;

            result = transport.receive_message(received);
            if (result != FT_ERR_EMPTY)
            {
                receive_error.store(result, std::memory_order_release);
                return ;
            }
            index += 1U;
        }
        return ;
    });
    lane_thread = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 64U)
        {
            int32_t result;

            result = connection.configure_lane(2U, (index % 8U) + 1U,
                index);
            if (result != FT_ERR_SUCCESS)
            {
                lane_error.store(result, std::memory_order_release);
                return ;
            }
            index += 1U;
        }
        return ;
    });
    begin_operations.store(FT_TRUE, std::memory_order_release);
    sender_thread.join();
    second_sender_thread.join();
    statistics_thread.join();
    receive_thread.join();
    lane_thread.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        send_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_send_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(64U, completed_sends.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        statistics_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        receive_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lane_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(connection_id,
        final_statistics));
    FT_ASSERT_EQ(64U, final_statistics.messages_sent);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT(!transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_repeated_worker_lifecycle)
{
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    uint32_t cycle = 0U;

    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    while (cycle < 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
        FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
        FT_ASSERT_EQ(FT_TRUE, transport.is_worker_running());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
        FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
        cycle += 1U;
    }
    return (1);
}

FT_TEST(test_networking_message_transport_serializes_concurrent_worker_stop)
{
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    std::atomic<ft_bool> begin_stop(FT_FALSE);
    std::atomic<int32_t> first_result(FT_ERR_INTERNAL);
    std::atomic<int32_t> second_result(FT_ERR_INTERNAL);
    std::thread first_stopper;
    std::thread second_stopper;

    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    first_stopper = std::thread([&]() noexcept
    {
        while (begin_stop.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        first_result.store(transport.stop_worker(), std::memory_order_release);
        return ;
    });
    second_stopper = std::thread([&]() noexcept
    {
        while (begin_stop.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        second_result.store(transport.stop_worker(), std::memory_order_release);
        return ;
    });
    begin_stop.store(FT_TRUE, std::memory_order_release);
    first_stopper.join();
    second_stopper.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_result.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_result.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_serializes_concurrent_worker_start)
{
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    std::atomic<ft_bool> begin_start(FT_FALSE);
    std::atomic<int32_t> first_result(FT_ERR_INTERNAL);
    std::atomic<int32_t> second_result(FT_ERR_INTERNAL);
    std::thread first_starter;
    std::thread second_starter;

    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    first_starter = std::thread([&]() noexcept
    {
        while (begin_start.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        first_result.store(transport.start_worker(), std::memory_order_release);
        return ;
    });
    second_starter = std::thread([&]() noexcept
    {
        while (begin_start.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        second_result.store(transport.start_worker(), std::memory_order_release);
        return ;
    });
    begin_start.store(FT_TRUE, std::memory_order_release);
    first_starter.join();
    second_starter.join();
    FT_ASSERT((first_result.load(std::memory_order_acquire) == FT_ERR_SUCCESS
            && second_result.load(std::memory_order_acquire)
                == FT_ERR_ALREADY_INITIALISED)
        || (second_result.load(std::memory_order_acquire) == FT_ERR_SUCCESS
            && first_result.load(std::memory_order_acquire)
                == FT_ERR_ALREADY_INITIALISED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_serializes_concurrent_send_close)
{
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    std::atomic<ft_bool> begin_operations(FT_FALSE);
    std::atomic<int32_t> send_error(FT_ERR_SUCCESS);
    std::atomic<int32_t> close_error(FT_ERR_INTERNAL);
    std::atomic<int32_t> statistics_error(FT_ERR_SUCCESS);
    std::atomic<int32_t> receive_error(FT_ERR_SUCCESS);
    std::atomic<uint32_t> completed_sends(0U);
    std::thread sender;
    std::thread closer;
    std::thread statistics_reader;
    std::thread receive_reader;
    uint64_t connection_id;
    uint8_t payload[4] = {3U, 1U, 4U, 1U};

    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    connection_id = connection.get_id();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    options.delivery = networking_message_delivery::UNRELIABLE;
    sender = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 64U)
        {
            int32_t result;

            result = connection.send_message(payload, sizeof(payload), options);
            if (result == FT_ERR_INVALID_HANDLE || result == FT_ERR_NO_MEMORY)
                return ;
            if (result != FT_ERR_SUCCESS)
            {
                send_error.store(result, std::memory_order_release);
                return ;
            }
            completed_sends.fetch_add(1U, std::memory_order_release);
            index += 1U;
        }
        return ;
    });
    closer = std::thread([&]() noexcept
    {
        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        close_error.store(connection.close(), std::memory_order_release);
        return ;
    });
    statistics_reader = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 256U)
        {
            networking_message_statistics statistics;
            int32_t result;

            result = transport.get_statistics(connection_id, statistics);
            if (result != FT_ERR_SUCCESS)
            {
                statistics_error.store(result, std::memory_order_release);
                return ;
            }
            index += 1U;
        }
        return ;
    });
    receive_reader = std::thread([&]() noexcept
    {
        uint32_t index = 0U;

        while (begin_operations.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        while (index < 256U)
        {
            networking_received_message received;
            int32_t result;

            result = transport.receive_message(received);
            if (result != FT_ERR_EMPTY)
            {
                receive_error.store(result, std::memory_order_release);
                return ;
            }
            index += 1U;
        }
        return ;
    });
    begin_operations.store(FT_TRUE, std::memory_order_release);
    sender.join();
    closer.join();
    statistics_reader.join();
    receive_reader.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, send_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, close_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        statistics_error.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        receive_error.load(std::memory_order_acquire));
    FT_ASSERT(completed_sends.load(std::memory_order_acquire) <= 64U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_start_racing_destroy_is_safe)
{
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    std::atomic<ft_bool> begin_lifecycle(FT_FALSE);
    std::atomic<int32_t> start_result(FT_ERR_INTERNAL);
    std::atomic<int32_t> destroy_result(FT_ERR_INTERNAL);
    std::thread starter;
    std::thread destroyer;

    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    starter = std::thread([&]() noexcept
    {
        while (begin_lifecycle.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        start_result.store(transport.start_worker(), std::memory_order_release);
        return ;
    });
    destroyer = std::thread([&]() noexcept
    {
        while (begin_lifecycle.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        destroy_result.store(transport.destroy(), std::memory_order_release);
        return ;
    });
    begin_lifecycle.store(FT_TRUE, std::memory_order_release);
    starter.join();
    destroyer.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destroy_result.load(std::memory_order_acquire));
    FT_ASSERT(start_result.load(std::memory_order_acquire) == FT_ERR_SUCCESS
        || start_result.load(std::memory_order_acquire) == FT_ERR_THREAD_BUSY
        || start_result.load(std::memory_order_acquire) == FT_ERR_NOT_INITIALISED);
    FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}
