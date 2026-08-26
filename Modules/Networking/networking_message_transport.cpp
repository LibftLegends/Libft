#include "message_transport.hpp"
#include "networking_handshake.hpp"
#include "networking_crypto_backend.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Observability/observability_networking_metrics.hpp"
#ifdef LIBFT_TEST_BUILD
# include "../../Test/Test/networking_test_hooks.hpp"
#else
# define NETWORKING_TEST_SHOULD_FAIL(point) FT_FALSE
#endif
#include <algorithm>
#include <chrono>
#include <cerrno>

enum class networking_message_command_type : uint8_t
{
    SEND_MESSAGE = 0U,
    CLOSE_CONNECTION = 1U,
    UPDATE_KEY_EPOCH = 2U,
    REQUEST_KEY_UPDATE = 3U,
    CONFIGURE_LANE = 4U,
    SET_QUEUE_LIMITS = 5U,
    FLUSH_CONNECTION = 6U,
    LISTEN = 7U,
    ACCEPT = 8U,
    REJECT = 9U,
    OPEN_CONNECTION = 10U,
    SET_EVENT_CALLBACK = 11U
};

struct networking_message_command
{
    networking_message_command_type type;
    uint64_t connection_id;
    networking_message_connection *connection;
    networking_message_endpoint endpoint;
    networking_message_send_options send_options;
    networking_message_close_reason close_reason;
    ft_bool abort_connection;
    char debug_text[96];
    uint64_t key_epoch;
    uint8_t lane;
    uint32_t priority_weight;
    uint32_t reserved_bandwidth_bytes_per_second;
    uint32_t maximum_queued_bytes;
    networking_message_event_callback callback;
    void *callback_user_data;
    ft_vector<uint8_t> payload;
    int32_t result;
    std::atomic<uint32_t> completion_epoch;
    std::atomic<ft_bool> completed;

    networking_message_command() noexcept
        : type(networking_message_command_type::SEND_MESSAGE),
          connection_id(0U), connection(ft_nullptr), endpoint(), send_options(),
          close_reason(networking_message_close_reason::NONE),
          abort_connection(FT_FALSE), debug_text(),
          key_epoch(0U), lane(0U), priority_weight(0U),
          reserved_bandwidth_bytes_per_second(0U), maximum_queued_bytes(0U),
          callback(ft_nullptr), callback_user_data(ft_nullptr), payload(),
          result(FT_ERR_SUCCESS), completion_epoch(0U), completed(FT_FALSE)
    {
        ft_memset(&this->endpoint, 0, sizeof(this->endpoint));
        ft_memset(this->debug_text, 0, sizeof(this->debug_text));
        return ;
    }

    int32_t initialize() noexcept
    {
        return (this->payload.initialize());
    }

    void destroy() noexcept
    {
        (void)this->payload.destroy();
        return ;
    }
};

namespace
{
    static const uint8_t NETWORKING_MESSAGE_MAGIC = 0x4cU;
    static const uint8_t NETWORKING_MESSAGE_VERSION = 1U;
    static const uint8_t NETWORKING_MESSAGE_FRAME = 1U;
    static const uint8_t NETWORKING_MESSAGE_ACK = 2U;
    static const uint8_t NETWORKING_MESSAGE_CLOSE = 3U;
    static const uint8_t NETWORKING_MESSAGE_HANDSHAKE_HELLO = 4U;
    static const uint8_t NETWORKING_MESSAGE_HANDSHAKE_FINISHED = 5U;
    static const uint8_t NETWORKING_MESSAGE_HANDSHAKE_RETRY = 6U;
    static const uint8_t NETWORKING_MESSAGE_PATH_CHALLENGE = 7U;
    static const uint8_t NETWORKING_MESSAGE_PATH_RESPONSE = 8U;
    static const uint8_t NETWORKING_MESSAGE_FLOW_CONTROL = 9U;
    static const uint8_t NETWORKING_MESSAGE_KEY_UPDATE = 10U;
    static const uint8_t NETWORKING_MESSAGE_KEY_UPDATE_ACK = 11U;
    static const uint32_t NETWORKING_MESSAGE_HEADER_SIZE = 64U;
    static const uint32_t NETWORKING_MESSAGE_KEY_EPOCH_SIZE = 8U;
    static const uint32_t NETWORKING_MESSAGE_PATH_TOKEN_SIZE = 8U;
    static const uint32_t NETWORKING_MESSAGE_ACK_RANGE_LIMIT = 4U;
    static const uint32_t NETWORKING_MESSAGE_ACK_RANGE_BYTES = 1U
        + NETWORKING_MESSAGE_ACK_RANGE_LIMIT * 4U;
    static const uint32_t NETWORKING_MESSAGE_FRAME_HEADER_SIZE =
        NETWORKING_MESSAGE_ACK_RANGE_BYTES - 4U
        + NETWORKING_MESSAGE_KEY_EPOCH_SIZE;

    struct networking_ack_range
    {
        uint64_t start;
        uint64_t end;

        networking_ack_range() noexcept : start(0U), end(0U)
        {
            return ;
        }
    };

    static void networking_message_wipe(void *data, ft_size_t size) noexcept
    {
        networking_crypto_backend backend;

        (void)backend.wipe(data, size);
        return ;
    }

    struct outgoing_frame
    {
        networking_message_delivery delivery;
        uint8_t lane;
        uint32_t channel;
        uint64_t message_id;
        uint64_t sequence;
        uint64_t expiry;
        uint32_t total_size;
        uint32_t offset;
        ft_vector<uint8_t> payload;
        uint8_t fragment_count;

        outgoing_frame() noexcept
            : delivery(networking_message_delivery::RELIABLE_ORDERED), lane(0U),
              channel(0U), message_id(0U), sequence(0U), expiry(0U), total_size(0U),
              offset(0U), payload(), fragment_count(1U)
        {
            return ;
        }

        int32_t initialize() noexcept
        {
            return (this->payload.initialize());
        }

        ~outgoing_frame() noexcept
        {
            (void)this->payload.destroy();
            return ;
        }
    };

    struct sent_packet
    {
        uint64_t packet_number;
        uint64_t sent_at;
        uint64_t wire_size;
        ft_bool retransmitted;
        outgoing_frame *frame;

        sent_packet() noexcept : packet_number(0U), sent_at(0U), wire_size(0U),
            retransmitted(FT_FALSE), frame(ft_nullptr)
        {
            return ;
        }
    };

    struct reassembly_record
    {
        uint64_t message_id;
        uint64_t sequence;
        uint32_t channel;
        uint8_t lane;
        networking_message_delivery delivery;
        uint32_t total_size;
        uint32_t fragment_count;
        uint32_t received_count;
        uint64_t expires_at;
        ft_vector<uint8_t> payload;
        ft_vector<uint8_t> received;

        reassembly_record() noexcept
            : message_id(0U), sequence(0U), channel(0U), lane(0U),
              delivery(networking_message_delivery::UNRELIABLE), total_size(0U),
              fragment_count(0U), received_count(0U), expires_at(0U), payload(), received()
        {
            return ;
        }

        int32_t initialize() noexcept
        {
            int32_t result;

            result = this->payload.initialize();
            if (result != FT_ERR_SUCCESS)
                return (result);
            result = this->received.initialize();
            if (result != FT_ERR_SUCCESS)
            {
                (void)this->payload.destroy();
                return (result);
            }
            return (FT_ERR_SUCCESS);
        }

        ~reassembly_record() noexcept
        {
            (void)this->payload.destroy();
            (void)this->received.destroy();
            return ;
        }
    };

    static void write_u16(ft_vector<uint8_t> &buffer, uint16_t value) noexcept
    {
        buffer.push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
        buffer.push_back(static_cast<uint8_t>(value & 0xffU));
        return ;
    }

    static void write_u32(ft_vector<uint8_t> &buffer, uint32_t value) noexcept
    {
        uint32_t shift;

        shift = 24U;
        while (true)
        {
            buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
            if (shift == 0U)
                break ;
            shift -= 8U;
        }
        return ;
    }

    static void write_u64(ft_vector<uint8_t> &buffer, uint64_t value) noexcept
    {
        uint32_t shift;

        shift = 56U;
        while (true)
        {
            buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
            if (shift == 0U)
                break ;
            shift -= 8U;
        }
        return ;
    }

    static ft_bool networking_message_encode_handshake(
        uint8_t handshake_type, uint64_t connection_id,
        const uint8_t *payload, ft_size_t payload_size,
        ft_vector<uint8_t> &packet, uint32_t maximum_size) noexcept
    {
        ft_size_t index;

        if (payload == ft_nullptr || payload_size > 65535U
            || 4U + 8U + 2U + payload_size > maximum_size)
            return (FT_FALSE);
        packet.clear();
        packet.reserve(4U + 8U + 2U + payload_size);
        packet.push_back(NETWORKING_MESSAGE_MAGIC);
        packet.push_back(NETWORKING_MESSAGE_VERSION);
        packet.push_back(handshake_type);
        packet.push_back(0U);
        write_u64(packet, connection_id);
        write_u16(packet, static_cast<uint16_t>(payload_size));
        index = 0U;
        while (index < payload_size)
        {
            packet.push_back(payload[index]);
            index += 1U;
        }
        if (packet.get_error() != FT_ERR_SUCCESS || packet.size() > maximum_size)
            return (FT_FALSE);
        return (FT_TRUE);
    }

    static ft_bool read_u16(const uint8_t *data, ft_size_t size,
        ft_size_t &offset, uint16_t &value) noexcept
    {
        if (offset > size || size - offset < 2U)
            return (FT_FALSE);
        value = static_cast<uint16_t>(data[offset]) << 8U;
        value = static_cast<uint16_t>(value | data[offset + 1U]);
        offset += 2U;
        return (FT_TRUE);
    }

    static ft_bool read_u32(const uint8_t *data, ft_size_t size,
        ft_size_t &offset, uint32_t &value) noexcept
    {
        uint32_t index;

        if (offset > size || size - offset < 4U)
            return (FT_FALSE);
        value = 0U;
        index = 0U;
        while (index < 4U)
        {
            value = (value << 8U) | data[offset + index];
            index += 1U;
        }
        offset += 4U;
        return (FT_TRUE);
    }

    static ft_bool read_u64(const uint8_t *data, ft_size_t size,
        ft_size_t &offset, uint64_t &value) noexcept
    {
        uint32_t index;

        if (offset > size || size - offset < 8U)
            return (FT_FALSE);
        value = 0U;
        index = 0U;
        while (index < 8U)
        {
            value = (value << 8U) | data[offset + index];
            index += 1U;
        }
        offset += 8U;
        return (FT_TRUE);
    }

    static ft_bool is_reliable(networking_message_delivery delivery) noexcept
    {
        if (delivery == networking_message_delivery::RELIABLE_ORDERED)
            return (FT_TRUE);
        return (FT_FALSE);
    }

    static ft_bool networking_message_has_nonzero_secret(
        const uint8_t secret[32]) noexcept
    {
        uint32_t index;
        uint8_t value;

        value = 0U;
        index = 0U;
        while (index < 32U)
        {
            value = static_cast<uint8_t>(value | secret[index]);
            index += 1U;
        }
        if (value == 0U)
            return (FT_FALSE);
        return (FT_TRUE);
    }

    static ft_bool networking_message_endpoint_equal(
        const networking_message_endpoint &left,
        const networking_message_endpoint &right) noexcept
    {
        if (left.length != right.length)
            return (FT_FALSE);
        if (left.length == 0U)
            return (FT_TRUE);
        if (ft_memcmp(&left.address, &right.address, left.length) != 0)
            return (FT_FALSE);
        return (FT_TRUE);
    }

    static int32_t networking_message_send_datagram(
        networking_datagram_io &io, const networking_message_endpoint &destination,
        const uint8_t *data, ft_size_t size) noexcept
    {
        if (NETWORKING_TEST_SHOULD_FAIL(
                NETWORKING_TEST_DATAGRAM_SEND) != FT_FALSE)
            return (FT_ERR_SOCKET_SEND_FAILED);
        return (io.send_datagram(destination, data, size));
    }
}

struct networking_message_transport::connection_record
{
    struct lane_state
    {
        uint32_t priority_weight;
        uint32_t reserved_bandwidth_bytes_per_second;
        uint64_t deficit;
        uint64_t sent_bytes_window;
        uint64_t window_started;

        lane_state() noexcept
            : priority_weight(1U), reserved_bandwidth_bytes_per_second(0U),
              deficit(0U), sent_bytes_window(0U), window_started(0U)
        {
            return ;
        }
    };

    struct channel_state
    {
        uint32_t channel;
        uint64_t next_reliable_sequence;
        uint64_t latest_unreliable_sequence;

        channel_state() noexcept
            : channel(0U), next_reliable_sequence(1U), latest_unreliable_sequence(0U)
        {
            return ;
        }
    };

    networking_message_endpoint remote;
    uint64_t id;
    networking_message_connection_state state;
    uint64_t next_packet;
    uint64_t next_message;
    uint64_t next_sequence;
    uint64_t largest_received;
    networking_ack_range received_ranges[NETWORKING_MESSAGE_ACK_RANGE_LIMIT];
    uint32_t received_range_count;
    uint64_t last_acknowledged_sent;
    uint64_t next_ordered_sequence;
    uint64_t last_receive;
    uint64_t last_send;
    uint64_t smoothed_rtt;
    uint64_t latest_rtt;
    uint64_t minimum_rtt;
    uint64_t rtt_variance;
    uint64_t previous_rtt;
    uint64_t bytes_in_flight;
    uint64_t congestion_window;
    uint64_t slow_start_threshold;
    uint64_t pacing_next_send;
    uint32_t maximum_queued_bytes;
    lane_state lanes[4];
    ft_deque<outgoing_frame *> pending_lanes[4];
    ft_vector<sent_packet *> sent;
    ft_size_t retransmission_cursor;
    ft_vector<reassembly_record *> reassembly;
    ft_vector<channel_state> channels;
    ft_vector<networking_received_message *> ordered_pending;
    networking_message_statistics statistics;
    ft_bool secure_enabled;
    networking_secure_channel secure_channel;
    networking_handshake handshake;
    ft_bool handshake_enabled;
    ft_bool incoming_pending;
    ft_bool finished_sent;
    uint32_t handshake_attempts;
    ft_bool key_update_pending;
    uint64_t key_update_epoch;
    uint64_t key_update_sent_at;
    uint64_t previous_receive_key_expiry;
    uint64_t draining_started;
    networking_message_close_reason draining_reason;
    char draining_debug_text[96];
    networking_message_endpoint pending_path;
    uint8_t pending_path_token[NETWORKING_MESSAGE_PATH_TOKEN_SIZE];
    uint64_t path_challenge_sent;
    ft_bool path_validation_pending;
    uint64_t remote_flow_credit;
    ft_bool remote_flow_control_received;
    uint64_t reliable_flow_reserved;
    uint64_t receive_flow_credit;
    uint64_t advertised_flow_credit;
    ft_bool containers_initialised;

    connection_record() noexcept
        : remote(), id(0U), state(networking_message_connection_state::IDLE),
          next_packet(1U), next_message(1U), next_sequence(1U), largest_received(0U),
          received_ranges(), received_range_count(0U), last_acknowledged_sent(0U),
          next_ordered_sequence(1U), last_receive(0U), last_send(0U),
          smoothed_rtt(0U), latest_rtt(0U), minimum_rtt(0U), rtt_variance(0U),
          previous_rtt(0U), bytes_in_flight(0U), congestion_window(12000U),
          slow_start_threshold(48000U), pacing_next_send(0U),
          maximum_queued_bytes(0U),
          lanes(), pending_lanes(), sent(), retransmission_cursor(0U),
          reassembly(), channels(), ordered_pending(),
          statistics(), secure_enabled(FT_FALSE), handshake(),
          handshake_enabled(FT_FALSE), incoming_pending(FT_FALSE),
          finished_sent(FT_FALSE),
          handshake_attempts(0U), key_update_pending(FT_FALSE),
          key_update_epoch(0U), key_update_sent_at(0U),
          previous_receive_key_expiry(0U),
          draining_started(0U),
          draining_reason(networking_message_close_reason::APPLICATION),
          draining_debug_text(),
          pending_path(), pending_path_token(), path_challenge_sent(0U),
          path_validation_pending(FT_FALSE),
          remote_flow_credit(UINT64_MAX), remote_flow_control_received(FT_FALSE),
          reliable_flow_reserved(0U),
          receive_flow_credit(0U), advertised_flow_credit(0U),
          containers_initialised(FT_FALSE)
    {
        this->lanes[0].priority_weight = 16U;
        this->lanes[1].priority_weight = 8U;
        this->lanes[2].priority_weight = 4U;
        this->lanes[3].priority_weight = 1U;
        uint32_t lane_index;

        lane_index = 0U;
        while (lane_index < 4U)
        {
            this->statistics.lane_priority_weight[lane_index] =
                this->lanes[lane_index].priority_weight;
            this->statistics.lane_reserved_bandwidth_bytes_per_second[lane_index]
                = this->lanes[lane_index].reserved_bandwidth_bytes_per_second;
            lane_index += 1U;
        }
        ft_memset(&this->remote, 0, sizeof(this->remote));
        ft_memset(this->draining_debug_text, 0,
            sizeof(this->draining_debug_text));
        ft_memset(&this->pending_path, 0, sizeof(this->pending_path));
        ft_memset(this->pending_path_token, 0,
            sizeof(this->pending_path_token));
        return ;
    }

    int32_t initialize() noexcept
    {
        uint32_t lane;
        int32_t result;

        if (this->containers_initialised != FT_FALSE)
            return (FT_ERR_ALREADY_INITIALISED);
        lane = 0U;
        while (lane < 4U)
        {
            result = this->pending_lanes[lane].initialize();
            if (result != FT_ERR_SUCCESS)
            {
                uint32_t cleanup_lane = 0U;
                while (cleanup_lane < lane)
                {
                    (void)this->pending_lanes[cleanup_lane].destroy();
                    cleanup_lane += 1U;
                }
                return (result);
            }
            lane += 1U;
        }
        result = this->sent.initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->reassembly.initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->channels.initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->ordered_pending.initialize();
        if (result != FT_ERR_SUCCESS)
        {
            (void)this->ordered_pending.destroy();
            (void)this->channels.destroy();
            (void)this->reassembly.destroy();
            (void)this->sent.destroy();
            lane = 0U;
            while (lane < 4U)
            {
                (void)this->pending_lanes[lane].destroy();
                lane += 1U;
            }
            return (result);
        }
        this->containers_initialised = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }

    ~connection_record() noexcept
    {
        ft_size_t index;
        uint32_t lane;

        if (this->containers_initialised == FT_FALSE)
            return ;
        lane = 0U;
        while (lane < 4U)
        {
            while (!this->pending_lanes[lane].empty())
            {
                outgoing_frame *frame = this->pending_lanes[lane].pop_front();
                delete frame;
            }
            (void)this->pending_lanes[lane].destroy();
            lane += 1U;
        }
        index = 0U;
        while (index < this->sent.size())
        {
            delete this->sent[index]->frame;
            delete this->sent[index];
            index += 1U;
        }
        this->sent.clear();
        (void)this->sent.destroy();
        index = 0U;
        while (index < this->reassembly.size())
        {
            delete this->reassembly[index];
            index += 1U;
        }
        this->reassembly.clear();
        (void)this->reassembly.destroy();
        index = 0U;
        while (index < this->ordered_pending.size())
        {
            delete this->ordered_pending[index];
            index += 1U;
        }
        this->ordered_pending.clear();
        (void)this->ordered_pending.destroy();
        (void)this->channels.destroy();
        return ;
    }
};

