#include "../../Modules/Networking/http2_client.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if NETWORKING_HAS_OPENSSL

FT_TEST(test_http2_stream_append_updates_windows_only_after_full_payload)
{
    constexpr char PREFIX[] = {'a', 0, 'b'};
    constexpr char NEXT[256] = {};
    http2_stream_manager manager;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(1U));
    FT_ASSERT_EQ(FT_TRUE, manager.append_data(1U, PREFIX, sizeof(PREFIX)));
    const uint32_t stream_window = manager.get_local_window(1U);
    const uint32_t connection_window = manager.get_connection_local_window();
    FT_ASSERT_EQ(65535U - sizeof(PREFIX), stream_window);
    FT_ASSERT_EQ(65535U - sizeof(PREFIX), connection_window);
    FT_ASSERT_EQ(FT_TRUE, manager.get_stream_buffer(1U, output));
    cma_set_alloc_limit(1U);
    const ft_bool appended = manager.append_data(1U, NEXT, sizeof(NEXT));
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_FALSE, appended);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, manager.get_error());
    FT_ASSERT_EQ(stream_window, manager.get_local_window(1U));
    FT_ASSERT_EQ(connection_window, manager.get_connection_local_window());
    const ft_bool copied = manager.get_stream_buffer(1U, output);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(FT_TRUE, copied);
    FT_ASSERT_EQ(sizeof(PREFIX), output.size());
    FT_ASSERT_EQ(0, ft_memcmp(PREFIX, output.c_str(), sizeof(PREFIX)));
    FT_ASSERT_EQ(FT_TRUE, manager.append_data(1U, NEXT, sizeof(NEXT)));
    FT_ASSERT_EQ(stream_window - sizeof(NEXT), manager.get_local_window(1U));
    FT_ASSERT_EQ(connection_window - sizeof(NEXT), manager.get_connection_local_window());
    FT_ASSERT_EQ(FT_TRUE, manager.get_stream_buffer(1U, output));
    FT_ASSERT_EQ(sizeof(PREFIX) + sizeof(NEXT), output.size());
    FT_ASSERT_EQ(0, ft_memcmp(NEXT, output.c_str() + sizeof(PREFIX), sizeof(NEXT)));
    return (1);
}

FT_TEST(test_http2_stream_window_update_skips_closed_streams_and_preserves_offsets)
{
    http2_stream_manager manager;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(1U));
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(3U));
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(5U));
    FT_ASSERT_EQ(FT_TRUE, manager.append_data(3U, "abc", 3U));
    FT_ASSERT_EQ(FT_TRUE, manager.reserve_send_window(5U, 7U));
    FT_ASSERT_EQ(FT_TRUE, manager.close_stream(1U));
    FT_ASSERT_EQ(FT_FALSE, manager.close_stream(1U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, manager.get_error());
    FT_ASSERT_EQ(FT_TRUE, manager.update_local_initial_window(12U));
    FT_ASSERT_EQ(FT_TRUE, manager.update_remote_initial_window(24U));
    FT_ASSERT_EQ(9U, manager.get_local_window(3U));
    FT_ASSERT_EQ(17U, manager.get_remote_window(5U));
    FT_ASSERT_EQ(65532U, manager.get_connection_local_window());
    FT_ASSERT_EQ(65528U, manager.get_connection_remote_window());
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(1U));
    FT_ASSERT_EQ(12U, manager.get_local_window(1U));
    FT_ASSERT_EQ(24U, manager.get_remote_window(1U));
    FT_ASSERT_EQ(FT_FALSE, manager.update_local_initial_window(0x80000000U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, manager.get_error());
    FT_ASSERT_EQ(9U, manager.get_local_window(3U));
    return (1);
}

FT_TEST(test_http2_stream_removal_keeps_payload_and_reassigns_active_siblings)
{
    http2_stream_manager manager;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(1U));
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(3U));
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(5U));
    FT_ASSERT_EQ(FT_TRUE, manager.append_data(5U, "payload", 7U));
    cma_set_alloc_limit(1U);
    const ft_bool closed = manager.close_stream(1U);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_TRUE, closed);
    FT_ASSERT_EQ(FT_TRUE, manager.get_stream_buffer(5U, output));
    FT_ASSERT_EQ(FT_TRUE, output == "payload");
    FT_ASSERT_EQ(FT_TRUE, manager.update_priority(3U, 0U, 20U, FT_TRUE));
    uint32_t parent = 0;
    uint8_t weight = 0;
    ft_bool exclusive = FT_FALSE;
    FT_ASSERT_EQ(FT_TRUE, manager.get_priority(5U, parent, weight, exclusive));
    FT_ASSERT_EQ(3U, parent);
    FT_ASSERT_EQ(16U, weight);
    FT_ASSERT_EQ(FT_FALSE, exclusive);
    return (1);
}

FT_TEST(test_http2_stream_open_reports_allocation_failure_and_can_retry)
{
    http2_stream_manager manager;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    const ft_size_t capacity = manager._streams.capacity();
    uint32_t stream_identifier = 1;
    while (stream_identifier <= capacity)
    {
        FT_ASSERT_EQ(FT_TRUE, manager.open_stream(stream_identifier));
        ++stream_identifier;
    }
    cma_set_alloc_limit(1U);
    const ft_bool opened = manager.open_stream(stream_identifier);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_FALSE, opened);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, manager.get_error());
    FT_ASSERT_EQ(capacity, manager._streams.size());
    FT_ASSERT_EQ(FT_TRUE, manager.update_local_initial_window(29U));
    FT_ASSERT_EQ(FT_TRUE, manager.open_stream(stream_identifier));
    FT_ASSERT_EQ(29U, manager.get_local_window(stream_identifier));
    return (1);
}

FT_TEST(test_http2_stream_manager_move_needs_no_allocation_and_preserves_state)
{
    http2_stream_manager source;
    http2_stream_manager destination;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_TRUE, source.open_stream(1U));
    FT_ASSERT_EQ(FT_TRUE, source.update_local_initial_window(30U));
    FT_ASSERT_EQ(FT_TRUE, source.append_data(1U, "abc", 3U));
    FT_ASSERT_EQ(FT_FALSE, source.open_stream(1U));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.get_error());
    cma_set_alloc_limit(1U);
    const int32_t move_error = destination.move(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, destination.get_error());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(destination));
    FT_ASSERT_EQ(27U, destination.get_local_window(1U));
    FT_ASSERT_EQ(65532U, destination.get_connection_local_window());
    FT_ASSERT_EQ(FT_TRUE, destination.get_stream_buffer(1U, output));
    FT_ASSERT_EQ(FT_TRUE, output == "abc");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_TRUE, source.open_stream(1U));
    FT_ASSERT_EQ(65535U, source.get_local_window(1U));
    return (1);
}

#endif
