#include "../../Modules/API/api_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if NETWORKING_HAS_OPENSSL

FT_TEST(test_api_pool_tls_registry_tracks_one_owner_after_duplicate_registration)
{
    SSL_CTX *context = SSL_CTX_new(TLS_client_method());
    FT_ASSERT(context != ft_nullptr);
    SSL *session = SSL_new(context);
    SSL_CTX_free(context);
    FT_ASSERT(session != ft_nullptr);
    const ft_bool first = api_connection_pool_track_tls_session(session);
    const ft_bool duplicate = api_connection_pool_track_tls_session(session);
    const ft_bool first_removal = api_connection_pool_untrack_tls_session(session);
    const ft_bool second_removal = api_connection_pool_untrack_tls_session(session);
    SSL_free(session);
    FT_ASSERT_EQ(FT_TRUE, first);
    FT_ASSERT_EQ(FT_TRUE, duplicate);
    FT_ASSERT_EQ(FT_TRUE, first_removal);
    FT_ASSERT_EQ(FT_FALSE, second_removal);
    return (1);
}

FT_TEST(test_api_pool_tls_handle_move_transfers_cleanup)
{
    api_connection_pool_handle source;
    api_connection_pool_handle destination;
    api_connection_pool_handle copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    SSL_CTX *context = SSL_CTX_new(TLS_client_method());
    FT_ASSERT(context != ft_nullptr);
    SSL *session = SSL_new(context);
    if (!session)
        SSL_CTX_free(context);
    FT_ASSERT(session != ft_nullptr);
    FT_ASSERT_EQ(1, SSL_up_ref(session));
    FT_ASSERT_EQ(FT_TRUE, api_connection_pool_track_tls_session(session));
    source.security_mode = api_connection_security_mode::TLS;
    source.tls_session = session;
    source.tls_context = context;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(ft_nullptr, copy.tls_session);
    FT_ASSERT_EQ(ft_nullptr, copy.tls_context);
    FT_ASSERT_EQ(0U, destination.move(source));
    FT_ASSERT_EQ(ft_nullptr, source.tls_session);
    FT_ASSERT_EQ(session, destination.tls_session);
    FT_ASSERT_EQ(context, destination.tls_context);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    const ft_bool still_registered = api_connection_pool_untrack_tls_session(session);
    SSL_free(session);
    FT_ASSERT_EQ(FT_FALSE, still_registered);
    return (1);
}

#endif