static networking_message_transport::connection_record::channel_state *
networking_message_find_channel_state(networking_message_transport::connection_record &connection,
    uint32_t channel) noexcept
{
    ft_size_t index;

    index = 0U;
    while (index < connection.channels.size())
    {
        if (connection.channels[index].channel == channel)
            return (&connection.channels[index]);
        index += 1U;
    }
    networking_message_transport::connection_record::channel_state state;
    state.channel = channel;
    if (connection.channels.push_back(state) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (&connection.channels[connection.channels.size() - 1U]);
}

static uint32_t networking_message_select_lane(
    networking_message_transport::connection_record &connection,
    uint64_t now_milliseconds) noexcept
{
    uint32_t lane_index;
    uint32_t selected_lane;
    uint64_t selected_deficit;
    ft_bool reserved_lane_available;

    if (!connection.pending_lanes[0].empty())
        return (0U);
    lane_index = 1U;
    while (lane_index < 4U)
    {
        if (connection.lanes[lane_index].window_started == 0U
            || (now_milliseconds >= connection.lanes[lane_index].window_started
                && now_milliseconds - connection.lanes[lane_index].window_started
                    >= 1000U))
        {
            connection.lanes[lane_index].window_started = now_milliseconds;
            connection.lanes[lane_index].sent_bytes_window = 0U;
        }
        lane_index += 1U;
    }
    reserved_lane_available = FT_FALSE;
    lane_index = 1U;
    while (lane_index < 4U)
    {
        if (!connection.pending_lanes[lane_index].empty()
            && connection.lanes[lane_index].reserved_bandwidth_bytes_per_second != 0U
            && connection.lanes[lane_index].sent_bytes_window
                < connection.lanes[lane_index].reserved_bandwidth_bytes_per_second)
            reserved_lane_available = FT_TRUE;
        lane_index += 1U;
    }
    selected_lane = 4U;
    selected_deficit = 0U;
    lane_index = 1U;
    while (lane_index < 4U)
    {
        if (!connection.pending_lanes[lane_index].empty()
            && (reserved_lane_available == FT_FALSE
                || (connection.lanes[lane_index].reserved_bandwidth_bytes_per_second != 0U
                    && connection.lanes[lane_index].sent_bytes_window
                        < connection.lanes[lane_index].reserved_bandwidth_bytes_per_second)))
        {
            connection.lanes[lane_index].deficit +=
                connection.lanes[lane_index].priority_weight;
            if (selected_lane == 4U
                || connection.lanes[lane_index].deficit > selected_deficit)
            {
                selected_lane = lane_index;
                selected_deficit = connection.lanes[lane_index].deficit;
            }
        }
        lane_index += 1U;
    }
    if (selected_lane != 4U && connection.lanes[selected_lane].deficit != 0U)
        connection.lanes[selected_lane].deficit -= 1U;
    return (selected_lane);
}

static ft_bool networking_message_record_packet(
    networking_message_transport::connection_record &connection,
    uint64_t packet_number) noexcept
{
    uint32_t index;
    uint32_t insert_index;

    index = 0U;
    while (index < connection.received_range_count)
    {
        networking_ack_range &range = connection.received_ranges[index];
        if (packet_number >= range.start && packet_number <= range.end)
            return (FT_FALSE);
        if (packet_number != static_cast<uint64_t>(-1)
            && packet_number == range.end + 1U)
        {
            range.end = packet_number;
            if (index != 0U
                && connection.received_ranges[index - 1U].start == range.end + 1U)
            {
                connection.received_ranges[index - 1U].start = range.start;
                while (index + 1U < connection.received_range_count)
                {
                    connection.received_ranges[index] =
                        connection.received_ranges[index + 1U];
                    index += 1U;
                }
                connection.received_range_count -= 1U;
            }
            return (FT_TRUE);
        }
        if (packet_number != 0U && packet_number == range.start - 1U)
        {
            range.start = packet_number;
            if (index + 1U < connection.received_range_count
                && connection.received_ranges[index + 1U].end + 1U
                    == range.start)
            {
                range.start = connection.received_ranges[index + 1U].start;
                while (index + 2U < connection.received_range_count)
                {
                    connection.received_ranges[index + 1U] =
                        connection.received_ranges[index + 2U];
                    index += 1U;
                }
                connection.received_range_count -= 1U;
            }
            return (FT_TRUE);
        }
        index += 1U;
    }
    insert_index = 0U;
    while (insert_index < connection.received_range_count
        && connection.received_ranges[insert_index].start > packet_number)
        insert_index += 1U;
    if (connection.received_range_count < NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
    {
        if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_ACK_RANGE_GROWTH)
            != FT_FALSE)
            return (FT_FALSE);
        index = connection.received_range_count;
        while (index > insert_index)
        {
            connection.received_ranges[index] =
                connection.received_ranges[index - 1U];
            index -= 1U;
        }
        connection.received_ranges[insert_index].start = packet_number;
        connection.received_ranges[insert_index].end = packet_number;
        connection.received_range_count += 1U;
    }
    else if (insert_index < NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
    {
        index = NETWORKING_MESSAGE_ACK_RANGE_LIMIT - 1U;
        while (index > insert_index)
        {
            connection.received_ranges[index] =
                connection.received_ranges[index - 1U];
            index -= 1U;
        }
        connection.received_ranges[insert_index].start = packet_number;
        connection.received_ranges[insert_index].end = packet_number;
    }
    return (FT_TRUE);
}

static networking_received_message *networking_message_clone(
    const networking_received_message &message) noexcept
{
    networking_received_message *clone;

    if (NETWORKING_TEST_SHOULD_FAIL(
            NETWORKING_TEST_RECEIVED_MESSAGE_ALLOCATE) != FT_FALSE)
        clone = ft_nullptr;
    else
        clone = new (std::nothrow) networking_received_message();
    if (clone == ft_nullptr)
        return (ft_nullptr);
    clone->connection_id = message.connection_id;
    clone->channel = message.channel;
    clone->lane = message.lane;
    clone->delivery = message.delivery;
    clone->sequence = message.sequence;
    if (clone->payload.initialize(message.payload) != FT_ERR_SUCCESS)
    {
        delete clone;
        return (ft_nullptr);
    }
    return (clone);
}

static ft_bool networking_message_append_received(
    ft_deque<networking_received_message *> &received_messages,
    const networking_received_message &message) noexcept
{
    networking_received_message *stored_message;

    stored_message = networking_message_clone(message);
    if (stored_message == ft_nullptr)
        return (FT_FALSE);
    received_messages.push_back(stored_message);
    if (received_messages.get_error() != FT_ERR_SUCCESS)
    {
        delete stored_message;
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

static void networking_message_apply_ack(
    networking_message_transport::connection_record &connection,
    uint64_t largest_acknowledged, uint32_t acknowledged_range_count,
    const uint16_t *acknowledged_range_starts,
    const uint16_t *acknowledged_range_ends,
    uint64_t now_milliseconds) noexcept
{
    ft_size_t sent_index;
    uint32_t lane_index;

    sent_index = 0U;
    while (sent_index < connection.sent.size())
    {
        sent_packet *sent = connection.sent[sent_index];
        ft_bool acknowledged;
        uint64_t difference;
        uint32_t range_index;

        acknowledged = FT_FALSE;
        if (sent->packet_number == largest_acknowledged)
            acknowledged = FT_TRUE;
        else if (sent->packet_number < largest_acknowledged
            && acknowledged_range_starts != ft_nullptr
            && acknowledged_range_ends != ft_nullptr)
        {
            difference = largest_acknowledged - sent->packet_number;
            range_index = 0U;
            while (range_index < acknowledged_range_count)
            {
                if (difference >= acknowledged_range_starts[range_index]
                    && difference <= acknowledged_range_ends[range_index])
                {
                    acknowledged = FT_TRUE;
                    break ;
                }
                range_index += 1U;
            }
        }
        if (acknowledged == FT_FALSE)
        {
            sent_index += 1U;
            continue ;
        }
        connection.statistics.packets_acknowledged += 1U;
        if (sent->wire_size <= connection.bytes_in_flight)
            connection.bytes_in_flight -= sent->wire_size;
        else
            connection.bytes_in_flight = 0U;
        if (sent->retransmitted == FT_FALSE)
        {
            uint64_t measured_rtt = now_milliseconds - sent->sent_at;
            uint64_t difference_from_smoothed;

            connection.latest_rtt = measured_rtt;
            if (connection.minimum_rtt == 0U || measured_rtt < connection.minimum_rtt)
                connection.minimum_rtt = measured_rtt;
            if (connection.smoothed_rtt == 0U)
            {
                connection.smoothed_rtt = measured_rtt;
                connection.rtt_variance = measured_rtt / 2U;
            }
            else
            {
                if (connection.smoothed_rtt > measured_rtt)
                    difference_from_smoothed = connection.smoothed_rtt - measured_rtt;
                else
                    difference_from_smoothed = measured_rtt - connection.smoothed_rtt;
                connection.rtt_variance = (connection.rtt_variance * 3U
                    + difference_from_smoothed) / 4U;
                connection.smoothed_rtt = (connection.smoothed_rtt * 7U
                    + measured_rtt) / 8U;
            }
            if (connection.previous_rtt != 0U)
            {
                if (connection.previous_rtt > measured_rtt)
                    connection.statistics.jitter_milliseconds =
                        connection.previous_rtt - measured_rtt;
                else
                    connection.statistics.jitter_milliseconds =
                        measured_rtt - connection.previous_rtt;
            }
            connection.previous_rtt = measured_rtt;
            connection.statistics.latest_rtt_milliseconds = connection.latest_rtt;
            connection.statistics.minimum_rtt_milliseconds = connection.minimum_rtt;
            connection.statistics.smoothed_rtt_milliseconds = connection.smoothed_rtt;
            connection.statistics.rtt_variance_milliseconds = connection.rtt_variance;
        }
        connection.statistics.last_acknowledgement_milliseconds = now_milliseconds;
        connection.statistics.last_progress_milliseconds = now_milliseconds;
        if (connection.congestion_window < connection.slow_start_threshold)
            connection.congestion_window += sent->wire_size;
        else if (connection.congestion_window != 0U)
        {
            uint64_t increase;

            increase = sent->wire_size * sent->wire_size;
            if (increase / sent->wire_size != sent->wire_size)
                increase = connection.congestion_window;
            if (increase < connection.congestion_window)
                increase = 1U;
            else
                increase = increase / connection.congestion_window;
            connection.congestion_window += increase;
        }
        if (connection.congestion_window > 48000U)
            connection.congestion_window = 48000U;
        connection.statistics.queue_bytes -= std::min<uint64_t>(
            connection.statistics.queue_bytes, sent->frame->payload.size());
        lane_index = static_cast<uint32_t>(sent->frame->lane) % 4U;
        connection.statistics.lane_queued_bytes[lane_index] -=
            std::min<uint64_t>(
                connection.statistics.lane_queued_bytes[lane_index],
                sent->frame->payload.size());
        if (is_reliable(sent->frame->delivery) != FT_FALSE)
            connection.reliable_flow_reserved -= std::min<uint64_t>(
                connection.reliable_flow_reserved,
                sent->frame->payload.size());
        delete sent->frame;
        delete sent;
        connection.sent.erase(connection.sent.begin() + sent_index);
    }
}

static ft_bool networking_message_append_ordered(
    ft_vector<networking_received_message *> &ordered_messages,
    const networking_received_message &message) noexcept
{
    networking_received_message *stored_message;

    stored_message = networking_message_clone(message);
    if (stored_message == ft_nullptr)
        return (FT_FALSE);
    if (ordered_messages.push_back(stored_message) != FT_ERR_SUCCESS)
    {
        delete stored_message;
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

static ft_bool networking_message_append_received(
    ft_deque<networking_received_message *> &received_messages,
    networking_received_message *message) noexcept
{
    if (message == ft_nullptr)
        return (FT_FALSE);
    received_messages.push_back(message);
    if (received_messages.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (FT_TRUE);
}

static void networking_message_record_lane_receive(
    networking_message_transport::connection_record &connection,
    const networking_received_message &message) noexcept
{
    uint32_t lane_index;
    uint64_t payload_size;

    lane_index = static_cast<uint32_t>(message.lane) % 4U;
    payload_size = message.payload.size();
    connection.statistics.lane_messages_received[lane_index] += 1U;
    connection.statistics.lane_bytes_received[lane_index] += payload_size;
}

static void networking_message_deliver(
    networking_message_transport::connection_record &connection,
    networking_received_message &message,
    ft_deque<networking_received_message *> &received_messages) noexcept
{
    networking_message_transport::connection_record::channel_state *channel_state;

    channel_state = networking_message_find_channel_state(connection, message.channel);
    if (channel_state == ft_nullptr)
        return ;
    if (message.delivery == networking_message_delivery::UNRELIABLE_SEQUENCED)
    {
        if (message.sequence <= channel_state->latest_unreliable_sequence)
            return ;
        channel_state->latest_unreliable_sequence = message.sequence;
        if (networking_message_append_received(received_messages, message) != FT_FALSE)
        {
            connection.statistics.messages_received += 1U;
            networking_message_record_lane_receive(connection, message);
        }
        return ;
    }
    if (message.delivery != networking_message_delivery::RELIABLE_ORDERED)
    {
        if (networking_message_append_received(received_messages, message) != FT_FALSE)
        {
            connection.statistics.messages_received += 1U;
            networking_message_record_lane_receive(connection, message);
        }
        return ;
    }
    if (message.sequence < channel_state->next_reliable_sequence)
        return ;
    if (message.sequence > channel_state->next_reliable_sequence)
    {
        (void)networking_message_append_ordered(connection.ordered_pending, message);
        return ;
    }
    if (networking_message_append_received(received_messages, message) == FT_FALSE)
        return ;
    connection.statistics.messages_received += 1U;
    networking_message_record_lane_receive(connection, message);
    channel_state->next_reliable_sequence += 1U;
    while (true)
    {
        ft_size_t index;
        ft_bool delivered_pending;

        delivered_pending = FT_FALSE;
        index = 0U;
        while (index < connection.ordered_pending.size())
        {
            networking_received_message *pending_message = connection.ordered_pending[index];
            if (pending_message->channel == message.channel
                && pending_message->sequence == channel_state->next_reliable_sequence)
            {
                if (networking_message_append_received(received_messages,
                        pending_message) != FT_FALSE)
                {
                    connection.statistics.messages_received += 1U;
                    networking_message_record_lane_receive(connection,
                        *pending_message);
                    pending_message = ft_nullptr;
                }
                if (pending_message != ft_nullptr)
                    delete pending_message;
                connection.ordered_pending.erase(connection.ordered_pending.begin() + index);
                channel_state->next_reliable_sequence += 1U;
                delivered_pending = FT_TRUE;
                break ;
            }
            index += 1U;
        }
        if (delivered_pending == FT_FALSE)
            break ;
    }
    return ;
}

networking_message_send_options::networking_message_send_options() noexcept
    : delivery(networking_message_delivery::RELIABLE_ORDERED), lane(2U), channel(0U),
      expiry_milliseconds(0U)
{
    return ;
}

networking_message_send_options::~networking_message_send_options() noexcept
{
    return ;
}

networking_message_transport_config::networking_message_transport_config() noexcept
    : maximum_datagram_size(1200U), maximum_message_size(1024U * 1024U),
      maximum_reassembly_bytes(8U * 1024U * 1024U), maximum_reassembly_messages(128U),
      maximum_queued_bytes(4U * 1024U * 1024U), maximum_events(256U),
      retransmission_timeout_milliseconds(250U), idle_timeout_milliseconds(30000U),
      acknowledgement_delay_milliseconds(25U), enable_reliability(FT_TRUE),
      enable_encryption(FT_TRUE), enable_authenticated_handshake(FT_FALSE),
      enable_retry_cookies(FT_FALSE), enable_peer_key_pinning(FT_FALSE),
      enable_thread_safety(FT_FALSE),
      retry_cookie_lifetime_milliseconds(10000U),
      encryption_key(), encryption_key_length(32U),
      encryption_initialization_vector(), retry_cookie_secret(),
      pinned_peer_public_key()
{
    uint32_t index;

    index = 0U;
    while (index < sizeof(this->encryption_key))
    {
        this->encryption_key[index] = 0U;
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(this->encryption_initialization_vector))
    {
        this->encryption_initialization_vector[index] = 0U;
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(this->retry_cookie_secret))
    {
        this->retry_cookie_secret[index] = 0U;
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(this->pinned_peer_public_key))
    {
        this->pinned_peer_public_key[index] = 0U;
        index += 1U;
    }
    return ;
}

networking_message_transport_config::~networking_message_transport_config() noexcept
{
    return ;
}

networking_message_event::networking_message_event() noexcept
    : type(networking_message_event_type::FAILED), connection_id(0U),
      reason(FT_ERR_SUCCESS), debug_text()
{
    ft_memset(this->debug_text, 0, sizeof(this->debug_text));
    return ;
}

networking_message_event::~networking_message_event() noexcept
{
    return ;
}

networking_message_peer_identity::networking_message_peer_identity() noexcept
    : public_key(), authenticated(FT_FALSE)
{
    ft_memset(this->public_key, 0, sizeof(this->public_key));
    return ;
}

networking_message_peer_identity::~networking_message_peer_identity() noexcept
{
    networking_crypto_backend backend;

    (void)backend.wipe(this->public_key, sizeof(this->public_key));
    this->authenticated = FT_FALSE;
    return ;
}

networking_message_peer_connect_ticket::networking_message_peer_connect_ticket()
    noexcept
    : peer_id(0U), attempt_id(0U), expires_at(0U), tie_breaker(0U),
      candidates(), candidate_count(0U), signature(), signature_length(0U)
{
    ft_memset(this->candidates, 0, sizeof(this->candidates));
    ft_memset(this->signature, 0, sizeof(this->signature));
    return ;
}

networking_message_peer_connect_ticket::~networking_message_peer_connect_ticket()
    noexcept
{
    networking_crypto_backend backend;

    (void)backend.wipe(this->signature, sizeof(this->signature));
    this->signature_length = 0U;
    return ;
}

networking_message_peer_ticket_verifier::networking_message_peer_ticket_verifier()
    noexcept
{
    return ;
}

networking_message_peer_ticket_verifier::~networking_message_peer_ticket_verifier()
    noexcept
{
    return ;
}

networking_received_message::networking_received_message() noexcept
    : connection_id(0U), channel(0U), lane(0U),
      delivery(networking_message_delivery::UNRELIABLE), sequence(0U), payload()
{
    return ;
}

networking_received_message::~networking_received_message() noexcept
{
    (void)this->payload.destroy();
    return ;
}

networking_message_statistics::networking_message_statistics() noexcept
    : packets_sent(0U), packets_received(0U), bytes_sent(0U), bytes_received(0U),
      packets_acknowledged(0U), packets_lost(0U), packets_retransmitted(0U),
      duplicate_packets(0U), reordered_packets(0U), messages_sent(0U),
      messages_received(0U), messages_expired(0U), fragments_reassembled(0U),
      fragments_dropped(0U), reassembly_bytes(0U), reassembly_messages(0U),
      smoothed_rtt_milliseconds(0U), latest_rtt_milliseconds(0U),
      minimum_rtt_milliseconds(0U), rtt_variance_milliseconds(0U),
      jitter_milliseconds(0U), queue_bytes(0U), queue_depth(0U),
      bytes_in_flight(0U), congestion_window_bytes(12000U),
      pacing_rate_bytes_per_second(0U), malformed_packets(0U),
      authentication_failures(0U), replay_rejections(0U), path_migrations(0U),
      nat_attempts(0U), relay_fallbacks(0U), last_receive_milliseconds(0U),
      last_send_milliseconds(0U), last_acknowledgement_milliseconds(0U),
      last_progress_milliseconds(0U)
{
    ft_memset(this->lane_messages_sent, 0, sizeof(this->lane_messages_sent));
    ft_memset(this->lane_messages_received, 0,
        sizeof(this->lane_messages_received));
    ft_memset(this->lane_bytes_sent, 0, sizeof(this->lane_bytes_sent));
    ft_memset(this->lane_bytes_received, 0,
        sizeof(this->lane_bytes_received));
    ft_memset(this->lane_queued_bytes, 0, sizeof(this->lane_queued_bytes));
    ft_memset(this->lane_sent_bytes_window, 0,
        sizeof(this->lane_sent_bytes_window));
    ft_memset(this->lane_rate_bytes_per_second, 0,
        sizeof(this->lane_rate_bytes_per_second));
    ft_memset(this->lane_priority_weight, 0,
        sizeof(this->lane_priority_weight));
    ft_memset(this->lane_reserved_bandwidth_bytes_per_second, 0,
        sizeof(this->lane_reserved_bandwidth_bytes_per_second));
    return ;
}

networking_message_statistics::~networking_message_statistics() noexcept
{
    return ;
}

networking_datagram_io::networking_datagram_io() noexcept
{
    return ;
}

networking_datagram_io::~networking_datagram_io() noexcept
{
    return ;
}

int32_t networking_datagram_io::wait_readable(int32_t timeout_milliseconds) noexcept
{
    (void)timeout_milliseconds;
    return (FT_ERR_UNSUPPORTED_TYPE);
}

networking_udp_datagram_io::networking_udp_datagram_io() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _connected(FT_FALSE), _socket()
{
    return ;
}

networking_udp_datagram_io::~networking_udp_datagram_io() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_udp_datagram_io::initialize(const SocketConfig &configuration) noexcept
{
    SocketConfig local_configuration;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    error_code = local_configuration.initialize(configuration);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    local_configuration._non_blocking = FT_TRUE;
    error_code = this->_socket.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_socket.initialize(local_configuration);
    (void)local_configuration.destroy();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_socket.destroy();
        return (error_code);
    }
    this->_connected = FT_FALSE;
    if (local_configuration._type == SocketType::CLIENT)
        this->_connected = FT_TRUE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_udp_datagram_io::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    (void)this->_socket.destroy();
    this->_connected = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_udp_datagram_io::move(networking_udp_datagram_io &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    if (this->_socket.move(other._socket) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    this->_connected = other._connected;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t networking_udp_datagram_io::send_datagram(
    const networking_message_endpoint &destination, const uint8_t *data,
    ft_size_t size) noexcept
{
    ssize_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (destination.length == 0U || (data == ft_nullptr && size != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_connected != FT_FALSE)
        result = this->_socket.send_to(data, size, 0, ft_nullptr, 0);
    else
        result = this->_socket.send_to(data, size, 0,
            reinterpret_cast<const struct sockaddr *>(&destination.address),
            destination.length);
    if (result < 0 || static_cast<ft_size_t>(result) != size)
        return (FT_ERR_SOCKET_SEND_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t networking_udp_datagram_io::receive_datagram(networking_message_endpoint &source,
    uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept
{
    ssize_t result;
    socklen_t address_length;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (data == ft_nullptr || received_size == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    address_length = sizeof(source.address);
    result = this->_socket.receive_from(data, capacity, 0,
        reinterpret_cast<struct sockaddr *>(&source.address), &address_length);
    if (result < 0)
    {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return (FT_ERR_EMPTY);
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return (FT_ERR_EMPTY);
#endif
        return (FT_ERR_SOCKET_RECEIVE_FAILED);
    }
    source.length = address_length;
    *received_size = static_cast<ft_size_t>(result);
    return (FT_ERR_SUCCESS);
}

uint64_t networking_udp_datagram_io::now_milliseconds() const noexcept
{
    std::chrono::steady_clock::time_point current_time;
    current_time = std::chrono::steady_clock::now();
    return (static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time.time_since_epoch()).count()));
}

int32_t networking_udp_datagram_io::wait_readable(
    int32_t timeout_milliseconds) noexcept
{
    fd_set read_set;
    timeval timeout;
    int32_t socket_fd;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (timeout_milliseconds < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    socket_fd = this->_socket.get_fd();
    if (socket_fd < 0)
        return (FT_ERR_NOT_INITIALISED);
    FD_ZERO(&read_set);
    FD_SET(socket_fd, &read_set);
    timeout.tv_sec = timeout_milliseconds / 1000;
    timeout.tv_usec = (timeout_milliseconds % 1000) * 1000;
    result = select(socket_fd + 1, &read_set, ft_nullptr, ft_nullptr,
        &timeout);
    if (result < 0)
        return (FT_ERR_SOCKET_RECEIVE_FAILED);
    if (result == 0 || !FD_ISSET(socket_fd, &read_set))
        return (FT_ERR_TIMEOUT);
    return (FT_ERR_SUCCESS);
}

int32_t networking_udp_datagram_io::get_file_descriptor() const noexcept
{
    return (this->_socket.get_fd());
}

networking_message_connection::networking_message_connection() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _transport(ft_nullptr), _connection_id(0U)
{
    return ;
}

networking_message_connection::~networking_message_connection() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_message_connection::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_transport = ft_nullptr;
    this->_connection_id = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_connection::destroy() noexcept
{
    this->_transport = ft_nullptr;
    this->_connection_id = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_connection::move(networking_message_connection &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_transport = other._transport;
    this->_connection_id = other._connection_id;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

void networking_message_connection::bind(networking_message_transport *transport,
    uint64_t connection_id) noexcept
{
    this->_transport = transport;
    this->_connection_id = connection_id;
    return ;
}

int32_t networking_message_connection::send_message(const void *data, ft_size_t size,
    const networking_message_send_options &options) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED || this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->send_connection_message(this->_connection_id, data, size, options));
}

int32_t networking_message_connection::close() noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->close_connection(this->_connection_id,
        networking_message_close_reason::APPLICATION, ft_nullptr, FT_FALSE));
}

int32_t networking_message_connection::close(
    networking_message_close_reason reason, const char *debug_text) noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->close_connection(this->_connection_id, reason,
        debug_text, FT_FALSE));
}

int32_t networking_message_connection::abort(
    networking_message_close_reason reason) noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->close_connection(this->_connection_id, reason,
        ft_nullptr, FT_TRUE));
}

int32_t networking_message_connection::update_key_epoch(
    uint64_t next_epoch) noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->update_connection_key_epoch(
        this->_connection_id, next_epoch));
}

int32_t networking_message_connection::request_key_update(
    uint64_t next_epoch) noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->request_connection_key_update(
        this->_connection_id, next_epoch));
}

int32_t networking_message_connection::configure_lane(uint8_t lane,
    uint32_t priority_weight,
    uint32_t reserved_bandwidth_bytes_per_second) noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->configure_connection_lane(this->_connection_id,
        lane, priority_weight, reserved_bandwidth_bytes_per_second));
}

