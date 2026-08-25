#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <cstdint>

class networking_fuzz_datagram_io : public networking_datagram_io
{
    public:
        networking_fuzz_datagram_io() noexcept
        {
            return ;
        }

        ~networking_fuzz_datagram_io() noexcept override
        {
            return ;
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)destination;
            (void)data;
            (void)size;
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity,
            ft_size_t *received_size) noexcept override
        {
            (void)source;
            (void)data;
            (void)capacity;
            if (received_size != ft_nullptr)
                *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (1U);
        }
};

static void networking_fuzz_run_transport(const uint8_t *data,
    ft_size_t size, ft_bool enable_encryption) noexcept
{
    networking_fuzz_datagram_io datagram_io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;

    configuration.enable_encryption = enable_encryption;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    ft_memset(&source, 0, sizeof(source));
    source.address.ss_family = AF_INET;
    source.length = sizeof(struct sockaddr_in);
    if (transport.initialize(configuration, datagram_io) != FT_ERR_SUCCESS)
        return ;
    (void)transport.process_datagram(source, data, size);
    (void)transport.poll();
    (void)transport.destroy();
    return ;
}

/* The libFuzzer ABI requires this exact exported symbol and 32-bit result. */
extern "C" int32_t LLVMFuzzerTestOneInput(const uint8_t *data,
    ft_size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = size;
    if (bounded_size > 65536U)
        bounded_size = 65536U;
    networking_fuzz_run_transport(data, bounded_size, FT_FALSE);
    networking_fuzz_run_transport(data, bounded_size, FT_TRUE);
    return (0);
}
