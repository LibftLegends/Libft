#include "../../Modules/Networking/http2_client.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if NETWORKING_HAS_OPENSSL

FT_TEST(test_http2_settings_applies_payload_without_allocation)
{
    constexpr char PAYLOAD[] = {0, 4, 0, 0, 4, 0, 0, 3, 0, 0, 0, 12};
    http2_frame frame;
    http2_stream_manager streams;
    http2_settings_state settings;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, streams.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, settings.initialize());
    FT_ASSERT_EQ(FT_TRUE, streams.open_stream(1U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_type(4U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_payload_from_buffer(PAYLOAD, sizeof(PAYLOAD)));
    cma_set_alloc_limit(1U);
    const ft_bool applied = settings.apply_remote_settings(frame, streams);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_TRUE, applied);
    FT_ASSERT_EQ(1024U, settings.get_initial_remote_window());
    FT_ASSERT_EQ(1024U, streams.get_remote_window(1U));
    FT_ASSERT_EQ(12U, settings.get_max_concurrent_streams());
    return (1);
}

FT_TEST(test_http2_settings_rejects_invalid_frame_before_updating_windows)
{
    constexpr char INVALID[] = {0, 4, 0, 0, 4, 0, 0, 2, 0, 0, 0, 2};
    constexpr char VALID[] = {0, 4, 0, 0, 4, 0};
    http2_frame frame;
    http2_stream_manager streams;
    http2_settings_state settings;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, streams.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, settings.initialize());
    FT_ASSERT_EQ(FT_TRUE, streams.open_stream(1U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_type(4U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_payload_from_buffer(INVALID, sizeof(INVALID)));
    FT_ASSERT_EQ(FT_FALSE, settings.apply_remote_settings(frame, streams));
    FT_ASSERT_EQ(65535U, settings.get_initial_remote_window());
    FT_ASSERT_EQ(65535U, streams.get_remote_window(1U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_payload_from_buffer(VALID, sizeof(VALID) - 1U));
    FT_ASSERT_EQ(FT_FALSE, settings.apply_remote_settings(frame, streams));
    FT_ASSERT_EQ(FT_TRUE, frame.set_payload_from_buffer(VALID, sizeof(VALID)));
    FT_ASSERT_EQ(FT_TRUE, frame.set_stream_identifier(1U));
    FT_ASSERT_EQ(FT_FALSE, settings.apply_remote_settings(frame, streams));
    FT_ASSERT_EQ(FT_TRUE, frame.set_stream_identifier(0U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_flags(1U));
    FT_ASSERT_EQ(FT_FALSE, settings.apply_remote_settings(frame, streams));
    frame.clear_payload();
    FT_ASSERT_EQ(FT_TRUE, settings.apply_remote_settings(frame, streams));
    FT_ASSERT_EQ(65535U, streams.get_remote_window(1U));
    return (1);
}

FT_TEST(test_http2_alpn_accepts_well_formed_protocol_offer)
{
    SSL_CTX *context = SSL_CTX_new(TLS_client_method());
    FT_ASSERT(context != ft_nullptr);
    SSL *session = SSL_new(context);
    SSL_CTX_free(context);
    FT_ASSERT(session != ft_nullptr);
    ft_bool selected = FT_TRUE;
    int32_t error_code = FT_ERR_INVALID_OPERATION;
    const ft_bool configured = http2_select_alpn_protocol(session, selected, error_code);
    SSL_free(session);
    FT_ASSERT_EQ(FT_TRUE, configured);
    FT_ASSERT_EQ(FT_FALSE, selected);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    return (1);
}

#endif