int32_t networking_message_connection::set_queue_limits(
    uint32_t maximum_queued_bytes) noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->set_connection_queue_limits(this->_connection_id,
        maximum_queued_bytes));
}

int32_t networking_message_connection::flush() noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->flush_connection(this->_connection_id));
}

uint64_t networking_message_connection::get_id() const noexcept
{
    return (this->_connection_id);
}

networking_message_connection_state networking_message_connection::get_state() const noexcept
{
    networking_message_connection_state state;

    state = networking_message_connection_state::CLOSED;
    if (this->_transport != ft_nullptr
        && this->_transport->get_connection_state(this->_connection_id, state) == FT_ERR_SUCCESS)
        return (state);
    return (state);
}

int32_t networking_message_connection::get_statistics(
    networking_message_statistics &statistics) const noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->get_connection_statistics(this->_connection_id, statistics));
}

int32_t networking_message_connection::get_remote_identity(
    networking_message_peer_identity &identity) const noexcept
{
    if (this->_transport == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_transport->get_connection_identity(this->_connection_id,
        identity));
}

networking_message_transport::networking_message_transport() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _configuration(), _io(ft_nullptr),
      _listening(FT_FALSE), _listen_endpoint(),
      _connections(), _received_messages(), _events(), _callback_events(),
      _commands(),
      _event_callback(ft_nullptr), _event_callback_user_data(ft_nullptr),
      _containers_initialised(FT_FALSE),
      _next_connection_id(1U), _last_error(FT_ERR_SUCCESS), _worker_thread(),
      _worker_thread_created(FT_FALSE), _worker_native_id(0),
      _worker_stop_requested(FT_FALSE),
      _worker_wakeup_epoch(0U), _mutex(ft_nullptr)
{
    ft_memset(&this->_listen_endpoint, 0, sizeof(this->_listen_endpoint));
    return ;
}

networking_message_transport::~networking_message_transport() noexcept
{
    (void)this->stop_worker();
    (void)this->destroy();
    return ;
}

int32_t networking_message_transport::lock_transport() const noexcept
{
    return (pt_recursive_mutex_lock_if_not_null(this->_mutex));
}

void networking_message_transport::unlock_transport() const noexcept
{
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void networking_message_transport::clear_records() noexcept
{
    ft_size_t index;

    index = 0U;
    while (index < this->_connections.size())
    {
        delete this->_connections[index];
        index += 1U;
    }
    this->_connections.clear();
    while (!this->_received_messages.empty())
    {
        networking_received_message *message = this->_received_messages.pop_front();
        delete message;
    }
    this->_events.clear();
    this->_callback_events.clear();
    return ;
}

int32_t networking_message_transport::emit_event(
    networking_message_event_type type, uint64_t connection_id,
    int32_t reason, const char *debug_text) noexcept
{
    networking_message_event event;
    ft_size_t index;

    if (this->_containers_initialised == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_events.size() >= this->_configuration.maximum_events)
        return (FT_ERR_FULL);
    if (this->_event_callback != ft_nullptr
        && this->_callback_events.size() >= this->_configuration.maximum_events)
        return (FT_ERR_FULL);
    event.type = type;
    event.connection_id = connection_id;
    event.reason = reason;
    if (debug_text != ft_nullptr)
    {
        index = 0U;
        while (index + 1U < sizeof(event.debug_text)
            && debug_text[index] != '\0')
        {
            event.debug_text[index] = debug_text[index];
            index += 1U;
        }
        event.debug_text[index] = '\0';
    }
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_EVENT_ENQUEUE)
        != FT_FALSE)
        return (FT_ERR_NO_MEMORY);
    if (this->_event_callback != ft_nullptr)
    {
        if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_CALLBACK_COPY)
            != FT_FALSE)
            return (FT_ERR_NO_MEMORY);
        this->_callback_events.push_back(event);
        if (this->_callback_events.get_error() != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
    }
    this->_events.push_back(event);
    if (this->_events.get_error() != FT_ERR_SUCCESS)
    {
        if (this->_event_callback != ft_nullptr)
            (void)this->_callback_events.pop_back();
        return (FT_ERR_NO_MEMORY);
    }
    return (FT_ERR_SUCCESS);
}

void networking_message_transport::dispatch_event_callbacks() noexcept
{
    networking_message_event_callback callback;
    void *user_data;
    networking_message_event event;
    int32_t lock_result;

    while (true)
    {
        lock_result = this->lock_transport();
        if (lock_result != FT_ERR_SUCCESS)
            return ;
        callback = this->_event_callback;
        user_data = this->_event_callback_user_data;
        if (callback == ft_nullptr || this->_callback_events.empty())
        {
            this->unlock_transport();
            return ;
        }
        event = this->_callback_events.pop_front();
        if (this->_callback_events.get_error() != FT_ERR_SUCCESS)
        {
            this->unlock_transport();
            return ;
        }
        this->unlock_transport();
        callback(event, user_data);
    }
}

void networking_message_transport::wake_worker() noexcept
{
    if (this->_worker_thread_created.load(std::memory_order_acquire) == FT_FALSE)
        return ;
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_WORKER_WAKEUP)
        != FT_FALSE)
        return ;
    this->_worker_wakeup_epoch.fetch_add(1U, std::memory_order_release);
    (void)pt_thread_wake_all_uint32(&this->_worker_wakeup_epoch);
    return ;
}

int32_t networking_message_transport::dispatch_callbacks() noexcept
{
    if (this->_worker_thread_created.load(std::memory_order_acquire)
        != FT_FALSE
        && this->_worker_native_id.load(std::memory_order_acquire)
            == pt_thread_self())
        return (FT_ERR_THREAD_BUSY);
    this->dispatch_event_callbacks();
    return (FT_ERR_SUCCESS);
}

ft_bool networking_message_transport::should_queue_commands() const noexcept
{
    if (this->_worker_thread_created.load(std::memory_order_acquire)
        == FT_FALSE)
        return (FT_FALSE);
    if (this->_worker_native_id.load(std::memory_order_acquire)
        == pt_thread_self())
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t networking_message_transport::enqueue_command(
    networking_message_command &command) noexcept
{
    int32_t lock_result;
    uint32_t expected_epoch;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_containers_initialised == FT_FALSE
        || this->_worker_thread_created.load(std::memory_order_acquire)
            == FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_THREAD_BUSY);
    }
    if (this->_commands.size() >= this->_configuration.maximum_events)
    {
        this->unlock_transport();
        return (FT_ERR_FULL);
    }
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_COMMAND_ENQUEUE)
        != FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    this->_commands.push_back(&command);
    if (this->_commands.get_error() != FT_ERR_SUCCESS)
    {
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    this->wake_worker();
    this->unlock_transport();
    while (command.completed.load(std::memory_order_acquire) == FT_FALSE)
    {
        expected_epoch = command.completion_epoch.load(std::memory_order_acquire);
        if (command.completed.load(std::memory_order_acquire) != FT_FALSE)
            break ;
        (void)pt_thread_wait_uint32_timed(&command.completion_epoch,
            expected_epoch, 100U);
    }
    return (command.result);
}

void networking_message_transport::process_command_queue() noexcept
{
    networking_message_command *command;
    int32_t result;
    uint32_t completion_epoch;

    while (true)
    {
        command = ft_nullptr;
        if (this->lock_transport() != FT_ERR_SUCCESS)
            return ;
        if (this->_commands.empty() == FT_TRUE)
        {
            this->unlock_transport();
            return ;
        }
        command = this->_commands.pop_front();
        this->unlock_transport();
        if (command == ft_nullptr)
            return ;
        result = this->execute_command(*command);
        command->result = result;
        command->completed.store(FT_TRUE, std::memory_order_release);
        completion_epoch = command->completion_epoch.fetch_add(
            1U, std::memory_order_release) + 1U;
        (void)completion_epoch;
        (void)pt_thread_wake_all_uint32(&command->completion_epoch);
    }
}

int32_t networking_message_transport::execute_command(
    networking_message_command &command) noexcept
{
    if (command.type == networking_message_command_type::SEND_MESSAGE)
    {
        const void *data;

        data = ft_nullptr;
        if (command.payload.size() != 0U)
            data = &command.payload[0];
        return (this->send_connection_message(command.connection_id, data,
            command.payload.size(), command.send_options));
    }
    if (command.type == networking_message_command_type::CLOSE_CONNECTION)
        return (this->close_connection(command.connection_id,
            command.close_reason, command.debug_text,
            command.abort_connection));
    if (command.type == networking_message_command_type::UPDATE_KEY_EPOCH)
        return (this->update_connection_key_epoch(command.connection_id,
            command.key_epoch));
    if (command.type == networking_message_command_type::REQUEST_KEY_UPDATE)
        return (this->request_connection_key_update(command.connection_id,
            command.key_epoch));
    if (command.type == networking_message_command_type::CONFIGURE_LANE)
        return (this->configure_connection_lane(command.connection_id,
            command.lane, command.priority_weight,
            command.reserved_bandwidth_bytes_per_second));
    if (command.type == networking_message_command_type::SET_QUEUE_LIMITS)
        return (this->set_connection_queue_limits(command.connection_id,
            command.maximum_queued_bytes));
    if (command.type == networking_message_command_type::FLUSH_CONNECTION)
        return (this->flush_connection(command.connection_id));
    if (command.type == networking_message_command_type::LISTEN)
        return (this->listen(command.endpoint));
    if (command.type == networking_message_command_type::ACCEPT)
        return (this->accept(command.connection_id));
    if (command.type == networking_message_command_type::REJECT)
        return (this->reject(command.connection_id, command.close_reason));
    if (command.type == networking_message_command_type::OPEN_CONNECTION)
    {
        if (command.connection == ft_nullptr)
            return (FT_ERR_INVALID_POINTER);
        return (this->open_connection(command.endpoint, *command.connection));
    }
    if (command.type == networking_message_command_type::SET_EVENT_CALLBACK)
        return (this->set_event_callback(command.callback,
            command.callback_user_data));
    return (FT_ERR_INVALID_ARGUMENT);
}

void networking_message_transport::fail_pending_commands(int32_t result) noexcept
{
    networking_message_command *command;

    while (this->_commands.empty() == FT_FALSE)
    {
        command = this->_commands.pop_front();
        if (command == ft_nullptr)
            continue ;
        command->result = result;
        command->completed.store(FT_TRUE, std::memory_order_release);
        command->completion_epoch.fetch_add(1U, std::memory_order_release);
        (void)pt_thread_wake_all_uint32(&command->completion_epoch);
    }
    return ;
}

void *networking_message_transport::worker_entry(void *argument) noexcept
{
    networking_message_transport *transport;

    transport = static_cast<networking_message_transport *>(argument);
    if (transport != ft_nullptr)
        transport->worker_loop();
    return (ft_nullptr);
}

void networking_message_transport::worker_loop() noexcept
{
    uint32_t wakeup_epoch;
    int32_t poll_result;

    this->_worker_native_id.store(pt_thread_self(), std::memory_order_release);
    while (this->_worker_stop_requested.load(std::memory_order_acquire) == FT_FALSE)
    {
        this->process_command_queue();
        poll_result = this->poll(50);
        wakeup_epoch = this->_worker_wakeup_epoch.load(std::memory_order_acquire);
        if (poll_result == FT_ERR_UNSUPPORTED_TYPE
            && this->_worker_stop_requested.load(std::memory_order_acquire)
                == FT_FALSE)
            (void)pt_thread_wait_uint32_timed(&this->_worker_wakeup_epoch,
                wakeup_epoch, 50U);
    }
    this->_worker_native_id.store(0, std::memory_order_release);
    return ;
}

int32_t networking_message_transport::start_worker() noexcept
{
    int32_t result;
    int32_t lock_result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (this->_worker_thread_created.load(std::memory_order_acquire) != FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_ALREADY_INITIALISED);
    }
    this->_worker_stop_requested.store(FT_FALSE, std::memory_order_release);
    this->_worker_native_id.store(0, std::memory_order_release);
    this->_worker_wakeup_epoch.store(0U, std::memory_order_release);
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_WORKER_CREATE)
        != FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    result = pt_thread_create(&this->_worker_thread, ft_nullptr,
        &networking_message_transport::worker_entry, this);
    if (result != 0)
    {
        this->unlock_transport();
        return (FT_ERR_THREAD_BUSY);
    }
    this->_worker_thread_created.store(FT_TRUE, std::memory_order_release);
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::enable_thread_safety() noexcept
{
    pt_recursive_mutex *new_mutex;
    int32_t mutex_result;

    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (this->_worker_thread_created.load(std::memory_order_acquire)
        != FT_FALSE)
        return (FT_ERR_THREAD_BUSY);
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_MUTEX_ALLOCATE)
        != FT_FALSE)
        return (FT_ERR_NO_MEMORY);
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_result = new_mutex->initialize();
    if (mutex_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (mutex_result);
    }
    this->_mutex = new_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::disable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t lock_result;
    int32_t destroy_result;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (this->_worker_thread_created.load(std::memory_order_acquire)
        != FT_FALSE)
    {
        if (this->_worker_native_id.load(std::memory_order_acquire)
            == pt_thread_self())
            return (FT_ERR_INVALID_OPERATION);
        if (this->stop_worker() != FT_ERR_SUCCESS)
            return (FT_ERR_THREAD_BUSY);
    }
    mutex_pointer = this->_mutex;
    lock_result = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    this->_mutex = ft_nullptr;
    (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    return (destroy_result);
}

