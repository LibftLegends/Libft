#ifndef NETWORKING_SIMULATOR_HPP
#define NETWORKING_SIMULATOR_HPP

#include "message_transport.hpp"

struct networking_simulator_config
{
    uint32_t fixed_latency_milliseconds;
    uint32_t jitter_milliseconds;
    uint32_t loss_parts_per_million;
    uint32_t duplicate_parts_per_million;
    uint32_t corruption_parts_per_million;
    uint32_t reorder_parts_per_million;
    uint32_t maximum_datagram_size;
    uint32_t maximum_pending_datagrams;
    uint64_t bandwidth_bytes_per_second;
    uint32_t bandwidth_burst_bytes;
    ft_bool disconnect;
    ft_bool blackhole_outgoing;
    ft_bool blackhole_incoming;
    uint64_t random_seed;

    networking_simulator_config() noexcept;
    ~networking_simulator_config() noexcept;
};

enum class networking_simulator_script_action : uint8_t
{
    SCRIPT_DROP = 0U,
    SCRIPT_DUPLICATE = 1U,
    SCRIPT_CORRUPT = 2U,
    SCRIPT_DELAY = 3U,
    SCRIPT_DISCONNECT = 4U,
    SCRIPT_MTU_DROP = 5U
};

struct networking_simulator_script_entry
{
    uint64_t packet_ordinal;
    networking_simulator_script_action action;
    uint32_t delay_milliseconds;

    networking_simulator_script_entry() noexcept;
    ~networking_simulator_script_entry() noexcept;
};

class networking_simulated_datagram_io : public networking_datagram_io
{
    private:
        struct pending_datagram
        {
            networking_message_endpoint destination;
            ft_vector<uint8_t> payload;
            uint64_t delivery_time;

            pending_datagram() noexcept : destination(), payload(), delivery_time(0U)
            {
                return ;
            }

            int32_t initialize() noexcept
            {
                return (this->payload.initialize());
            }

            ~pending_datagram() noexcept
            {
                (void)this->payload.destroy();
                return ;
            }
        };

        uint8_t _initialised_state;
        networking_datagram_io *_underlying;
        networking_simulator_config _configuration;
        ft_deque<pending_datagram *> _pending;
        ft_vector<networking_simulator_script_entry> _script;
        uint64_t _now;
        uint64_t _random_state;
        uint64_t _send_ordinal;
        uint64_t _bandwidth_window_start;
        uint64_t _bandwidth_window_bytes;

        uint32_t next_random() noexcept;
        ft_bool should_drop() noexcept;
        uint64_t calculate_delivery_time() noexcept;
        int32_t flush_ready_datagrams() noexcept;

    public:
        networking_simulated_datagram_io() noexcept;
        networking_simulated_datagram_io(const networking_simulated_datagram_io &other) noexcept = delete;
        networking_simulated_datagram_io(networking_simulated_datagram_io &&other) noexcept = delete;
        ~networking_simulated_datagram_io() noexcept;

        networking_simulated_datagram_io &operator=(const networking_simulated_datagram_io &other) noexcept = delete;
        networking_simulated_datagram_io &operator=(networking_simulated_datagram_io &&other) noexcept = delete;

        int32_t initialize(networking_datagram_io &underlying,
            const networking_simulator_config &configuration) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_simulated_datagram_io &other) noexcept;
        int32_t clear_script() noexcept;
        int32_t add_script_action(uint64_t packet_ordinal,
            networking_simulator_script_action action,
            uint32_t delay_milliseconds) noexcept;
        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override;
        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept override;
        uint64_t now_milliseconds() const noexcept override;
        void advance(uint64_t milliseconds) noexcept;
};

#endif
