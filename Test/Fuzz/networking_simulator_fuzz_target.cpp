#include "../../Modules/Networking/networking_simulator.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"

#include <cstddef>
#include <cstdint>

class networking_fuzz_sink : public networking_datagram_io
{
    public:
        networking_fuzz_sink() noexcept { return ; }
        ~networking_fuzz_sink() noexcept override { return ; }

        int32_t send_datagram(const networking_message_endpoint &, const uint8_t *,
            ft_size_t) noexcept override
        {
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &, uint8_t *,
            ft_size_t, ft_size_t *received_size) noexcept override
        {
            if (received_size != ft_nullptr)
                *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (1U);
        }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    networking_fuzz_sink sink;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    ft_size_t index;
    int32_t result;

    if (data == ft_nullptr && size != 0U)
        return (0);
    ft_memset(&endpoint, 0, sizeof(endpoint));
    endpoint.address.ss_family = AF_INET;
    endpoint.length = sizeof(struct sockaddr_in);
    configuration.random_seed = 1U;
    configuration.maximum_datagram_size = 1500U;
    if (simulator.initialize(sink, configuration) != FT_ERR_SUCCESS)
        return (0);
    index = 0U;
    while (index + 2U < size && index < 96U)
    {
        networking_simulator_script_action action =
            static_cast<networking_simulator_script_action>(data[index] % 6U);
        uint32_t delay = static_cast<uint32_t>(data[index + 1U]);

        result = simulator.add_script_action(data[index + 2U], action, delay);
        index += 3U;
    }
    if (size > 0U)
    {
        result = simulator.send_datagram(endpoint, data, size);
    }
    simulator.advance(1000U);
    if (simulator.destroy() == FT_ERR_NO_MEMORY || result == FT_ERR_NO_MEMORY)
        return (0);
    return (0);
}