ft_bool networking_message_transport::is_thread_safe() const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t networking_message_transport::stop_worker() noexcept
{
    int32_t result;
    int32_t lock_result;

    if (this->_worker_thread_created.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_worker_native_id.load(std::memory_order_acquire)
        == pt_thread_self())
        return (FT_ERR_INVALID_OPERATION);
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_worker_thread_created.load(std::memory_order_acquire) == FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_SUCCESS);
    }
    this->_worker_stop_requested.store(FT_TRUE, std::memory_order_release);
    this->unlock_transport();
    this->wake_worker();
    result = pt_thread_join(this->_worker_thread, ft_nullptr);
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    this->_worker_thread_created.store(FT_FALSE, std::memory_order_release);
    this->_worker_native_id.store(0, std::memory_order_release);
    this->_worker_stop_requested.store(FT_FALSE, std::memory_order_release);
    this->fail_pending_commands(FT_ERR_THREAD_BUSY);
    this->unlock_transport();
    if (result != 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

ft_bool networking_message_transport::is_worker_running() const noexcept
{
    return (this->_worker_thread_created.load(std::memory_order_acquire));
}

int32_t networking_message_transport::initialize(
    const networking_message_transport_config &configuration,
    networking_datagram_io &io) noexcept
{
    int32_t thread_safety_result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (configuration.maximum_datagram_size < NETWORKING_MESSAGE_HEADER_SIZE
        + NETWORKING_MESSAGE_FRAME_HEADER_SIZE
        || configuration.maximum_message_size == 0U
        || configuration.maximum_queued_bytes == 0U
        || configuration.maximum_events == 0U
        || configuration.maximum_reassembly_bytes == 0U
        || configuration.maximum_reassembly_messages == 0U)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_encryption != FT_FALSE
        && configuration.encryption_key_length != 32U)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_authenticated_handshake != FT_FALSE
        && configuration.enable_encryption == FT_FALSE)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_retry_cookies != FT_FALSE
        && configuration.enable_authenticated_handshake == FT_FALSE)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_peer_key_pinning != FT_FALSE
        && configuration.enable_authenticated_handshake == FT_FALSE)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_peer_key_pinning != FT_FALSE
        && networking_message_has_nonzero_secret(
            configuration.pinned_peer_public_key) == FT_FALSE)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_retry_cookies != FT_FALSE
        && configuration.retry_cookie_lifetime_milliseconds == 0U)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_retry_cookies != FT_FALSE
        && networking_message_has_nonzero_secret(configuration.retry_cookie_secret)
            == FT_FALSE)
        return (FT_ERR_CONFIGURATION);
    if (configuration.enable_thread_safety != FT_FALSE)
    {
        thread_safety_result = this->enable_thread_safety();
        if (thread_safety_result != FT_ERR_SUCCESS)
            return (thread_safety_result);
    }
    if (this->_containers_initialised == FT_FALSE)
    {
        if (this->_connections.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->disable_thread_safety();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        if (this->_received_messages.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->_connections.destroy();
            (void)this->disable_thread_safety();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        if (this->_events.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->_received_messages.destroy();
            (void)this->_connections.destroy();
            (void)this->disable_thread_safety();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        if (this->_callback_events.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->_events.destroy();
            (void)this->_received_messages.destroy();
            (void)this->_connections.destroy();
            (void)this->disable_thread_safety();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        if (this->_commands.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->_callback_events.destroy();
            (void)this->_events.destroy();
            (void)this->_received_messages.destroy();
            (void)this->_connections.destroy();
            (void)this->disable_thread_safety();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        this->_containers_initialised = FT_TRUE;
    }
    this->_configuration = configuration;
    this->_io = &io;
    this->_listening = FT_FALSE;
    ft_memset(&this->_listen_endpoint, 0, sizeof(this->_listen_endpoint));
    this->_next_connection_id = 1U;
    this->_last_error = FT_ERR_SUCCESS;
    this->_event_callback = ft_nullptr;
    this->_event_callback_user_data = ft_nullptr;
    this->_worker_stop_requested.store(FT_FALSE, std::memory_order_release);
    this->_worker_native_id.store(0, std::memory_order_release);
    this->_worker_wakeup_epoch.store(0U, std::memory_order_release);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::destroy() noexcept
{
    int32_t lock_result;

    if (this->_worker_thread_created.load(std::memory_order_acquire) != FT_FALSE)
        (void)this->stop_worker();
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        (void)this->disable_thread_safety();
        return (FT_ERR_SUCCESS);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_containers_initialised != FT_FALSE)
    {
        this->clear_records();
        this->fail_pending_commands(FT_ERR_THREAD_BUSY);
    }
    (void)this->_connections.destroy();
    (void)this->_received_messages.destroy();
    (void)this->_events.destroy();
    (void)this->_callback_events.destroy();
    (void)this->_commands.destroy();
    this->_containers_initialised = FT_FALSE;
    this->_io = ft_nullptr;
    this->_listening = FT_FALSE;
    ft_memset(&this->_listen_endpoint, 0, sizeof(this->_listen_endpoint));
    this->_worker_stop_requested.store(FT_FALSE, std::memory_order_release);
    this->_worker_native_id.store(0, std::memory_order_release);
    this->_worker_wakeup_epoch.store(0U, std::memory_order_release);
    this->_event_callback = ft_nullptr;
    this->_event_callback_user_data = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->unlock_transport();
    (void)this->disable_thread_safety();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::move(networking_message_transport &other) noexcept
{
    int32_t lock_result;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    lock_result = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
        return (FT_ERR_INVALID_STATE);
    }
    if (other._worker_thread_created.load(std::memory_order_acquire) != FT_FALSE)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
        return (FT_ERR_THREAD_BUSY);
    }
    (void)this->destroy();
    this->_configuration = other._configuration;
    this->_io = other._io;
    this->_listening = other._listening;
    this->_listen_endpoint = other._listen_endpoint;
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    if (other._containers_initialised != FT_FALSE)
    {
        if (this->_connections.move(other._connections) != FT_ERR_SUCCESS
            || this->_received_messages.move(other._received_messages) != FT_ERR_SUCCESS
            || this->_events.move(other._events) != FT_ERR_SUCCESS
            || this->_callback_events.move(other._callback_events)
            || this->_commands.move(other._commands)
                != FT_ERR_SUCCESS)
        {
            this->unlock_transport();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        this->_containers_initialised = FT_TRUE;
    }
    else
        this->_containers_initialised = FT_FALSE;
    this->_next_connection_id = other._next_connection_id;
    this->_last_error = other._last_error;
    this->_event_callback = other._event_callback;
    this->_event_callback_user_data = other._event_callback_user_data;
    this->_worker_thread_created.store(FT_FALSE, std::memory_order_release);
    this->_worker_native_id.store(0, std::memory_order_release);
    this->_worker_stop_requested.store(FT_FALSE, std::memory_order_release);
    this->_worker_wakeup_epoch.store(0U, std::memory_order_release);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._io = ft_nullptr;
    other._listening = FT_FALSE;
    ft_memset(&other._listen_endpoint, 0, sizeof(other._listen_endpoint));
    other._containers_initialised = FT_FALSE;
    other._event_callback = ft_nullptr;
    other._event_callback_user_data = ft_nullptr;
    this->unlock_transport();
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

networking_message_transport::connection_record *
networking_message_transport::find_connection(uint64_t connection_id) const noexcept
{
    ft_size_t index;

    index = 0U;
    while (index < this->_connections.size())
    {
        if (this->_connections[index]->id == connection_id)
            return (this->_connections[index]);
        index += 1U;
    }
    return (ft_nullptr);
}

int32_t networking_message_transport::listen(
    const networking_message_endpoint &local) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::LISTEN;
        command->endpoint = local;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_io == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (local.length <= 0
        || static_cast<ft_size_t>(local.length) > sizeof(local.address))
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_listen_endpoint = local;
    this->_listening = FT_TRUE;
    this->wake_worker();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::accept(uint64_t connection_id) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    int32_t result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::ACCEPT;
        command->connection_id = connection_id;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->state != networking_message_connection_state::HANDSHAKING
        || connection->incoming_pending == FT_FALSE
        || connection->handshake.get_role() != networking_handshake_role::SERVER)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    connection->incoming_pending = FT_FALSE;
    result = this->send_handshake_hello(*connection);
    if (result == FT_ERR_SUCCESS)
        result = this->send_handshake_finished(*connection);
    if (result != FT_ERR_SUCCESS)
    {
        connection->state = networking_message_connection_state::FAILED;
        (void)this->emit_event(networking_message_event_type::FAILED,
            connection->id, result, "accept handshake failed");
        this->unlock_transport();
        return (result);
    }
    connection->handshake_attempts = 1U;
    this->wake_worker();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::reject(uint64_t connection_id,
    networking_message_close_reason reason) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::REJECT;
        command->connection_id = connection_id;
        command->close_reason = reason;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->state != networking_message_connection_state::HANDSHAKING
        || connection->incoming_pending == FT_FALSE
        || connection->handshake.get_role() != networking_handshake_role::SERVER)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    connection->incoming_pending = FT_FALSE;
    connection->state = networking_message_connection_state::FAILED;
    (void)this->emit_event(networking_message_event_type::FAILED,
        connection->id, static_cast<int32_t>(reason), "connection rejected");
    this->wake_worker();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::connect(
    const networking_message_endpoint &remote,
    networking_message_connection &connection) noexcept
{
    return (this->open_connection(remote, connection));
}

int32_t networking_message_transport::connect_peer(
    const networking_message_peer_connect_ticket &ticket,
    networking_message_connection &connection) noexcept
{
    (void)ticket;
    (void)connection;
    return (FT_ERR_UNSUPPORTED_TYPE);
}

int32_t networking_message_transport::connect_peer(
    const networking_message_peer_connect_ticket &ticket,
    networking_message_peer_ticket_verifier &verifier,
    networking_message_connection &connection) noexcept
{
    int32_t lock_result;
    uint32_t candidate_index;
    uint32_t selected_index;
    uint64_t now;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_io == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (ticket.peer_id == 0U || ticket.attempt_id == 0U
        || ticket.candidate_count == 0U
        || ticket.candidate_count > NETWORKING_MESSAGE_MAX_PEER_CANDIDATES
        || ticket.signature_length == 0U
        || ticket.signature_length > sizeof(ticket.signature))
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    now = this->_io->now_milliseconds();
    this->unlock_transport();
    if (ticket.expires_at <= now)
        return (FT_ERR_PERMISSION_DENIED);

    if (verifier.verify(ticket) == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    selected_index = NETWORKING_MESSAGE_MAX_PEER_CANDIDATES;
    candidate_index = 0U;
    while (candidate_index < ticket.candidate_count)
    {
        if (ticket.candidates[candidate_index].length == 0U
            || ticket.candidates[candidate_index].length
                > static_cast<socklen_t>(sizeof(
                    ticket.candidates[candidate_index].address)))
            return (FT_ERR_INVALID_ARGUMENT);
        if (selected_index == NETWORKING_MESSAGE_MAX_PEER_CANDIDATES)
            selected_index = candidate_index;
        candidate_index += 1U;
    }
    if (selected_index == NETWORKING_MESSAGE_MAX_PEER_CANDIDATES)
        return (FT_ERR_NOT_FOUND);
    return (this->open_connection(ticket.candidates[selected_index], connection));
}

int32_t networking_message_transport::open_connection(
    const networking_message_endpoint &remote,
    networking_message_connection &connection) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *record;
    int32_t result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::OPEN_CONNECTION;
        command->endpoint = remote;
        command->connection = &connection;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED || this->_io == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (remote.length == 0U)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_CONNECTION_ALLOCATE)
        != FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    record = new (std::nothrow) connection_record();
    if (record == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    if (record->initialize() != FT_ERR_SUCCESS)
    {
        delete record;
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    record->remote = remote;
    record->id = this->_next_connection_id++;
    record->maximum_queued_bytes = this->_configuration.maximum_queued_bytes;
    record->remote_flow_credit = this->_configuration.maximum_reassembly_bytes;
    record->receive_flow_credit = this->_configuration.maximum_reassembly_bytes;
    record->secure_enabled = this->_configuration.enable_encryption;
    record->handshake_enabled = this->_configuration.enable_authenticated_handshake;
    if (record->handshake_enabled != FT_FALSE)
    {
        record->state = networking_message_connection_state::HANDSHAKING;
        result = record->handshake.initialize(networking_handshake_role::CLIENT,
            record->id);
        if (result != FT_ERR_SUCCESS)
        {
            delete record;
            this->unlock_transport();
            return (result);
        }
        record->handshake_attempts = 1U;
    }
    else
    {
        record->state = networking_message_connection_state::CONNECTED;
    }
    if (record->secure_enabled != FT_FALSE
        && record->handshake_enabled == FT_FALSE
        && record->secure_channel.initialize(this->_configuration.encryption_key,
            this->_configuration.encryption_key_length,
            this->_configuration.encryption_initialization_vector, 12U) != FT_ERR_SUCCESS)
    {
        delete record;
        this->unlock_transport();
        return (FT_ERR_INITIALIZATION_FAILED);
    }
    record->last_receive = this->_io->now_milliseconds();
    record->last_send = record->last_receive;
    if (this->_connections.push_back(record) != FT_ERR_SUCCESS)
    {
        delete record;
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    if (connection.initialize() != FT_ERR_SUCCESS)
    {
        this->_connections.pop_back();
        delete record;
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    connection.bind(this, record->id);
    if (record->handshake_enabled != FT_FALSE)
        (void)this->emit_event(networking_message_event_type::CONNECTING,
            record->id, FT_ERR_SUCCESS, ft_nullptr);
    else
        (void)this->emit_event(networking_message_event_type::CONNECTED,
            record->id, FT_ERR_SUCCESS, ft_nullptr);
    if (record->handshake_enabled != FT_FALSE)
    {
        result = this->send_handshake_hello(*record);
        if (result != FT_ERR_SUCCESS)
        {
            this->_connections.pop_back();
            delete record;
            (void)connection.destroy();
            this->unlock_transport();
            return (result);
        }
    }
    this->wake_worker();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::send_handshake_hello(
    connection_record &connection) noexcept
{
    ft_vector<uint8_t> hello;
    ft_vector<uint8_t> packet;
    int32_t result;
    const uint8_t *hello_data;

    result = hello.initialize();
    if (result == FT_ERR_SUCCESS)
        result = packet.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        (void)hello.destroy();
        (void)packet.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    result = connection.handshake.get_local_hello(hello);
    hello_data = ft_nullptr;
    if (hello.size() != 0U)
        hello_data = &hello[0];
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_handshake(NETWORKING_MESSAGE_HANDSHAKE_HELLO,
            connection.id, hello_data, hello.size(), packet,
            this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_INTERNAL;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size())
            != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
    }
    (void)hello.destroy();
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_handshake_retry(
    const networking_message_endpoint &remote, uint64_t connection_id,
    const uint8_t cookie[40]) noexcept
{
    ft_vector<uint8_t> packet;
    int32_t result;

    if (cookie == ft_nullptr || remote.length == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = packet.initialize();
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_handshake(
            NETWORKING_MESSAGE_HANDSHAKE_RETRY, connection_id, cookie, 40U,
            packet, this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_INTERNAL;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, remote, &packet[0],
            packet.size())
            != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_handshake_finished(
    connection_record &connection) noexcept
{
    ft_vector<uint8_t> packet;
    uint8_t finished[32];
    int32_t result;

    result = packet.initialize();
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    result = connection.handshake.create_finished(finished);
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_handshake(
            NETWORKING_MESSAGE_HANDSHAKE_FINISHED, connection.id, finished,
            sizeof(finished), packet, this->_configuration.maximum_datagram_size)
            == FT_FALSE)
        result = FT_ERR_INTERNAL;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size())
            != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.finished_sent = FT_TRUE;
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
    }
    (void)packet.destroy();
    networking_message_wipe(finished, sizeof(finished));
    return (result);
}

static ft_bool networking_message_encode_close(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    networking_message_close_reason reason, const char *debug_text,
    ft_vector<uint8_t> &packet, uint32_t maximum_size) noexcept;

static ft_bool networking_message_encode_path(uint8_t frame_type,
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    const uint8_t token[NETWORKING_MESSAGE_PATH_TOKEN_SIZE],
    ft_vector<uint8_t> &packet, uint32_t maximum_size) noexcept;

static ft_bool networking_message_encode_flow_control(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t receive_credit, ft_vector<uint8_t> &packet,
    uint32_t maximum_size) noexcept;

static ft_bool networking_message_encode_key_update(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t next_epoch, ft_vector<uint8_t> &packet,
    uint32_t maximum_size) noexcept;

static ft_bool networking_message_encode_key_update_ack(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t acknowledged_epoch, ft_vector<uint8_t> &packet,
    uint32_t maximum_size) noexcept;

static int32_t networking_message_protect_packet(
    networking_secure_channel &secure_channel, ft_vector<uint8_t> &packet) noexcept;

int32_t networking_message_transport::send_key_update_request(
    connection_record &connection, uint64_t next_epoch) noexcept
{
    ft_vector<uint8_t> packet;
    uint64_t key_epoch;
    int32_t result;

    key_epoch = connection.secure_channel.get_send_key_epoch();
    result = packet.initialize();
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_key_update(connection.id,
            connection.next_packet, key_epoch, next_epoch, packet,
            this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_OUT_OF_RANGE;
    if (result == FT_ERR_SUCCESS
        && networking_message_protect_packet(connection.secure_channel, packet)
            != FT_ERR_SUCCESS)
        result = FT_ERR_PERMISSION_DENIED;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size()) != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
        connection.next_packet += 1U;
    }
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_key_update_ack(
    connection_record &connection, uint64_t acknowledged_epoch) noexcept
{
    ft_vector<uint8_t> packet;
    uint64_t key_epoch;
    int32_t result;

    key_epoch = connection.secure_channel.get_send_key_epoch();
    result = packet.initialize();
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_key_update_ack(connection.id,
            connection.next_packet, key_epoch, acknowledged_epoch, packet,
            this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_OUT_OF_RANGE;
    if (result == FT_ERR_SUCCESS
        && networking_message_protect_packet(connection.secure_channel, packet)
            != FT_ERR_SUCCESS)
        result = FT_ERR_PERMISSION_DENIED;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size()) != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.next_packet += 1U;
    }
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_close(
    connection_record &connection, networking_message_close_reason reason,
    const char *debug_text) noexcept
{
    ft_vector<uint8_t> packet;
    int32_t result;
    uint64_t key_epoch;

    key_epoch = 0U;
    if (connection.secure_enabled != FT_FALSE)
        key_epoch = connection.secure_channel.get_send_key_epoch();
    result = packet.initialize();
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_close(connection.id, connection.next_packet,
            key_epoch, reason, debug_text, packet,
            this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_OUT_OF_RANGE;
    if (result == FT_ERR_SUCCESS && connection.secure_enabled != FT_FALSE
        && networking_message_protect_packet(connection.secure_channel, packet)
            != FT_ERR_SUCCESS)
        result = FT_ERR_PERMISSION_DENIED;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size())
            != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
        connection.next_packet += 1U;
    }
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_path_packet(
    connection_record &connection, uint8_t frame_type,
    const networking_message_endpoint &destination,
    const uint8_t token[NETWORKING_MESSAGE_PATH_TOKEN_SIZE]) noexcept
{
    ft_vector<uint8_t> packet;
    int32_t result;
    uint64_t key_epoch;

    key_epoch = 0U;
    if (connection.secure_enabled != FT_FALSE)
        key_epoch = connection.secure_channel.get_send_key_epoch();
    result = packet.initialize();
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_path(frame_type, connection.id,
            connection.next_packet, key_epoch, token, packet,
            this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_OUT_OF_RANGE;
    if (result == FT_ERR_SUCCESS && connection.secure_enabled != FT_FALSE
        && networking_message_protect_packet(connection.secure_channel, packet)
            != FT_ERR_SUCCESS)
        result = FT_ERR_PERMISSION_DENIED;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, destination, &packet[0],
            packet.size())
            != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
        connection.next_packet += 1U;
    }
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_flow_control(
    connection_record &connection, uint64_t receive_credit) noexcept
{
    ft_vector<uint8_t> packet;
    int32_t result;
    uint64_t key_epoch;

    key_epoch = 0U;
    if (connection.secure_enabled != FT_FALSE)
        key_epoch = connection.secure_channel.get_send_key_epoch();
    result = packet.initialize();
    if (result == FT_ERR_SUCCESS
        && networking_message_encode_flow_control(connection.id,
            connection.next_packet, key_epoch, receive_credit, packet,
            this->_configuration.maximum_datagram_size) == FT_FALSE)
        result = FT_ERR_OUT_OF_RANGE;
    if (result == FT_ERR_SUCCESS && connection.secure_enabled != FT_FALSE
        && networking_message_protect_packet(connection.secure_channel, packet)
            != FT_ERR_SUCCESS)
        result = FT_ERR_PERMISSION_DENIED;
    if (result == FT_ERR_SUCCESS
        && networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size())
            != FT_ERR_SUCCESS)
        result = FT_ERR_SOCKET_SEND_FAILED;
    if (result == FT_ERR_SUCCESS)
    {
        connection.last_send = this->_io->now_milliseconds();
        connection.statistics.last_send_milliseconds = connection.last_send;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
        connection.next_packet += 1U;
        connection.advertised_flow_credit = receive_credit;
    }
    (void)packet.destroy();
    return (result);
}

int32_t networking_message_transport::send_connection_message(uint64_t connection_id,
    const void *data, ft_size_t size, const networking_message_send_options &options) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    networking_message_command *command;
    uint32_t payload_capacity;
    uint32_t fragment_count;
    uint32_t offset;
    uint64_t now;
    uint64_t message_sequence;
    uint32_t lane_index;
    ft_size_t inserted_count;

    if (data == ft_nullptr && size != 0U)
        return (FT_ERR_INVALID_POINTER);
    if (this->should_queue_commands() != FT_FALSE)
    {
        if (size > this->_configuration.maximum_message_size)
            return (FT_ERR_OUT_OF_RANGE);
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::SEND_MESSAGE;
        command->connection_id = connection_id;
        command->send_options = options;
        command->payload.resize(size);
        if (command->payload.get_error() != FT_ERR_SUCCESS)
        {
            command->destroy();
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        if (size != 0U)
            ft_memcpy(&command->payload[0], data, size);
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr || connection->state != networking_message_connection_state::CONNECTED)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->key_update_pending != FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    if (options.lane >= 4U)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (options.delivery != networking_message_delivery::RELIABLE_ORDERED
        && options.delivery != networking_message_delivery::UNRELIABLE
        && options.delivery != networking_message_delivery::UNRELIABLE_SEQUENCED)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (options.delivery == networking_message_delivery::RELIABLE_ORDERED
        && (size > connection->remote_flow_credit
            || connection->reliable_flow_reserved
                > connection->remote_flow_credit - size))
    {
        this->unlock_transport();
        return (FT_ERR_FULL);
    }
    if (size > this->_configuration.maximum_message_size)
    {
        this->unlock_transport();
        return (FT_ERR_OUT_OF_RANGE);
    }
    payload_capacity = this->_configuration.maximum_datagram_size
        - NETWORKING_MESSAGE_HEADER_SIZE - NETWORKING_MESSAGE_FRAME_HEADER_SIZE;
    if (payload_capacity == 0U)
    {
        this->unlock_transport();
        return (FT_ERR_CONFIGURATION);
    }
    fragment_count = static_cast<uint32_t>((size + payload_capacity - 1U) / payload_capacity);
    if (fragment_count == 0U)
        fragment_count = 1U;
    if (fragment_count > 255U)
    {
        this->unlock_transport();
        return (FT_ERR_OUT_OF_RANGE);
    }
    now = this->_io->now_milliseconds();
    networking_message_transport::connection_record::channel_state *channel_state;
    channel_state = networking_message_find_channel_state(*connection, options.channel);
    if (channel_state == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
        message_sequence = channel_state->next_reliable_sequence++;
    else
        message_sequence = ++channel_state->latest_unreliable_sequence;
    lane_index = static_cast<uint32_t>(options.lane) % 4U;
    inserted_count = 0U;
    offset = 0U;
    while (offset < size || (size == 0U && offset == 0U))
    {
        outgoing_frame *frame;
        uint32_t chunk_size;

        if (NETWORKING_TEST_SHOULD_FAIL(
                NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE) != FT_FALSE)
            frame = ft_nullptr;
        else
            frame = new (std::nothrow) outgoing_frame();
        if (frame == ft_nullptr)
        {
            while (inserted_count != 0U)
            {
                outgoing_frame *rollback_frame = connection->pending_lanes[lane_index].pop_back();
                delete rollback_frame;
                inserted_count -= 1U;
            }
            if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
                channel_state->next_reliable_sequence -= 1U;
            else
                channel_state->latest_unreliable_sequence -= 1U;
            this->unlock_transport();
            return (FT_ERR_NO_MEMORY);
        }
        if (frame->initialize() != FT_ERR_SUCCESS)
        {
            delete frame;
            while (inserted_count != 0U)
            {
                outgoing_frame *rollback_frame = connection->pending_lanes[lane_index].pop_back();
                delete rollback_frame;
                inserted_count -= 1U;
            }
            if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
                channel_state->next_reliable_sequence -= 1U;
            else
                channel_state->latest_unreliable_sequence -= 1U;
            this->unlock_transport();
            return (FT_ERR_NO_MEMORY);
        }
        chunk_size = static_cast<uint32_t>(std::min<ft_size_t>(payload_capacity, size - offset));
        frame->delivery = options.delivery;
        frame->lane = options.lane;
        frame->channel = options.channel;
        frame->message_id = connection->next_message;
        frame->sequence = message_sequence;
        frame->expiry = 0U;
        if (options.expiry_milliseconds != 0U)
            frame->expiry = now + options.expiry_milliseconds;
        frame->total_size = static_cast<uint32_t>(size);
        frame->offset = offset;
        frame->fragment_count = static_cast<uint8_t>(fragment_count);
        frame->payload.resize(chunk_size);
        if (frame->payload.get_error() != FT_ERR_SUCCESS)
        {
            delete frame;
            while (inserted_count != 0U)
            {
                outgoing_frame *rollback_frame = connection->pending_lanes[lane_index].pop_back();
                delete rollback_frame;
                inserted_count -= 1U;
            }
            if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
                channel_state->next_reliable_sequence -= 1U;
            else
                channel_state->latest_unreliable_sequence -= 1U;
            this->unlock_transport();
            return (FT_ERR_NO_MEMORY);
        }
        if (chunk_size != 0U)
            ft_memcpy(&frame->payload[0], static_cast<const uint8_t *>(data) + offset, chunk_size);
        connection->pending_lanes[lane_index].push_back(frame);
        if (connection->pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
        {
            delete frame;
            while (inserted_count != 0U)
            {
                outgoing_frame *rollback_frame = connection->pending_lanes[lane_index].pop_back();
                delete rollback_frame;
                inserted_count -= 1U;
            }
            if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
                channel_state->next_reliable_sequence -= 1U;
            else
                channel_state->latest_unreliable_sequence -= 1U;
            this->unlock_transport();
            return (FT_ERR_NO_MEMORY);
        }
        inserted_count += 1U;
        offset += chunk_size;
        if (size == 0U)
            offset = 1U;
    }
    connection->next_message += 1U;
    connection->statistics.messages_sent += 1U;
    connection->statistics.queue_bytes += size;
    connection->statistics.lane_messages_sent[lane_index] += 1U;
    connection->statistics.lane_bytes_sent[lane_index] += size;
    connection->statistics.lane_queued_bytes[lane_index] += size;
    if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
        connection->reliable_flow_reserved += size;
    if (connection->statistics.queue_bytes > connection->maximum_queued_bytes)
    {
        while (inserted_count != 0U)
        {
            outgoing_frame *rollback_frame = connection->pending_lanes[lane_index].pop_back();
            delete rollback_frame;
            inserted_count -= 1U;
        }
        connection->statistics.queue_bytes -= size;
        connection->statistics.lane_queued_bytes[lane_index] -=
            std::min<uint64_t>(connection->statistics.lane_queued_bytes[lane_index],
                size);
        connection->statistics.lane_messages_sent[lane_index] -= 1U;
        connection->statistics.lane_bytes_sent[lane_index] -=
            std::min<uint64_t>(connection->statistics.lane_bytes_sent[lane_index],
                size);
        if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
            connection->reliable_flow_reserved -= size;
        connection->statistics.messages_sent -= 1U;
        if (options.delivery == networking_message_delivery::RELIABLE_ORDERED)
            channel_state->next_reliable_sequence -= 1U;
        else
            channel_state->latest_unreliable_sequence -= 1U;
        connection->next_message -= 1U;
        this->unlock_transport();
        return (FT_ERR_FULL);
    }
    this->wake_worker();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::close_connection(uint64_t connection_id,
    networking_message_close_reason reason, const char *debug_text,
    ft_bool abort_connection) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    uint32_t lane;
    ft_size_t index;
    int32_t send_result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::CLOSE_CONNECTION;
        command->connection_id = connection_id;
        command->close_reason = reason;
        command->abort_connection = abort_connection;
        if (debug_text != ft_nullptr)
        {
            index = 0U;
            while (index + 1U < sizeof(command->debug_text)
                && debug_text[index] != '\0')
            {
                command->debug_text[index] = debug_text[index];
                index += 1U;
            }
            command->debug_text[index] = '\0';
        }
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->state == networking_message_connection_state::CLOSED)
    {
        this->unlock_transport();
        return (FT_ERR_SUCCESS);
    }
    if (connection->state == networking_message_connection_state::DRAINING
        && abort_connection == FT_FALSE)
    {
        this->unlock_transport();
        return (FT_ERR_SUCCESS);
    }
    if (abort_connection == FT_FALSE)
    {
        connection->draining_started = this->_io->now_milliseconds();
        connection->draining_reason = reason;
        ft_memset(connection->draining_debug_text, 0,
            sizeof(connection->draining_debug_text));
        if (debug_text != ft_nullptr)
        {
            index = 0U;
            while (index + 1U < sizeof(connection->draining_debug_text)
                && debug_text[index] != '\0')
            {
                connection->draining_debug_text[index] = debug_text[index];
                index += 1U;
            }
            connection->draining_debug_text[index] = '\0';
        }
        connection->state = networking_message_connection_state::DRAINING;
        if (connection->sent.empty()
            && connection->pending_lanes[0].empty()
            && connection->pending_lanes[1].empty()
            && connection->pending_lanes[2].empty()
            && connection->pending_lanes[3].empty())
        {
            send_result = this->send_close(*connection,
                connection->draining_reason, connection->draining_debug_text);
            connection->state = networking_message_connection_state::CLOSED;
            (void)this->emit_event(networking_message_event_type::CLOSED,
                connection->id, static_cast<int32_t>(reason), ft_nullptr);
            this->wake_worker();
            if (send_result != FT_ERR_SUCCESS
                && send_result != FT_ERR_SOCKET_SEND_FAILED)
            {
                this->unlock_transport();
                return (send_result);
            }
        }
        else
            this->wake_worker();
        this->unlock_transport();
        return (FT_ERR_SUCCESS);
    }
    send_result = this->send_close(*connection, reason, debug_text);
    connection->state = networking_message_connection_state::CLOSED;
    (void)this->emit_event(networking_message_event_type::CLOSED,
        connection->id, FT_ERR_SUCCESS, ft_nullptr);
    lane = 0U;
    while (lane < 4U)
    {
        while (!connection->pending_lanes[lane].empty())
            delete connection->pending_lanes[lane].pop_front();
        lane += 1U;
    }
    index = 0U;
    while (index < connection->sent.size())
    {
        delete connection->sent[index]->frame;
        delete connection->sent[index];
        index += 1U;
    }
    connection->sent.clear();
    connection->bytes_in_flight = 0U;
    connection->statistics.bytes_in_flight = 0U;
    connection->statistics.queue_bytes = 0U;
    connection->reliable_flow_reserved = 0U;
    index = 0U;
    while (index < connection->reassembly.size())
    {
        delete connection->reassembly[index];
        index += 1U;
    }
    connection->reassembly.clear();
    index = 0U;
    while (index < connection->ordered_pending.size())
    {
        delete connection->ordered_pending[index];
        index += 1U;
    }
    connection->ordered_pending.clear();
    connection->statistics.reassembly_bytes = 0U;
    connection->statistics.reassembly_messages = 0U;
    if (abort_connection != FT_FALSE)
        connection->state = networking_message_connection_state::CLOSED;
    this->wake_worker();
    if (send_result != FT_ERR_SUCCESS && send_result != FT_ERR_SOCKET_SEND_FAILED)
    {
        this->unlock_transport();
        return (send_result);
    }
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::update_connection_key_epoch(
    uint64_t connection_id, uint64_t next_epoch) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::UPDATE_KEY_EPOCH;
        command->connection_id = connection_id;
        command->key_epoch = next_epoch;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->secure_enabled == FT_FALSE
        || connection->state != networking_message_connection_state::CONNECTED)
    {
        this->unlock_transport();
        return (FT_ERR_UNSUPPORTED_TYPE);
    }
    if (!connection->sent.empty()
        || !connection->pending_lanes[0].empty()
        || !connection->pending_lanes[1].empty()
        || !connection->pending_lanes[2].empty()
        || !connection->pending_lanes[3].empty())
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    lock_result = connection->secure_channel.update_key_epoch(next_epoch);
    this->unlock_transport();
    return (lock_result);
}

int32_t networking_message_transport::request_connection_key_update(
    uint64_t connection_id, uint64_t next_epoch) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    uint64_t current_epoch;
    int32_t result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::REQUEST_KEY_UPDATE;
        command->connection_id = connection_id;
        command->key_epoch = next_epoch;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->secure_enabled == FT_FALSE
        || connection->state != networking_message_connection_state::CONNECTED)
    {
        this->unlock_transport();
        return (FT_ERR_UNSUPPORTED_TYPE);
    }
    if (!connection->sent.empty()
        || connection->key_update_pending != FT_FALSE
        || !connection->pending_lanes[0].empty()
        || !connection->pending_lanes[1].empty()
        || !connection->pending_lanes[2].empty()
        || !connection->pending_lanes[3].empty())
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    current_epoch = connection->secure_channel.get_send_key_epoch();
    if (next_epoch <= current_epoch)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    result = this->send_key_update_request(*connection, next_epoch);
    if (result == FT_ERR_SUCCESS)
    {
        connection->key_update_pending = FT_TRUE;
        connection->key_update_epoch = next_epoch;
        connection->key_update_sent_at = this->_io->now_milliseconds();
    }
    this->unlock_transport();
    return (result);
}

int32_t networking_message_transport::configure_connection_lane(
    uint64_t connection_id, uint8_t lane, uint32_t priority_weight,
    uint32_t reserved_bandwidth_bytes_per_second) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::CONFIGURE_LANE;
        command->connection_id = connection_id;
        command->lane = lane;
        command->priority_weight = priority_weight;
        command->reserved_bandwidth_bytes_per_second =
            reserved_bandwidth_bytes_per_second;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (lane >= 4U || priority_weight == 0U)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    connection->lanes[lane].priority_weight = priority_weight;
    connection->lanes[lane].reserved_bandwidth_bytes_per_second =
        reserved_bandwidth_bytes_per_second;
    connection->statistics.lane_priority_weight[lane] = priority_weight;
    connection->statistics.lane_reserved_bandwidth_bytes_per_second[lane] =
        reserved_bandwidth_bytes_per_second;
    this->wake_worker();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::set_connection_queue_limits(
    uint64_t connection_id, uint32_t maximum_queued_bytes) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::SET_QUEUE_LIMITS;
        command->connection_id = connection_id;
        command->maximum_queued_bytes = maximum_queued_bytes;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (maximum_queued_bytes == 0U
        || connection->statistics.queue_bytes > maximum_queued_bytes)
    {
        this->unlock_transport();
        return (FT_ERR_FULL);
    }
    connection->maximum_queued_bytes = maximum_queued_bytes;
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::flush_connection(
    uint64_t connection_id) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    connection_record *connection;
    uint32_t lane;
    int32_t result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::FLUSH_CONNECTION;
        command->connection_id = connection_id;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->state != networking_message_connection_state::CONNECTED)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    lane = 0U;
    while (lane < 4U)
    {
        while (!connection->pending_lanes[lane].empty())
        {
            ft_size_t pending_before;
            ft_size_t pending_after;

            pending_before = connection->pending_lanes[lane].size();
            result = this->advance_connection(*connection,
                this->_io->now_milliseconds());
            if (result != FT_ERR_SUCCESS)
            {
                this->unlock_transport();
                return (result);
            }
            pending_after = connection->pending_lanes[lane].size();
            if (pending_after >= pending_before)
                break ;
        }
        lane += 1U;
    }
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::get_connection_state(uint64_t connection_id,
    networking_message_connection_state &state) const noexcept
{
    int32_t lock_result;
    connection_record *connection;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    state = connection->state;
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::get_connection_statistics(uint64_t connection_id,
    networking_message_statistics &statistics) const noexcept
{
    int32_t lock_result;
    connection_record *connection;
    uint32_t lane;
    uint64_t queue_depth;
    uint64_t now_milliseconds;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    statistics = connection->statistics;
    statistics.bytes_in_flight = connection->bytes_in_flight;
    statistics.congestion_window_bytes = connection->congestion_window;
    queue_depth = connection->sent.size();
    lane = 0U;
    while (lane < 4U)
    {
        queue_depth += connection->pending_lanes[lane].size();
        lane += 1U;
    }
    statistics.queue_depth = queue_depth;
    statistics.last_receive_milliseconds = connection->last_receive;
    statistics.last_send_milliseconds = connection->last_send;
    now_milliseconds = this->_io->now_milliseconds();
    lane = 0U;
    while (lane < 4U)
    {
        statistics.lane_priority_weight[lane] =
            connection->lanes[lane].priority_weight;
        statistics.lane_reserved_bandwidth_bytes_per_second[lane] =
            connection->lanes[lane].reserved_bandwidth_bytes_per_second;
        statistics.lane_sent_bytes_window[lane] =
            connection->lanes[lane].sent_bytes_window;
        if (connection->lanes[lane].window_started != 0U
            && now_milliseconds >= connection->lanes[lane].window_started
            && now_milliseconds - connection->lanes[lane].window_started != 0U)
            statistics.lane_rate_bytes_per_second[lane] =
                (connection->lanes[lane].sent_bytes_window * 1000U)
                / (now_milliseconds - connection->lanes[lane].window_started);
        lane += 1U;
    }
    if (connection->smoothed_rtt != 0U)
        statistics.pacing_rate_bytes_per_second =
            (statistics.congestion_window_bytes * 1000U)
            / connection->smoothed_rtt;
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::get_connection_identity(
    uint64_t connection_id, networking_message_peer_identity &identity) const noexcept
{
    int32_t lock_result;
    connection_record *connection;
    uint8_t public_key[32];
    int32_t result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection->handshake_enabled == FT_FALSE
        || connection->handshake.get_state() != networking_handshake_state::FINISHED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_FOUND);
    }
    result = connection->handshake.get_peer_public_key(public_key);
    if (result == FT_ERR_SUCCESS)
    {
        ft_memcpy(identity.public_key, public_key, sizeof(public_key));
        identity.authenticated = FT_TRUE;
    }
    networking_message_wipe(public_key, sizeof(public_key));
    this->unlock_transport();
    return (result);
}

static ft_bool networking_message_read_ack_ranges(const uint8_t *data,
    ft_size_t size, ft_size_t &offset, uint32_t &range_count,
    uint16_t starts[NETWORKING_MESSAGE_ACK_RANGE_LIMIT],
    uint16_t ends[NETWORKING_MESSAGE_ACK_RANGE_LIMIT]) noexcept
{
    uint32_t index;

    if (data == ft_nullptr || offset >= size)
        return (FT_FALSE);
    range_count = data[offset++];
    if (range_count > NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
        return (FT_FALSE);
    index = 0U;
    while (index < NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
    {
        if (!read_u16(data, size, offset, starts[index])
            || !read_u16(data, size, offset, ends[index])
            || starts[index] < ends[index])
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

    static ft_bool networking_message_encode_frame(const outgoing_frame &frame,
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t largest_received,
    const networking_ack_range *received_ranges, uint32_t received_range_count,
    ft_vector<uint8_t> &packet, uint32_t maximum_size) noexcept
{
    if (frame.payload.size() > 65535U || frame.total_size > 0xffffffffU)
        return (FT_FALSE);
    packet.clear();
    packet.reserve(maximum_size);
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(NETWORKING_MESSAGE_FRAME);
    packet.push_back(static_cast<uint8_t>(frame.delivery));
    write_u64(packet, connection_id);
    write_u64(packet, frame.message_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    write_u64(packet, largest_received);
    if (received_range_count > NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
        return (FT_FALSE);
    packet.push_back(static_cast<uint8_t>(received_range_count));
    uint32_t range_index = 0U;
    while (range_index < NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
    {
        uint16_t start_delta = 0U;
        uint16_t end_delta = 0U;

        if (range_index < received_range_count
            && received_ranges != ft_nullptr
            && received_ranges[range_index].start <= largest_received
            && largest_received - received_ranges[range_index].start <= 65535U
            && received_ranges[range_index].end <= largest_received
            && largest_received - received_ranges[range_index].end <= 65535U)
        {
            start_delta = static_cast<uint16_t>(largest_received
                - received_ranges[range_index].start);
            end_delta = static_cast<uint16_t>(largest_received
                - received_ranges[range_index].end);
        }
        write_u16(packet, start_delta);
        write_u16(packet, end_delta);
        range_index += 1U;
    }
    write_u32(packet, frame.channel);
    packet.push_back(frame.lane);
    packet.push_back(frame.fragment_count);
    write_u32(packet, frame.total_size);
    write_u32(packet, frame.offset);
    write_u64(packet, frame.sequence);
    write_u16(packet, static_cast<uint16_t>(frame.payload.size()));
    ft_size_t payload_index = 0U;
    while (payload_index < frame.payload.size())
    {
        packet.push_back(frame.payload[payload_index]);
        payload_index += 1U;
    }
    if (packet.size() <= maximum_size)
        return (FT_TRUE);
    return (FT_FALSE);
}

static void networking_message_encode_ack(uint64_t connection_id, uint64_t packet_number,
    uint64_t key_epoch, uint64_t largest_received,
    const networking_ack_range *received_ranges, uint32_t received_range_count,
    ft_vector<uint8_t> &packet) noexcept
{
    packet.clear();
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(NETWORKING_MESSAGE_ACK);
    packet.push_back(0U);
    write_u64(packet, connection_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    write_u64(packet, largest_received);
    packet.push_back(static_cast<uint8_t>(received_range_count));
    uint32_t range_index = 0U;
    while (range_index < NETWORKING_MESSAGE_ACK_RANGE_LIMIT)
    {
        uint16_t start_delta = 0U;
        uint16_t end_delta = 0U;

        if (range_index < received_range_count
            && received_ranges != ft_nullptr
            && received_ranges[range_index].start <= largest_received
            && largest_received - received_ranges[range_index].start <= 65535U
            && received_ranges[range_index].end <= largest_received
            && largest_received - received_ranges[range_index].end <= 65535U)
        {
            start_delta = static_cast<uint16_t>(largest_received
                - received_ranges[range_index].start);
            end_delta = static_cast<uint16_t>(largest_received
                - received_ranges[range_index].end);
        }
        write_u16(packet, start_delta);
        write_u16(packet, end_delta);
        range_index += 1U;
    }
    return ;
}

static ft_bool networking_message_encode_path(uint8_t frame_type,
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    const uint8_t token[NETWORKING_MESSAGE_PATH_TOKEN_SIZE],
    ft_vector<uint8_t> &packet, uint32_t maximum_size) noexcept
{
    ft_size_t index;

    if ((frame_type != NETWORKING_MESSAGE_PATH_CHALLENGE
            && frame_type != NETWORKING_MESSAGE_PATH_RESPONSE)
        || token == ft_nullptr || maximum_size < 36U)
        return (FT_FALSE);
    packet.clear();
    packet.reserve(36U);
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(frame_type);
    packet.push_back(0U);
    write_u64(packet, connection_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    index = 0U;
    while (index < NETWORKING_MESSAGE_PATH_TOKEN_SIZE)
    {
        packet.push_back(token[index]);
        index += 1U;
    }
    if (packet.get_error() != FT_ERR_SUCCESS || packet.size() > maximum_size)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool networking_message_encode_flow_control(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t receive_credit, ft_vector<uint8_t> &packet,
    uint32_t maximum_size) noexcept
{
    if (maximum_size < 36U)
        return (FT_FALSE);
    packet.clear();
    packet.reserve(36U);
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(NETWORKING_MESSAGE_FLOW_CONTROL);
    packet.push_back(0U);
    write_u64(packet, connection_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    write_u64(packet, receive_credit);
    if (packet.get_error() != FT_ERR_SUCCESS || packet.size() > maximum_size)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool networking_message_encode_key_update(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t next_epoch, ft_vector<uint8_t> &packet,
    uint32_t maximum_size) noexcept
{
    if (maximum_size < 36U || next_epoch <= key_epoch)
        return (FT_FALSE);
    packet.clear();
    packet.reserve(36U);
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(NETWORKING_MESSAGE_KEY_UPDATE);
    packet.push_back(0U);
    write_u64(packet, connection_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    write_u64(packet, next_epoch);
    if (packet.get_error() != FT_ERR_SUCCESS || packet.size() > maximum_size)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool networking_message_encode_key_update_ack(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    uint64_t acknowledged_epoch, ft_vector<uint8_t> &packet,
    uint32_t maximum_size) noexcept
{
    if (maximum_size < 36U || acknowledged_epoch == 0U)
        return (FT_FALSE);
    packet.clear();
    packet.reserve(36U);
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(NETWORKING_MESSAGE_KEY_UPDATE_ACK);
    packet.push_back(0U);
    write_u64(packet, connection_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    write_u64(packet, acknowledged_epoch);
    if (packet.get_error() != FT_ERR_SUCCESS || packet.size() > maximum_size)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool networking_message_encode_close(
    uint64_t connection_id, uint64_t packet_number, uint64_t key_epoch,
    networking_message_close_reason reason, const char *debug_text,
    ft_vector<uint8_t> &packet, uint32_t maximum_size) noexcept
{
    ft_size_t debug_length;

    debug_length = 0U;
    if (debug_text != ft_nullptr)
    {
        while (debug_length < 95U && debug_text[debug_length] != '\0')
            debug_length += 1U;
    }
    packet.clear();
    packet.reserve(31U + debug_length);
    packet.push_back(NETWORKING_MESSAGE_MAGIC);
    packet.push_back(NETWORKING_MESSAGE_VERSION);
    packet.push_back(NETWORKING_MESSAGE_CLOSE);
    packet.push_back(0U);
    write_u64(packet, connection_id);
    write_u64(packet, packet_number);
    write_u64(packet, key_epoch);
    write_u16(packet, static_cast<uint16_t>(reason));
    packet.push_back(static_cast<uint8_t>(debug_length));
    if (debug_length != 0U)
    {
        ft_size_t index = 0U;
        while (index < debug_length)
        {
            packet.push_back(static_cast<uint8_t>(debug_text[index]));
            index += 1U;
        }
    }
    if (packet.get_error() != FT_ERR_SUCCESS || packet.size() > maximum_size)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t networking_message_protect_packet(networking_secure_channel &secure_channel,
    ft_vector<uint8_t> &packet) noexcept
{
    ft_size_t associated_data_length;
    ft_size_t packet_number_offset;
    ft_size_t offset;
    uint64_t packet_number;
    ft_vector<uint8_t> ciphertext;
    uint8_t authentication_tag[16];
    ft_vector<uint8_t> protected_packet;

    if (packet.size() < 20U || packet.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_ARGUMENT);
    associated_data_length = 36U;
    packet_number_offset = 20U;
    if (packet[2] == NETWORKING_MESSAGE_ACK
        || packet[2] == NETWORKING_MESSAGE_CLOSE
        || packet[2] == NETWORKING_MESSAGE_PATH_CHALLENGE
        || packet[2] == NETWORKING_MESSAGE_PATH_RESPONSE
        || packet[2] == NETWORKING_MESSAGE_FLOW_CONTROL
        || packet[2] == NETWORKING_MESSAGE_KEY_UPDATE
        || packet[2] == NETWORKING_MESSAGE_KEY_UPDATE_ACK)
    {
        associated_data_length = 28U;
        packet_number_offset = 12U;
    }
    offset = packet_number_offset;
    if (!read_u64(&packet[0], packet.size(), offset, packet_number))
        return (FT_ERR_INVALID_ARGUMENT);
    if (!secure_channel.seal(packet_number, &packet[0], associated_data_length,
        &packet[associated_data_length], packet.size() - associated_data_length,
        ciphertext, authentication_tag))
        return (FT_ERR_INTERNAL);
    (void)protected_packet.initialize();
    offset = 0U;
    while (offset < associated_data_length)
    {
        protected_packet.push_back(packet[offset]);
        offset += 1U;
    }
    offset = 0U;
    while (offset < ciphertext.size())
    {
        protected_packet.push_back(ciphertext[offset]);
        offset += 1U;
    }
    offset = 0U;
    while (offset < sizeof(authentication_tag))
    {
        protected_packet.push_back(authentication_tag[offset]);
        offset += 1U;
    }
    (void)packet.destroy();
    if (packet.move(protected_packet) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::advance_connection(connection_record &connection,
    uint64_t now_milliseconds) noexcept
{
    ft_vector<uint8_t> packet;
    ft_size_t reassembly_index;
    ft_size_t sent_index;
    ft_size_t sent_payload_size;
    uint32_t lane_index;
    outgoing_frame *frame;
    ft_bool reliable_frame;
    sent_packet *prepared_sent;
    uint64_t key_epoch;

    if (connection.previous_receive_key_expiry != 0U
        && now_milliseconds >= connection.previous_receive_key_expiry)
    {
        if (connection.secure_channel.clear_previous_receive_key()
            != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        connection.previous_receive_key_expiry = 0U;
    }

    key_epoch = 0U;
    if (connection.secure_enabled != FT_FALSE)
        key_epoch = connection.secure_channel.get_send_key_epoch();

    if (connection.state == networking_message_connection_state::DRAINING)
    {
        if (connection.sent.empty()
            && connection.pending_lanes[0].empty()
            && connection.pending_lanes[1].empty()
            && connection.pending_lanes[2].empty()
            && connection.pending_lanes[3].empty())
        {
            int32_t close_result = this->send_close(connection,
                connection.draining_reason, connection.draining_debug_text);
            connection.state = networking_message_connection_state::CLOSED;
            (void)this->emit_event(networking_message_event_type::CLOSED,
                connection.id, static_cast<int32_t>(connection.draining_reason),
                ft_nullptr);
            if (close_result != FT_ERR_SUCCESS
                && close_result != FT_ERR_SOCKET_SEND_FAILED)
                return (close_result);
            return (FT_ERR_SUCCESS);
        }
    }

    if (connection.state == networking_message_connection_state::HANDSHAKING)
    {
        uint64_t retry_timeout;
        uint32_t retry_shift;
        int32_t handshake_result;

        if (connection.incoming_pending != FT_FALSE
            && connection.handshake.get_role() == networking_handshake_role::SERVER)
            return (FT_ERR_SUCCESS);

        retry_timeout = this->_configuration.retransmission_timeout_milliseconds;
        if (retry_timeout == 0U)
            retry_timeout = 1U;
        retry_shift = 0U;
        if (connection.handshake_attempts > 0U)
            retry_shift = connection.handshake_attempts - 1U;
        if (retry_shift > 3U)
            retry_shift = 3U;
        retry_timeout <<= retry_shift;
        if (now_milliseconds - connection.last_send < retry_timeout)
            return (FT_ERR_SUCCESS);
        if (connection.handshake_attempts >= 6U)
        {
            connection.state = networking_message_connection_state::FAILED;
            return (FT_ERR_TIMEOUT);
        }
        handshake_result = FT_ERR_SUCCESS;
        if (connection.handshake.get_role() == networking_handshake_role::CLIENT)
            handshake_result = this->send_handshake_hello(connection);
        else
        {
            handshake_result = this->send_handshake_hello(connection);
            if (handshake_result == FT_ERR_SUCCESS)
                handshake_result = this->send_handshake_finished(connection);
        }
        if (handshake_result != FT_ERR_SUCCESS)
        {
            connection.state = networking_message_connection_state::FAILED;
            return (handshake_result);
        }
        connection.handshake_attempts += 1U;
        return (FT_ERR_SUCCESS);
    }

    if (connection.key_update_pending != FT_FALSE
        && now_milliseconds >= connection.key_update_sent_at
        && now_milliseconds - connection.key_update_sent_at
            >= this->_configuration.retransmission_timeout_milliseconds)
    {
        int32_t update_result;

        update_result = this->send_key_update_request(connection,
            connection.key_update_epoch);
        if (update_result != FT_ERR_SUCCESS)
            return (update_result);
        connection.key_update_sent_at = now_milliseconds;
        return (FT_ERR_SUCCESS);
    }

    if (connection.path_validation_pending != FT_FALSE
        && now_milliseconds >= connection.path_challenge_sent
        && now_milliseconds - connection.path_challenge_sent
            >= this->_configuration.retransmission_timeout_milliseconds)
    {
        int32_t path_result = this->send_path_packet(connection,
            NETWORKING_MESSAGE_PATH_CHALLENGE, connection.pending_path,
            connection.pending_path_token);
        if (path_result != FT_ERR_SUCCESS)
            return (path_result);
        connection.path_challenge_sent = now_milliseconds;
    }

    if (connection.receive_flow_credit > connection.advertised_flow_credit)
    {
        int32_t flow_result = this->send_flow_control(connection,
            connection.receive_flow_credit);
        if (flow_result != FT_ERR_SUCCESS)
            return (flow_result);
    }

    if (this->_configuration.idle_timeout_milliseconds != 0U)
    {
        uint64_t last_activity;

        last_activity = connection.last_receive;
        if (connection.last_send > last_activity)
            last_activity = connection.last_send;
        if (now_milliseconds >= last_activity
            && now_milliseconds - last_activity
                >= this->_configuration.idle_timeout_milliseconds)
        {
            (void)this->close_connection(connection.id,
                networking_message_close_reason::TIMEOUT,
                "idle timeout", FT_TRUE);
            return (FT_ERR_TIMEOUT);
        }
    }

    if (packet.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);

    reassembly_index = 0U;
    while (reassembly_index < connection.reassembly.size())
    {
        reassembly_record *record = connection.reassembly[reassembly_index];
        if (record->expires_at <= now_milliseconds)
        {
            connection.statistics.fragments_dropped += 1U;
            (void)this->emit_event(networking_message_event_type::DELIVERY_FAILED,
                connection.id, FT_ERR_TIMEOUT, "reassembly expired");
            connection.statistics.reassembly_bytes -= std::min<uint64_t>(
                connection.statistics.reassembly_bytes,
                record->total_size);
            if (connection.receive_flow_credit <= UINT64_MAX - record->total_size)
                connection.receive_flow_credit += record->total_size;
            else
                connection.receive_flow_credit = UINT64_MAX;
            if (connection.statistics.reassembly_messages != 0U)
                connection.statistics.reassembly_messages -= 1U;
            delete record;
            connection.reassembly.erase(connection.reassembly.begin() + reassembly_index);
            continue ;
        }
        reassembly_index += 1U;
    }

    sent_index = 0U;
    while (sent_index < connection.sent.size())
    {
        ft_size_t candidate_index;
        sent_packet *sent;

        candidate_index = connection.retransmission_cursor + sent_index;
        if (candidate_index >= connection.sent.size())
            candidate_index -= connection.sent.size();
        sent = connection.sent[candidate_index];
        if (now_milliseconds - sent->sent_at >= this->_configuration.retransmission_timeout_milliseconds)
        {
            uint64_t minimum_window;

            if (sent->frame->expiry != 0U && now_milliseconds >= sent->frame->expiry)
            {
                connection.statistics.messages_expired += 1U;
                (void)this->emit_event(
                    networking_message_event_type::DELIVERY_FAILED,
                    connection.id, FT_ERR_TIMEOUT, "message expired");
                connection.statistics.queue_bytes -= std::min<uint64_t>(
                    connection.statistics.queue_bytes,
                    sent->frame->payload.size());
                if (is_reliable(sent->frame->delivery) != FT_FALSE)
                    connection.reliable_flow_reserved -= std::min<uint64_t>(
                        connection.reliable_flow_reserved,
                        sent->frame->payload.size());
                delete sent->frame;
                delete sent;
                connection.sent.erase(connection.sent.begin() + candidate_index);
                if (connection.retransmission_cursor >= connection.sent.size())
                    connection.retransmission_cursor = 0U;
                continue ;
            }
            minimum_window = static_cast<uint64_t>(this->_configuration.maximum_datagram_size) * 2U;
            if (minimum_window == 0U)
                minimum_window = 2U;
            connection.slow_start_threshold = connection.congestion_window / 2U;
            if (connection.slow_start_threshold < minimum_window)
                connection.slow_start_threshold = minimum_window;
            connection.congestion_window = connection.slow_start_threshold;
            if (!networking_message_encode_frame(*sent->frame, connection.id, connection.next_packet,
                key_epoch, connection.largest_received, connection.received_ranges,
                connection.received_range_count, packet,
                this->_configuration.maximum_datagram_size))
                return (FT_ERR_INTERNAL);
            if (connection.secure_enabled != FT_FALSE
                && networking_message_protect_packet(connection.secure_channel, packet) != FT_ERR_SUCCESS)
                return (FT_ERR_INTERNAL);
            if (networking_message_send_datagram(*this->_io, connection.remote,
                    &packet[0], packet.size()) != FT_ERR_SUCCESS)
                return (FT_ERR_SOCKET_SEND_FAILED);
            sent->sent_at = now_milliseconds;
            sent->packet_number = connection.next_packet;
            if (packet.size() > sent->wire_size)
                connection.bytes_in_flight += packet.size() - sent->wire_size;
            else if (sent->wire_size - packet.size() <= connection.bytes_in_flight)
                connection.bytes_in_flight -= sent->wire_size - packet.size();
            else
                connection.bytes_in_flight = 0U;
            sent->wire_size = packet.size();
            sent->retransmitted = FT_TRUE;
            connection.retransmission_cursor = candidate_index + 1U;
            if (connection.retransmission_cursor >= connection.sent.size())
                connection.retransmission_cursor = 0U;
            connection.next_packet += 1U;
            connection.statistics.packets_lost += 1U;
            connection.statistics.packets_retransmitted += 1U;
            connection.statistics.packets_sent += 1U;
            connection.statistics.bytes_sent += packet.size();
            break ;
        }
        sent_index += 1U;
    }
    if (connection.pending_lanes[0].empty() && connection.pending_lanes[1].empty()
        && connection.pending_lanes[2].empty() && connection.pending_lanes[3].empty()
        && connection.largest_received != 0U
        && connection.last_acknowledged_sent != connection.largest_received)
    {
        networking_message_encode_ack(connection.id, connection.next_packet,
            key_epoch, connection.largest_received, connection.received_ranges,
            connection.received_range_count, packet);
        if (connection.secure_enabled != FT_FALSE
            && networking_message_protect_packet(connection.secure_channel, packet) != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        if (networking_message_send_datagram(*this->_io, connection.remote,
                &packet[0], packet.size()) != FT_ERR_SUCCESS)
            return (FT_ERR_SOCKET_SEND_FAILED);
        connection.last_send = now_milliseconds;
        connection.statistics.last_send_milliseconds = now_milliseconds;
        connection.statistics.packets_sent += 1U;
        connection.statistics.bytes_sent += packet.size();
        connection.next_packet += 1U;
        connection.last_acknowledged_sent = connection.largest_received;
    }
    lane_index = networking_message_select_lane(connection, now_milliseconds);
    if (lane_index == 4U)
        return (FT_ERR_SUCCESS);
    frame = connection.pending_lanes[lane_index].pop_front();
    if (frame == ft_nullptr)
        return (FT_ERR_INTERNAL);
    if (frame->expiry != 0U && now_milliseconds >= frame->expiry)
    {
        connection.statistics.messages_expired += 1U;
        if (is_reliable(frame->delivery) != FT_FALSE)
            connection.reliable_flow_reserved -= std::min<uint64_t>(
                connection.reliable_flow_reserved, frame->payload.size());
        delete frame;
        return (FT_ERR_SUCCESS);
    }
    if (!networking_message_encode_frame(*frame, connection.id, connection.next_packet,
        key_epoch, connection.largest_received, connection.received_ranges,
        connection.received_range_count, packet,
        this->_configuration.maximum_datagram_size))
    {
        connection.pending_lanes[lane_index].push_front(frame);
        if (connection.pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
            delete frame;
        return (FT_ERR_INTERNAL);
    }
    if (connection.secure_enabled != FT_FALSE
        && networking_message_protect_packet(connection.secure_channel, packet) != FT_ERR_SUCCESS)
    {
        connection.pending_lanes[lane_index].push_front(frame);
        if (connection.pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
            delete frame;
        return (FT_ERR_INTERNAL);
    }
    if (connection.pacing_next_send > now_milliseconds
        || connection.bytes_in_flight > connection.congestion_window
        || packet.size() > connection.congestion_window - connection.bytes_in_flight)
    {
        connection.pending_lanes[lane_index].push_front(frame);
        if (connection.pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
            delete frame;
        return (FT_ERR_SUCCESS);
    }
    sent_payload_size = frame->payload.size();
    reliable_frame = is_reliable(frame->delivery);
    prepared_sent = ft_nullptr;
    if (reliable_frame != FT_FALSE)
    {
        if (NETWORKING_TEST_SHOULD_FAIL(
                NETWORKING_TEST_SENT_PACKET_ALLOCATE) != FT_FALSE)
            prepared_sent = ft_nullptr;
        else
            prepared_sent = new (std::nothrow) sent_packet();
        if (prepared_sent == ft_nullptr)
        {
            connection.pending_lanes[lane_index].push_front(frame);
            if (connection.pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
                delete frame;
            return (FT_ERR_NO_MEMORY);
        }
        prepared_sent->packet_number = connection.next_packet;
        prepared_sent->sent_at = now_milliseconds;
        prepared_sent->wire_size = packet.size();
        prepared_sent->frame = frame;
        if (connection.sent.push_back(prepared_sent) != FT_ERR_SUCCESS)
        {
            delete prepared_sent;
            connection.pending_lanes[lane_index].push_front(frame);
            if (connection.pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
                delete frame;
            return (FT_ERR_NO_MEMORY);
        }
        connection.bytes_in_flight += prepared_sent->wire_size;
    }
    if (networking_message_send_datagram(*this->_io, connection.remote,
            &packet[0], packet.size()) != FT_ERR_SUCCESS)
    {
        if (prepared_sent != ft_nullptr)
        {
            if (prepared_sent->wire_size <= connection.bytes_in_flight)
                connection.bytes_in_flight -= prepared_sent->wire_size;
            connection.sent.erase(connection.sent.begin()
                + connection.sent.size() - 1U);
            delete prepared_sent;
        }
        connection.pending_lanes[lane_index].push_front(frame);
        if (connection.pending_lanes[lane_index].get_error() != FT_ERR_SUCCESS)
            delete frame;
        return (FT_ERR_SOCKET_SEND_FAILED);
    }
    connection.last_send = now_milliseconds;
    connection.statistics.last_send_milliseconds = now_milliseconds;
    connection.statistics.packets_sent += 1U;
    connection.statistics.bytes_sent += packet.size();
    connection.lanes[lane_index].sent_bytes_window += packet.size();
    connection.statistics.lane_sent_bytes_window[lane_index] =
        connection.lanes[lane_index].sent_bytes_window;
    connection.statistics.lane_priority_weight[lane_index] =
        connection.lanes[lane_index].priority_weight;
    connection.statistics.lane_reserved_bandwidth_bytes_per_second[lane_index]
        = connection.lanes[lane_index].reserved_bandwidth_bytes_per_second;
    if (connection.lanes[lane_index].window_started != 0U
        && now_milliseconds >= connection.lanes[lane_index].window_started
        && now_milliseconds - connection.lanes[lane_index].window_started != 0U)
        connection.statistics.lane_rate_bytes_per_second[lane_index] =
            (connection.lanes[lane_index].sent_bytes_window * 1000U)
            / (now_milliseconds - connection.lanes[lane_index].window_started);
    if (connection.smoothed_rtt != 0U && connection.congestion_window != 0U)
    {
        uint64_t pacing_interval;

        pacing_interval = (packet.size() * connection.smoothed_rtt)
            / connection.congestion_window;
        if (pacing_interval == 0U)
            pacing_interval = 1U;
        connection.pacing_next_send = now_milliseconds + pacing_interval;
    }
    if (reliable_frame == FT_FALSE)
        delete frame;
    if (reliable_frame == FT_FALSE)
        connection.statistics.queue_bytes -= std::min<uint64_t>(
            connection.statistics.queue_bytes, sent_payload_size);
    if (reliable_frame == FT_FALSE)
        connection.statistics.lane_queued_bytes[lane_index] -=
            std::min<uint64_t>(
                connection.statistics.lane_queued_bytes[lane_index],
                sent_payload_size);
    connection.statistics.bytes_in_flight = connection.bytes_in_flight;
    connection.statistics.queue_depth = connection.sent.size();
    connection.next_packet += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::process_handshake_datagram(
    const networking_message_endpoint &source, const uint8_t *data,
    ft_size_t size) noexcept
{
    uint8_t frame_type;
    uint64_t connection_id;
    uint16_t payload_size;
    ft_size_t offset;
    connection_record *connection;
    int32_t result;
    uint8_t send_key[32];
    uint8_t receive_key[32];
    uint8_t send_initialization_vector[12];
    uint8_t receive_initialization_vector[12];
    uint8_t hello_digest[32];
    uint8_t cookie[40];
    uint64_t now_milliseconds;
    ft_bool keys_ready;

    if (data == ft_nullptr || size < 4U + 8U + 2U
        || this->_configuration.enable_authenticated_handshake == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    frame_type = data[2];
    offset = 4U;
    if (!read_u64(data, size, offset, connection_id)
        || !read_u16(data, size, offset, payload_size)
        || payload_size != size - offset)
        return (FT_ERR_INVALID_ARGUMENT);
    connection = this->find_connection(connection_id);
    now_milliseconds = this->_io->now_milliseconds();
    if (frame_type == NETWORKING_MESSAGE_HANDSHAKE_RETRY)
    {
        if (connection == ft_nullptr
            || connection->state != networking_message_connection_state::HANDSHAKING
            || connection->handshake.get_role() != networking_handshake_role::CLIENT
            || payload_size != sizeof(cookie))
            return (FT_ERR_INVALID_STATE);
        result = connection->handshake.set_retry_cookie(data + offset);
        if (result == FT_ERR_SUCCESS)
            result = this->send_handshake_hello(*connection);
        if (result == FT_ERR_SUCCESS)
            connection->handshake_attempts += 1U;
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = this->_io->now_milliseconds();
        connection->statistics.last_receive_milliseconds = now_milliseconds;
        return (result);
    }
    if (frame_type == NETWORKING_MESSAGE_HANDSHAKE_HELLO)
    {
        if (payload_size != 80U
            && payload_size != 80U + sizeof(cookie))
            return (FT_ERR_INVALID_ARGUMENT);
        if (connection == ft_nullptr)
        {
            if (this->_listening == FT_FALSE)
                return (FT_ERR_PERMISSION_DENIED);
            if (this->_configuration.enable_retry_cookies != FT_FALSE)
            {
                if (payload_size == 80U)
                {
                    result = networking_handshake::hash_hello(data + offset,
                        80U, hello_digest);
                    if (result == FT_ERR_SUCCESS)
                        result = networking_handshake::create_retry_cookie(
                            this->_configuration.retry_cookie_secret, source,
                            hello_digest, now_milliseconds, cookie);
                    if (result == FT_ERR_SUCCESS)
                        result = this->send_handshake_retry(source,
                            connection_id, cookie);
                    networking_message_wipe(hello_digest, sizeof(hello_digest));
                    networking_message_wipe(cookie, sizeof(cookie));
                    return (result);
                }
                result = networking_handshake::hash_hello(data + offset, 80U,
                    hello_digest);
                if (result == FT_ERR_SUCCESS)
                    result = networking_handshake::verify_retry_cookie(
                        this->_configuration.retry_cookie_secret, source,
                        hello_digest, now_milliseconds,
                        this->_configuration.retry_cookie_lifetime_milliseconds,
                        data + offset + 80U);
                networking_message_wipe(hello_digest, sizeof(hello_digest));
                if (result != FT_ERR_SUCCESS)
                    return (FT_ERR_PERMISSION_DENIED);
            }
            if (NETWORKING_TEST_SHOULD_FAIL(
                    NETWORKING_TEST_CONNECTION_ALLOCATE) != FT_FALSE)
                return (FT_ERR_NO_MEMORY);
            connection = new (std::nothrow) connection_record();
            if (connection == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            if (connection->initialize() != FT_ERR_SUCCESS)
            {
                delete connection;
                return (FT_ERR_NO_MEMORY);
            }
            connection->id = connection_id;
            connection->maximum_queued_bytes =
                this->_configuration.maximum_queued_bytes;
            connection->remote_flow_credit =
                this->_configuration.maximum_reassembly_bytes;
            connection->receive_flow_credit =
                this->_configuration.maximum_reassembly_bytes;
            connection->remote = source;
            connection->state = networking_message_connection_state::HANDSHAKING;
            connection->secure_enabled = FT_TRUE;
            connection->handshake_enabled = FT_TRUE;
            connection->incoming_pending = FT_TRUE;
            result = connection->handshake.initialize(
                networking_handshake_role::SERVER, this->_next_connection_id++);
            if (result != FT_ERR_SUCCESS)
            {
                delete connection;
                return (result);
            }
            if (this->_connections.push_back(connection) != FT_ERR_SUCCESS)
            {
                delete connection;
                return (FT_ERR_NO_MEMORY);
            }
            (void)this->emit_event(
                networking_message_event_type::CONNECTION_REQUESTED,
                connection->id, FT_ERR_SUCCESS, ft_nullptr);
            connection->last_receive = this->_io->now_milliseconds();
            connection->last_send = connection->last_receive;
        }
        else if (connection->state != networking_message_connection_state::HANDSHAKING
            && connection->state != networking_message_connection_state::CONNECTED)
            return (FT_ERR_INVALID_STATE);
        if (connection->remote.length != source.length
            || ft_memcmp(&connection->remote.address, &source.address,
                source.length) != 0)
            return (FT_ERR_PERMISSION_DENIED);
        result = connection->handshake.accept_peer_hello(data + offset, payload_size);
        keys_ready = FT_FALSE;
        if (result == FT_ERR_SUCCESS
            && connection->handshake.get_state()
                == networking_handshake_state::PEER_HELLO_ACCEPTED)
        {
            if (this->_configuration.enable_peer_key_pinning != FT_FALSE)
            {
                uint8_t peer_public_key[32];

                result = connection->handshake.get_peer_public_key(
                    peer_public_key);
                if (result == FT_ERR_SUCCESS
                    && ft_memcmp(peer_public_key,
                        this->_configuration.pinned_peer_public_key,
                        sizeof(peer_public_key)) != 0)
                    result = FT_ERR_PERMISSION_DENIED;
                networking_message_wipe(peer_public_key,
                    sizeof(peer_public_key));
            }
        }
        if (result == FT_ERR_SUCCESS
            && connection->handshake.get_state()
                == networking_handshake_state::PEER_HELLO_ACCEPTED)
        {
            result = connection->handshake.derive_keys();
            if (result == FT_ERR_SUCCESS)
                keys_ready = FT_TRUE;
        }
        if (result == FT_ERR_SUCCESS && keys_ready != FT_FALSE)
            result = connection->handshake.get_traffic_keys(send_key, receive_key,
                send_initialization_vector, receive_initialization_vector);
        if (result == FT_ERR_SUCCESS && keys_ready != FT_FALSE)
            result = connection->secure_channel.initialize_directional(send_key,
                receive_key, send_initialization_vector,
                receive_initialization_vector);
        if (result == FT_ERR_SUCCESS
            && connection->handshake.get_role() == networking_handshake_role::SERVER
            && connection->incoming_pending == FT_FALSE)
        {
            result = this->send_handshake_hello(*connection);
            if (result == FT_ERR_SUCCESS)
                result = this->send_handshake_finished(*connection);
            if (result == FT_ERR_SUCCESS)
                connection->handshake_attempts = 1U;
        }
        if (result != FT_ERR_SUCCESS)
        {
            connection->state = networking_message_connection_state::FAILED;
            connection->statistics.authentication_failures += 1U;
            (void)this->emit_event(networking_message_event_type::FAILED,
                connection->id, result, "handshake failed");
        }
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = this->_io->now_milliseconds();
        connection->statistics.last_receive_milliseconds = connection->last_receive;
        networking_message_wipe(send_key, sizeof(send_key));
        networking_message_wipe(receive_key, sizeof(receive_key));
        networking_message_wipe(send_initialization_vector,
            sizeof(send_initialization_vector));
        networking_message_wipe(receive_initialization_vector,
            sizeof(receive_initialization_vector));
        return (result);
    }
    if (frame_type != NETWORKING_MESSAGE_HANDSHAKE_FINISHED
        || connection == ft_nullptr
        || (connection->state != networking_message_connection_state::HANDSHAKING
            && connection->state != networking_message_connection_state::CONNECTED)
        || payload_size != 32U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = connection->handshake.verify_finished(data + offset);
    if (result != FT_ERR_SUCCESS)
    {
        connection->statistics.authentication_failures += 1U;
        connection->state = networking_message_connection_state::FAILED;
        (void)this->emit_event(networking_message_event_type::FAILED,
            connection->id, result, "finished authentication failed");
        return (FT_ERR_PERMISSION_DENIED);
    }
    if (connection->handshake.get_role() == networking_handshake_role::CLIENT
        && connection->finished_sent == FT_FALSE)
        result = this->send_handshake_finished(*connection);
    if (result == FT_ERR_SUCCESS)
    {
        if (connection->state != networking_message_connection_state::CONNECTED)
            (void)this->emit_event(networking_message_event_type::CONNECTED,
                connection->id, FT_ERR_SUCCESS, ft_nullptr);
        connection->state = networking_message_connection_state::CONNECTED;
    }
    connection->statistics.packets_received += 1U;
    connection->statistics.bytes_received += size;
    connection->last_receive = this->_io->now_milliseconds();
    connection->statistics.last_receive_milliseconds = connection->last_receive;
    if (result != FT_ERR_SUCCESS)
    {
        connection->state = networking_message_connection_state::FAILED;
        (void)this->emit_event(networking_message_event_type::FAILED,
            connection->id, result, "finished verification failed");
    }
    return (result);
}

int32_t networking_message_transport::process_datagram(const networking_message_endpoint &source,
    const uint8_t *data, ft_size_t size) noexcept
{
    ft_size_t offset;
    uint8_t magic;
    uint8_t version;
    uint8_t frame_type;
    uint8_t delivery_value;
    uint64_t connection_id;
    uint64_t message_id;
    uint64_t packet_number;
    uint64_t key_epoch;
    uint64_t largest_acknowledged;
    uint32_t acknowledged_range_count;
    uint16_t acknowledged_range_starts[NETWORKING_MESSAGE_ACK_RANGE_LIMIT];
    uint16_t acknowledged_range_ends[NETWORKING_MESSAGE_ACK_RANGE_LIMIT];
    uint32_t channel;
    uint8_t lane;
    uint8_t fragment_count;
    uint32_t total_size;
    uint32_t fragment_offset;
    uint64_t sequence;
    uint16_t payload_size;
    uint16_t close_reason;
    uint8_t close_text_length;
    char close_text[96];
    connection_record *connection;
    ft_vector<uint8_t> decrypted_packet;

    channel = 0U;
    offset = 0U;
    if (data == ft_nullptr || size < 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    magic = data[offset++];
    version = data[offset++];
    frame_type = data[offset++];
    delivery_value = data[offset++];
    if (magic != NETWORKING_MESSAGE_MAGIC || version != NETWORKING_MESSAGE_VERSION)
        return (FT_ERR_INVALID_ARGUMENT);
    if (frame_type == NETWORKING_MESSAGE_HANDSHAKE_HELLO
        || frame_type == NETWORKING_MESSAGE_HANDSHAKE_FINISHED
        || frame_type == NETWORKING_MESSAGE_HANDSHAKE_RETRY)
        return (this->process_handshake_datagram(source, data, size));
    if (frame_type != NETWORKING_MESSAGE_FRAME
        && frame_type != NETWORKING_MESSAGE_ACK
        && frame_type != NETWORKING_MESSAGE_CLOSE
        && frame_type != NETWORKING_MESSAGE_PATH_CHALLENGE
        && frame_type != NETWORKING_MESSAGE_PATH_RESPONSE
        && frame_type != NETWORKING_MESSAGE_FLOW_CONTROL
        && frame_type != NETWORKING_MESSAGE_KEY_UPDATE
        && frame_type != NETWORKING_MESSAGE_KEY_UPDATE_ACK)
        return (FT_ERR_INVALID_ARGUMENT);
    if (frame_type == NETWORKING_MESSAGE_PATH_CHALLENGE
        || frame_type == NETWORKING_MESSAGE_PATH_RESPONSE
        || frame_type == NETWORKING_MESSAGE_FLOW_CONTROL)
    {
        if (size < 36U)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    else if (frame_type == NETWORKING_MESSAGE_CLOSE)
    {
        if (size < 31U)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    else if (size < 4U + 8U + 8U + 8U + 8U + 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    offset = 4U;
    if (!read_u64(data, size, offset, connection_id))
        return (FT_ERR_INVALID_ARGUMENT);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr)
    {
        if (frame_type == NETWORKING_MESSAGE_CLOSE
            || frame_type == NETWORKING_MESSAGE_PATH_CHALLENGE
            || frame_type == NETWORKING_MESSAGE_PATH_RESPONSE
            || frame_type == NETWORKING_MESSAGE_FLOW_CONTROL
            || frame_type == NETWORKING_MESSAGE_KEY_UPDATE
            || frame_type == NETWORKING_MESSAGE_KEY_UPDATE_ACK)
            return (FT_ERR_INVALID_HANDLE);
        if (this->_listening == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        if (this->_configuration.enable_authenticated_handshake != FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_CONNECTION_ALLOCATE)
            != FT_FALSE)
            return (FT_ERR_NO_MEMORY);
        connection = new (std::nothrow) connection_record();
        if (connection == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (connection->initialize() != FT_ERR_SUCCESS)
        {
            delete connection;
            return (FT_ERR_NO_MEMORY);
        }
        connection->id = connection_id;
        connection->maximum_queued_bytes = this->_configuration.maximum_queued_bytes;
        connection->remote_flow_credit =
            this->_configuration.maximum_reassembly_bytes;
        connection->receive_flow_credit =
            this->_configuration.maximum_reassembly_bytes;
        connection->remote = source;
        connection->state = networking_message_connection_state::CONNECTED;
        connection->secure_enabled = this->_configuration.enable_encryption;
        if (connection->secure_enabled != FT_FALSE
            && connection->secure_channel.initialize(this->_configuration.encryption_key,
                this->_configuration.encryption_key_length,
                this->_configuration.encryption_initialization_vector, 12U) != FT_ERR_SUCCESS)
        {
            delete connection;
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        connection->last_receive = this->_io->now_milliseconds();
        connection->last_send = connection->last_receive;
        if (this->_connections.push_back(connection) != FT_ERR_SUCCESS)
        {
            delete connection;
            return (FT_ERR_NO_MEMORY);
        }
        (void)this->emit_event(networking_message_event_type::CONNECTED,
            connection->id, FT_ERR_SUCCESS, ft_nullptr);
    }
    if (connection->state != networking_message_connection_state::CONNECTED
        && connection->state != networking_message_connection_state::DRAINING)
        return (FT_ERR_PERMISSION_DENIED);
    if (networking_message_endpoint_equal(connection->remote, source) == FT_FALSE
        && frame_type != NETWORKING_MESSAGE_FRAME
        && frame_type != NETWORKING_MESSAGE_PATH_CHALLENGE
        && frame_type != NETWORKING_MESSAGE_PATH_RESPONSE)
        return (FT_ERR_PERMISSION_DENIED);
    if (connection->secure_enabled != FT_FALSE)
    {
        ft_size_t associated_data_length;
        ft_size_t packet_number_offset;
        uint64_t packet_number_value;
        ft_size_t packet_offset;

        associated_data_length = 36U;
        packet_number_offset = 20U;
        if (frame_type == NETWORKING_MESSAGE_ACK
            || frame_type == NETWORKING_MESSAGE_CLOSE
            || frame_type == NETWORKING_MESSAGE_PATH_CHALLENGE
            || frame_type == NETWORKING_MESSAGE_PATH_RESPONSE
            || frame_type == NETWORKING_MESSAGE_FLOW_CONTROL
            || frame_type == NETWORKING_MESSAGE_KEY_UPDATE
            || frame_type == NETWORKING_MESSAGE_KEY_UPDATE_ACK)
        {
            associated_data_length = 28U;
            packet_number_offset = 12U;
        }
        if (size < associated_data_length + 16U)
            return (FT_ERR_INVALID_ARGUMENT);
        packet_offset = packet_number_offset;
        if (!read_u64(data, size, packet_offset, packet_number_value))
            return (FT_ERR_INVALID_ARGUMENT);
        if (!read_u64(data, size, packet_offset, key_epoch)
            || (key_epoch != connection->secure_channel.get_receive_key_epoch()
                && frame_type != NETWORKING_MESSAGE_KEY_UPDATE)
            || (frame_type == NETWORKING_MESSAGE_KEY_UPDATE
                && connection->secure_channel.has_receive_epoch(key_epoch)
                    == FT_FALSE))
            return (FT_ERR_PERMISSION_DENIED);
        if (!connection->secure_channel.open_at_epoch(packet_number_value,
            key_epoch, data,
            associated_data_length, data + associated_data_length,
            size - associated_data_length - 16U, data + size - 16U, decrypted_packet))
            return (FT_ERR_PERMISSION_DENIED);
        ft_vector<uint8_t> decoded_packet;
        if (decoded_packet.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        ft_size_t decoded_index = 0U;
        while (decoded_index < associated_data_length)
        {
            decoded_packet.push_back(data[decoded_index]);
            decoded_index += 1U;
        }
        decoded_index = 0U;
        while (decoded_index < decrypted_packet.size())
        {
            decoded_packet.push_back(decrypted_packet[decoded_index]);
            decoded_index += 1U;
        }
        size = decoded_packet.size();
        (void)decrypted_packet.destroy();
        if (decrypted_packet.move(decoded_packet) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        data = &decrypted_packet[0];
    }
    if (frame_type == NETWORKING_MESSAGE_FLOW_CONTROL)
    {
        uint64_t receive_credit;

        offset = 4U;
        if (!read_u64(data, size, offset, connection_id)
            || !read_u64(data, size, offset, packet_number)
            || !read_u64(data, size, offset, key_epoch)
            || !read_u64(data, size, offset, receive_credit)
            || offset != size)
            return (FT_ERR_INVALID_ARGUMENT);
        if (connection->secure_enabled == FT_FALSE && key_epoch != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (connection->remote_flow_control_received != FT_FALSE
            && receive_credit < connection->remote_flow_credit)
            return (FT_ERR_INVALID_ARGUMENT);
        connection->remote_flow_credit = receive_credit;
        connection->remote_flow_control_received = FT_TRUE;
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = this->_io->now_milliseconds();
        connection->statistics.last_receive_milliseconds = connection->last_receive;
        (void)packet_number;
        return (FT_ERR_SUCCESS);
    }
    if (frame_type == NETWORKING_MESSAGE_KEY_UPDATE)
    {
        uint64_t current_epoch;
        uint64_t requested_epoch;
        uint64_t now_milliseconds;
        int32_t update_result;

        offset = 4U;
        if (!read_u64(data, size, offset, connection_id)
            || !read_u64(data, size, offset, packet_number)
            || !read_u64(data, size, offset, key_epoch)
            || !read_u64(data, size, offset, requested_epoch)
            || offset != size
            || connection->secure_enabled == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        current_epoch = connection->secure_channel.get_receive_key_epoch();
        now_milliseconds = this->_io->now_milliseconds();
        if (key_epoch == current_epoch && requested_epoch > current_epoch)
        {
            update_result = this->send_key_update_ack(*connection,
                requested_epoch);
            if (update_result == FT_ERR_SUCCESS)
                update_result = connection->secure_channel.update_receive_key_epoch(
                    requested_epoch);
        }
        else if (key_epoch != current_epoch
            && requested_epoch == current_epoch)
            update_result = this->send_key_update_ack(*connection,
                requested_epoch);
        else
            update_result = FT_ERR_INVALID_ARGUMENT;
        if (update_result != FT_ERR_SUCCESS)
            return (update_result);
        connection->previous_receive_key_expiry =
            now_milliseconds
            + static_cast<uint64_t>(this->_configuration
                .retransmission_timeout_milliseconds) * 4U;
        if (connection->previous_receive_key_expiry == 0U)
            connection->previous_receive_key_expiry = 1U;
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = now_milliseconds;
        connection->statistics.last_receive_milliseconds = connection->last_receive;
        return (FT_ERR_SUCCESS);
    }
    if (frame_type == NETWORKING_MESSAGE_KEY_UPDATE_ACK)
    {
        uint64_t acknowledged_epoch;
        int32_t update_result;

        offset = 4U;
        if (!read_u64(data, size, offset, connection_id)
            || !read_u64(data, size, offset, packet_number)
            || !read_u64(data, size, offset, key_epoch)
            || !read_u64(data, size, offset, acknowledged_epoch)
            || offset != size
            || connection->secure_enabled == FT_FALSE
            || key_epoch != connection->secure_channel.get_receive_key_epoch()
            || connection->key_update_pending == FT_FALSE
            || acknowledged_epoch != connection->key_update_epoch)
            return (FT_ERR_INVALID_ARGUMENT);
        update_result = connection->secure_channel.update_send_key_epoch(
            acknowledged_epoch);
        if (update_result != FT_ERR_SUCCESS)
            return (update_result);
        connection->key_update_pending = FT_FALSE;
        connection->key_update_epoch = 0U;
        connection->key_update_sent_at = 0U;
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = this->_io->now_milliseconds();
        connection->statistics.last_receive_milliseconds = connection->last_receive;
        return (FT_ERR_SUCCESS);
    }
    if (frame_type == NETWORKING_MESSAGE_PATH_CHALLENGE
        || frame_type == NETWORKING_MESSAGE_PATH_RESPONSE)
    {
        uint8_t path_token[NETWORKING_MESSAGE_PATH_TOKEN_SIZE];
        ft_bool token_matches;

        offset = 4U;
        if (!read_u64(data, size, offset, connection_id)
            || !read_u64(data, size, offset, packet_number)
            || !read_u64(data, size, offset, key_epoch)
            || size - offset != NETWORKING_MESSAGE_PATH_TOKEN_SIZE)
            return (FT_ERR_INVALID_ARGUMENT);
        if (connection->secure_enabled == FT_FALSE && key_epoch != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        ft_memcpy(path_token, data + offset, sizeof(path_token));
        if (frame_type == NETWORKING_MESSAGE_PATH_CHALLENGE)
        {
            int32_t path_result = this->send_path_packet(*connection,
                NETWORKING_MESSAGE_PATH_RESPONSE, source, path_token);
            connection->statistics.packets_received += 1U;
            connection->statistics.bytes_received += size;
            connection->last_receive = this->_io->now_milliseconds();
            connection->statistics.last_receive_milliseconds = connection->last_receive;
            networking_message_wipe(path_token, sizeof(path_token));
            return (path_result);
        }
        token_matches = FT_FALSE;
        if (connection->path_validation_pending != FT_FALSE
            && networking_message_endpoint_equal(connection->pending_path, source)
                != FT_FALSE
            && ft_memcmp(connection->pending_path_token, path_token,
                sizeof(path_token)) == 0)
            token_matches = FT_TRUE;
        if (token_matches == FT_FALSE)
        {
            networking_message_wipe(path_token, sizeof(path_token));
            return (FT_ERR_PERMISSION_DENIED);
        }
        connection->remote = source;
        connection->path_validation_pending = FT_FALSE;
        connection->path_challenge_sent = 0U;
        ft_memset(&connection->pending_path, 0, sizeof(connection->pending_path));
        ft_memset(connection->pending_path_token, 0,
            sizeof(connection->pending_path_token));
        connection->statistics.path_migrations += 1U;
        (void)this->emit_event(networking_message_event_type::PATH_CHANGED,
            connection->id, FT_ERR_SUCCESS, ft_nullptr);
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = this->_io->now_milliseconds();
        connection->statistics.last_receive_milliseconds = connection->last_receive;
        networking_message_wipe(path_token, sizeof(path_token));
        return (FT_ERR_SUCCESS);
    }
    if (networking_message_endpoint_equal(connection->remote, source) == FT_FALSE)
    {
        networking_crypto_backend random_backend;
        uint8_t path_token[NETWORKING_MESSAGE_PATH_TOKEN_SIZE];
        int32_t path_result;

        if (connection->path_validation_pending == FT_FALSE
            || networking_message_endpoint_equal(connection->pending_path, source)
                == FT_FALSE)
        {
            if (random_backend.random_bytes(path_token, sizeof(path_token))
                != FT_ERR_SUCCESS)
                return (FT_ERR_INTERNAL);
            connection->pending_path = source;
            ft_memcpy(connection->pending_path_token, path_token,
                sizeof(path_token));
            connection->path_validation_pending = FT_TRUE;
            connection->path_challenge_sent = this->_io->now_milliseconds();
            path_result = this->send_path_packet(*connection,
                NETWORKING_MESSAGE_PATH_CHALLENGE, source, path_token);
            networking_message_wipe(path_token, sizeof(path_token));
            if (path_result != FT_ERR_SUCCESS)
                return (path_result);
        }
        return (FT_ERR_PERMISSION_DENIED);
    }
    if (frame_type == NETWORKING_MESSAGE_CLOSE)
    {
        offset = 4U;
        if (!read_u64(data, size, offset, connection_id)
            || !read_u64(data, size, offset, packet_number)
            || !read_u64(data, size, offset, key_epoch)
            || !read_u16(data, size, offset, close_reason)
            || offset >= size)
            return (FT_ERR_INVALID_ARGUMENT);
        if (connection->secure_enabled == FT_FALSE && key_epoch != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        close_text_length = data[offset++];
        if (close_text_length != size - offset
            || close_text_length >= sizeof(close_text)
            || close_reason > static_cast<uint16_t>(
                networking_message_close_reason::INTERNAL_ERROR)
            || connection->state == networking_message_connection_state::CLOSED)
            return (FT_ERR_INVALID_ARGUMENT);
        ft_memset(close_text, 0, sizeof(close_text));
        if (close_text_length != 0U)
            ft_memcpy(close_text, data + offset, close_text_length);
        connection->statistics.packets_received += 1U;
        connection->statistics.bytes_received += size;
        connection->last_receive = this->_io->now_milliseconds();
        connection->statistics.last_receive_milliseconds = connection->last_receive;
        (void)this->emit_event(networking_message_event_type::PEER_CLOSING,
            connection->id, static_cast<int32_t>(close_reason),
            close_text);
        connection->state = networking_message_connection_state::CLOSED;
        (void)this->emit_event(networking_message_event_type::CLOSED,
            connection->id, static_cast<int32_t>(close_reason), ft_nullptr);
        return (FT_ERR_SUCCESS);
    }
    if (frame_type == NETWORKING_MESSAGE_ACK)
    {
        uint64_t acknowledgement_packet_number;

        offset = 4U;
        if (!read_u64(data, size, offset, connection_id)
            || !read_u64(data, size, offset, acknowledgement_packet_number)
            || !read_u64(data, size, offset, key_epoch)
            || !read_u64(data, size, offset, largest_acknowledged)
            || !networking_message_read_ack_ranges(data, size, offset,
                acknowledged_range_count, acknowledged_range_starts,
                acknowledged_range_ends))
            return (FT_ERR_INVALID_ARGUMENT);
        if (connection->secure_enabled == FT_FALSE && key_epoch != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        (void)acknowledgement_packet_number;
        networking_message_apply_ack(*connection, largest_acknowledged,
            acknowledged_range_count, acknowledged_range_starts,
            acknowledged_range_ends, this->_io->now_milliseconds());
        return (FT_ERR_SUCCESS);
    }
    offset = 4U;
    if (!read_u64(data, size, offset, connection_id)
        || !read_u64(data, size, offset, message_id)
        || !read_u64(data, size, offset, packet_number)
        || !read_u64(data, size, offset, key_epoch)
        || !read_u64(data, size, offset, largest_acknowledged)
        || !networking_message_read_ack_ranges(data, size, offset,
            acknowledged_range_count, acknowledged_range_starts,
            acknowledged_range_ends)
        || (frame_type == NETWORKING_MESSAGE_FRAME && !read_u32(data, size, offset, channel)))
        return (FT_ERR_INVALID_ARGUMENT);
    if (connection->secure_enabled == FT_FALSE && key_epoch != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (offset + 2U > size)
        return (FT_ERR_INVALID_ARGUMENT);
    lane = data[offset++];
    fragment_count = data[offset++];
    if (!read_u32(data, size, offset, total_size)
        || !read_u32(data, size, offset, fragment_offset)
        || !read_u64(data, size, offset, sequence)
        || !read_u16(data, size, offset, payload_size))
        return (FT_ERR_INVALID_ARGUMENT);
    if (delivery_value > static_cast<uint8_t>(
            networking_message_delivery::UNRELIABLE_SEQUENCED))
        return (FT_ERR_INVALID_ARGUMENT);
    connection = this->find_connection(connection_id);
    if (connection == ft_nullptr || connection->state != networking_message_connection_state::CONNECTED)
        return (FT_ERR_INVALID_HANDLE);
    if (payload_size > size - offset || total_size > this->_configuration.maximum_message_size
        || fragment_count == 0U || fragment_offset > total_size
        || payload_size > total_size - fragment_offset)
        return (FT_ERR_OUT_OF_RANGE);
    connection->statistics.packets_received += 1U;
    connection->statistics.bytes_received += size;
    connection->last_receive = this->_io->now_milliseconds();
    connection->statistics.last_receive_milliseconds = connection->last_receive;
    if (networking_message_record_packet(*connection, packet_number) == FT_FALSE)
    {
        connection->statistics.duplicate_packets += 1U;
        if (connection->largest_received != 0U)
            connection->last_acknowledged_sent = 0U;
    }
    else if (packet_number > connection->largest_received)
        connection->largest_received = packet_number;
    networking_message_apply_ack(*connection, largest_acknowledged,
        acknowledged_range_count, acknowledged_range_starts,
        acknowledged_range_ends, this->_io->now_milliseconds());
    if (fragment_count == 1U)
    {
        networking_received_message message;
        message.connection_id = connection->id;
        message.channel = channel;
        message.lane = lane;
        message.delivery = static_cast<networking_message_delivery>(delivery_value);
        message.sequence = sequence;
        if (message.payload.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        message.payload.resize(payload_size);
        if (message.payload.get_error() != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        if (payload_size != 0U)
            ft_memcpy(&message.payload[0], data + offset, payload_size);
        ft_size_t received_before = this->_received_messages.size();
        networking_message_deliver(*connection, message, this->_received_messages);
        if (this->_received_messages.size() > received_before)
            (void)this->emit_event(networking_message_event_type::MESSAGE_AVAILABLE,
                connection->id, FT_ERR_SUCCESS, ft_nullptr);
        return (FT_ERR_SUCCESS);
    }
    reassembly_record *record_pointer;
    ft_size_t reassembly_index;
    uint32_t byte_index;

    record_pointer = ft_nullptr;
    reassembly_index = 0U;
    while (reassembly_index < connection->reassembly.size())
    {
        if (connection->reassembly[reassembly_index]->message_id == message_id)
        {
            record_pointer = connection->reassembly[reassembly_index];
            break ;
        }
        reassembly_index += 1U;
    }
    if (record_pointer == ft_nullptr)
    {
        if (connection->reassembly.size()
            >= this->_configuration.maximum_reassembly_messages
            || connection->statistics.reassembly_bytes
                > connection->receive_flow_credit
            || total_size > connection->receive_flow_credit
                - connection->statistics.reassembly_bytes)
            return (FT_ERR_FULL);
        if (NETWORKING_TEST_SHOULD_FAIL(
                NETWORKING_TEST_REASSEMBLY_ALLOCATE) != FT_FALSE)
            record_pointer = ft_nullptr;
        else
            record_pointer = new (std::nothrow) reassembly_record();
        if (record_pointer == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (record_pointer->initialize() != FT_ERR_SUCCESS)
        {
            delete record_pointer;
            return (FT_ERR_NO_MEMORY);
        }
        record_pointer->message_id = message_id;
        record_pointer->sequence = sequence;
        record_pointer->channel = channel;
        record_pointer->lane = lane;
        record_pointer->delivery = static_cast<networking_message_delivery>(delivery_value);
        record_pointer->total_size = total_size;
        record_pointer->fragment_count = fragment_count;
        record_pointer->expires_at = this->_io->now_milliseconds() + this->_configuration.idle_timeout_milliseconds;
        record_pointer->payload.resize(total_size);
        record_pointer->received.resize(total_size, 0U);
        if (record_pointer->payload.get_error() != FT_ERR_SUCCESS
            || record_pointer->received.get_error() != FT_ERR_SUCCESS
            || connection->reassembly.push_back(record_pointer) != FT_ERR_SUCCESS)
        {
            delete record_pointer;
            return (FT_ERR_NO_MEMORY);
        }
        connection->statistics.reassembly_bytes += total_size;
        connection->statistics.reassembly_messages += 1U;
    }
    if (record_pointer->total_size != total_size
        || record_pointer->fragment_count != fragment_count
        || record_pointer->channel != channel
        || record_pointer->sequence != sequence)
        return (FT_ERR_INVALID_ARGUMENT);
    if (fragment_offset + payload_size > record_pointer->payload.size())
        return (FT_ERR_OUT_OF_RANGE);
    byte_index = 0U;
    while (byte_index < payload_size)
    {
        if (record_pointer->received[fragment_offset + byte_index] != 0U
            && record_pointer->payload[fragment_offset + byte_index]
                != data[offset + byte_index])
            return (FT_ERR_INVALID_ARGUMENT);
        byte_index += 1U;
    }
    if (payload_size != 0U)
        ft_memcpy(&record_pointer->payload[fragment_offset], data + offset, payload_size);
    byte_index = 0U;
    while (byte_index < payload_size)
    {
        if (record_pointer->received[fragment_offset + byte_index] == 0U)
        {
            record_pointer->received[fragment_offset + byte_index] = 1U;
            record_pointer->received_count += 1U;
        }
        byte_index += 1U;
    }
    connection->statistics.fragments_reassembled += 1U;
    if (record_pointer->received_count == record_pointer->total_size)
    {
        networking_received_message message;
        message.connection_id = connection->id;
        message.channel = record_pointer->channel;
        message.lane = record_pointer->lane;
        message.delivery = record_pointer->delivery;
        message.sequence = record_pointer->sequence;
        if (message.payload.initialize(record_pointer->payload) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        ft_size_t received_before = this->_received_messages.size();
        networking_message_deliver(*connection, message, this->_received_messages);
        if (this->_received_messages.size() > received_before)
            (void)this->emit_event(networking_message_event_type::MESSAGE_AVAILABLE,
                connection->id, FT_ERR_SUCCESS, ft_nullptr);
        connection->statistics.reassembly_bytes -= record_pointer->total_size;
        if (connection->receive_flow_credit <= UINT64_MAX
            - record_pointer->total_size)
            connection->receive_flow_credit += record_pointer->total_size;
        else
            connection->receive_flow_credit = UINT64_MAX;
        if (connection->statistics.reassembly_messages != 0U)
            connection->statistics.reassembly_messages -= 1U;
        delete record_pointer;
        connection->reassembly.erase(connection->reassembly.begin() + reassembly_index);
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::poll(int32_t timeout_milliseconds) noexcept
{
    networking_datagram_io *io;
    int32_t lock_result;
    int32_t wait_result;

    if (timeout_milliseconds < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->should_queue_commands() != FT_FALSE)
        return (FT_ERR_THREAD_BUSY);
    if (timeout_milliseconds == 0)
        return (this->poll());
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_io == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    io = this->_io;
    this->unlock_transport();
    wait_result = io->wait_readable(timeout_milliseconds);
    if (wait_result != FT_ERR_SUCCESS
        && wait_result != FT_ERR_TIMEOUT
        && wait_result != FT_ERR_UNSUPPORTED_TYPE)
        return (wait_result);
    return (this->poll());
}

int32_t networking_message_transport::set_event_callback(
    networking_message_event_callback callback, void *user_data) noexcept
{
    int32_t lock_result;
    int32_t command_result;
    networking_message_command *command;

    if (this->should_queue_commands() != FT_FALSE)
    {
        command = new (std::nothrow) networking_message_command();
        if (command == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (command->initialize() != FT_ERR_SUCCESS)
        {
            delete command;
            return (FT_ERR_NO_MEMORY);
        }
        command->type = networking_message_command_type::SET_EVENT_CALLBACK;
        command->callback = callback;
        command->callback_user_data = user_data;
        command_result = this->enqueue_command(*command);
        command->destroy();
        delete command;
        return (command_result);
    }
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    this->_event_callback = callback;
    this->_event_callback_user_data = user_data;
    if (callback == ft_nullptr)
        this->_callback_events.clear();
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::poll() noexcept
{
    int32_t lock_result;
    uint8_t buffer[65536];
    networking_message_endpoint source;
    ft_size_t received_size;
    int32_t receive_result;
    int32_t first_error;
    int32_t process_result;
    int32_t advance_result;
    uint64_t now;
    ft_size_t index;

    if (this->should_queue_commands() != FT_FALSE)
        return (FT_ERR_THREAD_BUSY);
    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED || this->_io == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    first_error = FT_ERR_SUCCESS;
    while (true)
    {
        received_size = 0U;
        receive_result = this->_io->receive_datagram(source, buffer, sizeof(buffer), &received_size);
        if (receive_result == FT_ERR_EMPTY)
            break ;
        if (receive_result != FT_ERR_SUCCESS)
        {
            first_error = receive_result;
            break ;
        }
        if (received_size != 0U)
        {
            process_result = this->process_datagram(source, buffer, received_size);
            if (process_result != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
                first_error = process_result;
        }
    }
    now = this->_io->now_milliseconds();
    index = 0U;
    while (index < this->_connections.size())
    {
        if (this->_connections[index]->state == networking_message_connection_state::CONNECTED
            || this->_connections[index]->state
                == networking_message_connection_state::HANDSHAKING
            || this->_connections[index]->state
                == networking_message_connection_state::DRAINING)
        {
            advance_result = this->advance_connection(*this->_connections[index], now);
            if (advance_result != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
                first_error = advance_result;
        }
        index += 1U;
    }
    this->unlock_transport();
    if (this->_worker_native_id.load(std::memory_order_acquire)
        != pt_thread_self())
        this->dispatch_event_callbacks();
    return (first_error);
}

int32_t networking_message_transport::receive_message(networking_received_message &message) noexcept
{
    int32_t lock_result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (this->_received_messages.empty())
    {
        this->unlock_transport();
        return (FT_ERR_EMPTY);
    }
    networking_received_message *stored_message = this->_received_messages.pop_front();
    if (stored_message == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INTERNAL);
    }
    message.connection_id = stored_message->connection_id;
    message.channel = stored_message->channel;
    message.lane = stored_message->lane;
    message.delivery = stored_message->delivery;
    message.sequence = stored_message->sequence;
    (void)message.payload.destroy();
    if (message.payload.move(stored_message->payload) != FT_ERR_SUCCESS)
    {
        delete stored_message;
        this->unlock_transport();
        return (FT_ERR_NO_MEMORY);
    }
    delete stored_message;
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::receive_messages(
    networking_received_message *messages, ft_size_t capacity,
    ft_size_t &received_count) noexcept
{
    int32_t lock_result;
    int32_t result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    received_count = 0U;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (messages == ft_nullptr || capacity == 0U)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    while (received_count < capacity)
    {
        result = this->receive_message(messages[received_count]);
        if (result == FT_ERR_EMPTY)
        {
            if (received_count == 0U)
            {
                this->unlock_transport();
                return (FT_ERR_EMPTY);
            }
            this->unlock_transport();
            return (FT_ERR_SUCCESS);
        }
        if (result != FT_ERR_SUCCESS)
        {
            this->unlock_transport();
            return (result);
        }
        received_count += 1U;
    }
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::receive_event(
    networking_message_event &event) noexcept
{
    int32_t lock_result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    if (this->_events.empty())
    {
        this->unlock_transport();
        return (FT_ERR_EMPTY);
    }
    event = this->_events.pop_front();
    if (this->_events.get_error() != FT_ERR_SUCCESS)
    {
        this->unlock_transport();
        return (FT_ERR_INTERNAL);
    }
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}

int32_t networking_message_transport::get_statistics(uint64_t connection_id,
    networking_message_statistics &statistics) const noexcept
{
    return (this->get_connection_statistics(connection_id, statistics));
}

int32_t networking_message_transport::export_observability() const noexcept
{
    int32_t lock_result;
    networking_message_statistics aggregate;
    networking_message_statistics current;
    ft_size_t index;
    ft_size_t connection_count;
    uint32_t lane;
    ft_networking_observability_sample sample;
    int32_t result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_transport();
        return (FT_ERR_NOT_INITIALISED);
    }
    aggregate = networking_message_statistics();
    connection_count = this->_connections.size();
    index = 0U;
    while (index < connection_count)
    {
        if (this->_connections[index] != ft_nullptr)
        {
            current = this->_connections[index]->statistics;
            current.bytes_in_flight = this->_connections[index]->bytes_in_flight;
            current.congestion_window_bytes =
                this->_connections[index]->congestion_window;
            aggregate.packets_sent += current.packets_sent;
            aggregate.packets_received += current.packets_received;
            aggregate.bytes_sent += current.bytes_sent;
            aggregate.bytes_received += current.bytes_received;
            aggregate.packets_acknowledged += current.packets_acknowledged;
            aggregate.packets_lost += current.packets_lost;
            aggregate.packets_retransmitted += current.packets_retransmitted;
            aggregate.duplicate_packets += current.duplicate_packets;
            aggregate.reordered_packets += current.reordered_packets;
            aggregate.messages_sent += current.messages_sent;
            aggregate.messages_received += current.messages_received;
            aggregate.messages_expired += current.messages_expired;
            aggregate.fragments_reassembled += current.fragments_reassembled;
            aggregate.fragments_dropped += current.fragments_dropped;
            aggregate.reassembly_bytes += current.reassembly_bytes;
            aggregate.reassembly_messages += current.reassembly_messages;
            aggregate.queue_bytes += current.queue_bytes;
            aggregate.queue_depth += current.queue_depth;
            aggregate.bytes_in_flight += current.bytes_in_flight;
            aggregate.congestion_window_bytes += current.congestion_window_bytes;
            aggregate.pacing_rate_bytes_per_second +=
                current.pacing_rate_bytes_per_second;
            aggregate.malformed_packets += current.malformed_packets;
            aggregate.authentication_failures += current.authentication_failures;
            aggregate.replay_rejections += current.replay_rejections;
            aggregate.path_migrations += current.path_migrations;
            aggregate.nat_attempts += current.nat_attempts;
            aggregate.relay_fallbacks += current.relay_fallbacks;
            if (current.latest_rtt_milliseconds > aggregate.latest_rtt_milliseconds)
                aggregate.latest_rtt_milliseconds = current.latest_rtt_milliseconds;
            if (current.smoothed_rtt_milliseconds > aggregate.smoothed_rtt_milliseconds)
                aggregate.smoothed_rtt_milliseconds = current.smoothed_rtt_milliseconds;
            if (current.minimum_rtt_milliseconds != 0U
                && (aggregate.minimum_rtt_milliseconds == 0U
                    || current.minimum_rtt_milliseconds
                        < aggregate.minimum_rtt_milliseconds))
                aggregate.minimum_rtt_milliseconds = current.minimum_rtt_milliseconds;
            if (current.last_receive_milliseconds > aggregate.last_receive_milliseconds)
                aggregate.last_receive_milliseconds = current.last_receive_milliseconds;
            if (current.last_send_milliseconds > aggregate.last_send_milliseconds)
                aggregate.last_send_milliseconds = current.last_send_milliseconds;
            if (current.last_acknowledgement_milliseconds
                > aggregate.last_acknowledgement_milliseconds)
                aggregate.last_acknowledgement_milliseconds =
                    current.last_acknowledgement_milliseconds;
            if (current.last_progress_milliseconds > aggregate.last_progress_milliseconds)
                aggregate.last_progress_milliseconds = current.last_progress_milliseconds;
            lane = 0U;
            while (lane < 4U)
            {
                aggregate.lane_messages_sent[lane] +=
                    current.lane_messages_sent[lane];
                aggregate.lane_messages_received[lane] +=
                    current.lane_messages_received[lane];
                aggregate.lane_bytes_sent[lane] += current.lane_bytes_sent[lane];
                aggregate.lane_bytes_received[lane] +=
                    current.lane_bytes_received[lane];
                aggregate.lane_queued_bytes[lane] +=
                    current.lane_queued_bytes[lane];
                aggregate.lane_sent_bytes_window[lane] +=
                    current.lane_sent_bytes_window[lane];
                aggregate.lane_rate_bytes_per_second[lane] +=
                    current.lane_rate_bytes_per_second[lane];
                if (current.lane_priority_weight[lane]
                    > aggregate.lane_priority_weight[lane])
                    aggregate.lane_priority_weight[lane] =
                        current.lane_priority_weight[lane];
                aggregate.lane_reserved_bandwidth_bytes_per_second[lane] +=
                    current.lane_reserved_bandwidth_bytes_per_second[lane];
                lane += 1U;
            }
        }
        index += 1U;
    }
    this->unlock_transport();
    sample.labels.component = "networking_message_transport";
    sample.labels.operation = "export_statistics";
    sample.labels.target = "aggregate";
    sample.labels.resource = "connections";
    sample.duration_ms = static_cast<int64_t>(aggregate.smoothed_rtt_milliseconds);
    sample.request_bytes = static_cast<ft_size_t>(aggregate.bytes_sent);
    sample.response_bytes = static_cast<ft_size_t>(aggregate.bytes_received);
    sample.status_code = static_cast<int32_t>(connection_count);
    sample.error_code = FT_ERR_SUCCESS;
    sample.error_tag = "ok";
    sample.success = FT_TRUE;
    result = observability_networking_metrics_record(sample);
    return (result);
}

int32_t networking_message_transport::get_connection(uint64_t connection_id,
    networking_message_connection &connection) noexcept
{
    int32_t lock_result;

    lock_result = this->lock_transport();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (this->find_connection(connection_id) == ft_nullptr)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_HANDLE);
    }
    if (connection.initialize() != FT_ERR_SUCCESS)
    {
        this->unlock_transport();
        return (FT_ERR_INVALID_STATE);
    }
    connection.bind(this, connection_id);
    this->unlock_transport();
    return (FT_ERR_SUCCESS);
}
