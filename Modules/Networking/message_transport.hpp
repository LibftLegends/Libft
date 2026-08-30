#ifndef NETWORKING_MESSAGE_TRANSPORT_HPP
#define NETWORKING_MESSAGE_TRANSPORT_HPP

#include "networking.hpp"
#include "udp_socket.hpp"
#include "networking_secure_channel.hpp"
#include "../Template/vector.hpp"
#include "../Template/deque.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <atomic>
#include <cstdint>

enum class networking_message_delivery : uint8_t
{
    RELIABLE_ORDERED = 0U,
    UNRELIABLE = 1U,
    UNRELIABLE_SEQUENCED = 2U
};

enum class networking_message_connection_state : uint8_t
{
    IDLE = 0U,
    RESOLVING = 1U,
    PROBING = 2U,
    HANDSHAKING = 3U,
    CONNECTED = 4U,
    DRAINING = 5U,
    CLOSED = 6U,
    FAILED = 7U
};

enum class networking_message_event_type : uint8_t
{
    CONNECTING = 0U,
    CONNECTION_REQUESTED = 1U,
    CONNECTED = 2U,
    PATH_CHANGED = 3U,
    MESSAGE_AVAILABLE = 4U,
    DELIVERY_FAILED = 5U,
    PEER_CLOSING = 6U,
    CLOSED = 7U,
    FAILED = 8U
};

enum class networking_message_close_reason : uint16_t
{
    NONE = 0U,
    APPLICATION = 1U,
    PROTOCOL_ERROR = 2U,
    AUTHENTICATION_FAILED = 3U,
    TIMEOUT = 4U,
    RESOURCE_LIMIT = 5U,
    INTERNAL_ERROR = 6U
};

struct networking_message_event
{
    networking_message_event_type type;
    uint64_t connection_id;
    int32_t reason;
    char debug_text[96];

    networking_message_event() noexcept;
    ~networking_message_event() noexcept;
};

typedef void (*networking_message_event_callback)(
    const networking_message_event &event, void *user_data) noexcept;

struct networking_message_command;

struct networking_message_endpoint
{
    sockaddr_storage address;
    socklen_t length;
};

struct networking_message_peer_identity
{
    uint8_t public_key[32];
    ft_bool authenticated;

    networking_message_peer_identity() noexcept;
    ~networking_message_peer_identity() noexcept;
};

static const uint32_t NETWORKING_MESSAGE_MAX_PEER_CANDIDATES = 8U;

struct networking_message_peer_connect_ticket
{
    uint64_t peer_id;
    uint64_t attempt_id;
    uint64_t expires_at;
    uint64_t tie_breaker;
    networking_message_endpoint candidates[NETWORKING_MESSAGE_MAX_PEER_CANDIDATES];
    uint32_t candidate_count;
    uint8_t signature[64];
    uint8_t signature_length;

    networking_message_peer_connect_ticket() noexcept;
    ~networking_message_peer_connect_ticket() noexcept;
};

class networking_message_peer_ticket_verifier
{
    public:
        networking_message_peer_ticket_verifier() noexcept;
        networking_message_peer_ticket_verifier(
            const networking_message_peer_ticket_verifier &other) noexcept = delete;
        networking_message_peer_ticket_verifier(
            networking_message_peer_ticket_verifier &&other) noexcept = delete;
        virtual ~networking_message_peer_ticket_verifier() noexcept;

        networking_message_peer_ticket_verifier &operator=(
            const networking_message_peer_ticket_verifier &other) noexcept = delete;
        networking_message_peer_ticket_verifier &operator=(
            networking_message_peer_ticket_verifier &&other) noexcept = delete;

        virtual ft_bool verify(
            const networking_message_peer_connect_ticket &ticket) noexcept = 0;
};

struct networking_message_send_options
{
    networking_message_delivery delivery;
    uint8_t lane;
    uint32_t channel;
    uint64_t expiry_milliseconds;

    networking_message_send_options() noexcept;
    ~networking_message_send_options() noexcept;
};

struct networking_message_transport_config
{
    uint32_t maximum_datagram_size;
    uint32_t maximum_message_size;
    uint32_t maximum_reassembly_bytes;
    uint32_t maximum_reassembly_messages;
    uint32_t maximum_queued_bytes;
    uint32_t maximum_events;
    uint32_t retransmission_timeout_milliseconds;
    uint32_t idle_timeout_milliseconds;
    uint32_t acknowledgement_delay_milliseconds;
    ft_bool enable_reliability;
    ft_bool enable_encryption;
    ft_bool enable_authenticated_handshake;
    ft_bool enable_retry_cookies;
    ft_bool enable_peer_key_pinning;
    ft_bool enable_thread_safety;
    uint32_t retry_cookie_lifetime_milliseconds;
    uint8_t encryption_key[32];
    ft_size_t encryption_key_length;
    uint8_t encryption_initialization_vector[12];
    uint8_t retry_cookie_secret[32];
    uint8_t pinned_peer_public_key[32];

    networking_message_transport_config() noexcept;
    ~networking_message_transport_config() noexcept;
};

struct networking_received_message
{
    uint64_t connection_id;
    uint32_t channel;
    uint8_t lane;
    networking_message_delivery delivery;
    uint64_t sequence;
    ft_vector<uint8_t> payload;

    networking_received_message() noexcept;
    ~networking_received_message() noexcept;
};

struct networking_message_statistics
{
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t packets_acknowledged;
    uint64_t packets_lost;
    uint64_t packets_retransmitted;
    uint64_t duplicate_packets;
    uint64_t reordered_packets;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_expired;
    uint64_t fragments_reassembled;
    uint64_t fragments_dropped;
    uint64_t reassembly_bytes;
    uint64_t reassembly_messages;
    uint64_t smoothed_rtt_milliseconds;
    uint64_t latest_rtt_milliseconds;
    uint64_t minimum_rtt_milliseconds;
    uint64_t rtt_variance_milliseconds;
    uint64_t jitter_milliseconds;
    uint64_t queue_bytes;
    uint64_t queue_depth;
    uint64_t bytes_in_flight;
    uint64_t congestion_window_bytes;
    uint64_t pacing_rate_bytes_per_second;
    uint64_t malformed_packets;
    uint64_t authentication_failures;
    uint64_t replay_rejections;
    uint64_t path_migrations;
    uint64_t nat_attempts;
    uint64_t relay_fallbacks;
    uint64_t last_receive_milliseconds;
    uint64_t last_send_milliseconds;
    uint64_t last_acknowledgement_milliseconds;
    uint64_t last_progress_milliseconds;
    uint64_t lane_messages_sent[4];
    uint64_t lane_messages_received[4];
    uint64_t lane_bytes_sent[4];
    uint64_t lane_bytes_received[4];
    uint64_t lane_queued_bytes[4];
    uint64_t lane_sent_bytes_window[4];
    uint64_t lane_rate_bytes_per_second[4];
    uint32_t lane_priority_weight[4];
    uint32_t lane_reserved_bandwidth_bytes_per_second[4];

    networking_message_statistics() noexcept;
    ~networking_message_statistics() noexcept;
};

class networking_datagram_io
{
    public:
        networking_datagram_io() noexcept;
        networking_datagram_io(const networking_datagram_io &other) noexcept = delete;
        networking_datagram_io(networking_datagram_io &&other) noexcept = delete;
        virtual ~networking_datagram_io() noexcept;

        networking_datagram_io &operator=(const networking_datagram_io &other) noexcept = delete;
        networking_datagram_io &operator=(networking_datagram_io &&other) noexcept = delete;

        virtual int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept = 0;
    virtual int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept = 0;
        virtual uint64_t now_milliseconds() const noexcept = 0;
        virtual int32_t wait_readable(int32_t timeout_milliseconds) noexcept;
};

class networking_udp_datagram_io : public networking_datagram_io
{
    private:
        uint8_t _initialised_state;
        ft_bool _connected;
        udp_socket _socket;

    public:
        networking_udp_datagram_io() noexcept;
        networking_udp_datagram_io(const networking_udp_datagram_io &other) noexcept = delete;
        networking_udp_datagram_io(networking_udp_datagram_io &&other) noexcept = delete;
        ~networking_udp_datagram_io() noexcept;

        networking_udp_datagram_io &operator=(const networking_udp_datagram_io &other) noexcept = delete;
        networking_udp_datagram_io &operator=(networking_udp_datagram_io &&other) noexcept = delete;

        int32_t initialize(const SocketConfig &configuration) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_udp_datagram_io &other) noexcept;
        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override;
        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept override;
        uint64_t now_milliseconds() const noexcept override;
        int32_t wait_readable(int32_t timeout_milliseconds) noexcept override;
        int32_t get_file_descriptor() const noexcept;
};

class networking_message_transport;

class networking_message_connection
{
    private:
        uint8_t _initialised_state;
        networking_message_transport *_transport;
        uint64_t _connection_id;

    public:
        networking_message_connection() noexcept;
        networking_message_connection(const networking_message_connection &other) noexcept = delete;
        networking_message_connection(networking_message_connection &&other) noexcept = delete;
        ~networking_message_connection() noexcept;

        networking_message_connection &operator=(const networking_message_connection &other) noexcept = delete;
        networking_message_connection &operator=(networking_message_connection &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_message_connection &other) noexcept;
        int32_t send_message(const void *data, ft_size_t size,
            const networking_message_send_options &options) noexcept;
        int32_t close() noexcept;
        int32_t close(networking_message_close_reason reason,
            const char *debug_text) noexcept;
        int32_t abort(networking_message_close_reason reason) noexcept;
        int32_t update_key_epoch(uint64_t next_epoch) noexcept;
        int32_t request_key_update(uint64_t next_epoch) noexcept;
        int32_t configure_lane(uint8_t lane, uint32_t priority_weight,
            uint32_t reserved_bandwidth_bytes_per_second) noexcept;
        int32_t set_queue_limits(uint32_t maximum_queued_bytes) noexcept;
        int32_t flush() noexcept;
        uint64_t get_id() const noexcept;
        networking_message_connection_state get_state() const noexcept;
        int32_t get_statistics(networking_message_statistics &statistics) const noexcept;
        int32_t get_remote_identity(
            networking_message_peer_identity &identity) const noexcept;

        void bind(networking_message_transport *transport, uint64_t connection_id) noexcept;
};

class networking_message_transport
{
    public:
        struct connection_record;

    private:
        uint8_t _initialised_state;
        networking_message_transport_config _configuration;
        networking_datagram_io *_io;
        ft_bool _listening;
        networking_message_endpoint _listen_endpoint;
        ft_vector<connection_record *> _connections;
        ft_deque<networking_received_message *> _received_messages;
        ft_deque<networking_message_event> _events;
        ft_deque<networking_message_event> _callback_events;
        ft_deque<networking_message_command *> _commands;
        networking_message_event_callback _event_callback;
        void *_event_callback_user_data;
        ft_bool _containers_initialised;
        uint64_t _next_connection_id;
        uint64_t _last_error;
        pthread_t _worker_thread;
        std::atomic<ft_bool> _worker_thread_created;
        std::atomic<pt_thread_id_type> _worker_native_id;
        std::atomic<ft_bool> _worker_stop_requested;
        std::atomic<uint32_t> _worker_wakeup_epoch;
        mutable pt_recursive_mutex *_mutex;

        int32_t send_connection_message(uint64_t connection_id, const void *data,
            ft_size_t size, const networking_message_send_options &options) noexcept;
        int32_t close_connection(uint64_t connection_id,
            networking_message_close_reason reason, const char *debug_text,
            ft_bool abort_connection) noexcept;
        int32_t update_connection_key_epoch(uint64_t connection_id,
            uint64_t next_epoch) noexcept;
        int32_t request_connection_key_update(uint64_t connection_id,
            uint64_t next_epoch) noexcept;
        int32_t get_connection_state(uint64_t connection_id,
            networking_message_connection_state &state) const noexcept;
        int32_t get_connection_statistics(uint64_t connection_id,
            networking_message_statistics &statistics) const noexcept;
        int32_t get_connection_identity(uint64_t connection_id,
            networking_message_peer_identity &identity) const noexcept;
        int32_t configure_connection_lane(uint64_t connection_id, uint8_t lane,
            uint32_t priority_weight,
            uint32_t reserved_bandwidth_bytes_per_second) noexcept;
        int32_t set_connection_queue_limits(uint64_t connection_id,
            uint32_t maximum_queued_bytes) noexcept;
        int32_t flush_connection(uint64_t connection_id) noexcept;
#ifdef LIBFT_TEST_BUILD
    public:
#endif
        int32_t process_datagram(const networking_message_endpoint &source,
            const uint8_t *data, ft_size_t size) noexcept;
#ifdef LIBFT_TEST_BUILD
    private:
#endif
        int32_t process_handshake_datagram(
            const networking_message_endpoint &source, const uint8_t *data,
            ft_size_t size) noexcept;
        int32_t advance_connection(connection_record &connection,
            uint64_t now_milliseconds) noexcept;
        int32_t send_handshake_hello(connection_record &connection) noexcept;
        int32_t send_handshake_finished(connection_record &connection) noexcept;
        int32_t send_key_update_request(connection_record &connection,
            uint64_t next_epoch) noexcept;
        int32_t send_key_update_ack(connection_record &connection,
            uint64_t acknowledged_epoch) noexcept;
        int32_t send_path_packet(connection_record &connection, uint8_t frame_type,
            const networking_message_endpoint &destination,
            const uint8_t token[8]) noexcept;
        int32_t send_flow_control(connection_record &connection,
            uint64_t receive_credit) noexcept;
        int32_t send_close(connection_record &connection,
            networking_message_close_reason reason,
            const char *debug_text) noexcept;
        int32_t send_handshake_retry(const networking_message_endpoint &remote,
            uint64_t connection_id, const uint8_t cookie[40]) noexcept;
        connection_record *find_connection(uint64_t connection_id) const noexcept;
        void clear_records() noexcept;
        int32_t emit_event(networking_message_event_type type,
            uint64_t connection_id, int32_t reason,
            const char *debug_text) noexcept;
        void dispatch_event_callbacks() noexcept;
        ft_bool should_queue_commands() const noexcept;
        int32_t enqueue_command(networking_message_command &command) noexcept;
        void process_command_queue() noexcept;
        int32_t execute_command(networking_message_command &command) noexcept;
        void fail_pending_commands(int32_t result) noexcept;
        static void *worker_entry(void *argument) noexcept;
        void worker_loop() noexcept;
        void wake_worker() noexcept;
        int32_t lock_transport() const noexcept;
        void unlock_transport() const noexcept;

        friend class networking_message_connection;

    public:
        networking_message_transport() noexcept;
        networking_message_transport(const networking_message_transport &other) noexcept = delete;
        networking_message_transport(networking_message_transport &&other) noexcept = delete;
        ~networking_message_transport() noexcept;

        networking_message_transport &operator=(const networking_message_transport &other) noexcept = delete;
        networking_message_transport &operator=(networking_message_transport &&other) noexcept = delete;

        int32_t initialize(const networking_message_transport_config &configuration,
            networking_datagram_io &io) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_message_transport &other) noexcept;
        int32_t listen(const networking_message_endpoint &local) noexcept;
        int32_t accept(uint64_t connection_id) noexcept;
        int32_t reject(uint64_t connection_id,
            networking_message_close_reason reason) noexcept;
        int32_t connect(const networking_message_endpoint &remote,
            networking_message_connection &connection) noexcept;
        int32_t connect_peer(
            const networking_message_peer_connect_ticket &ticket,
            networking_message_connection &connection) noexcept;
        int32_t connect_peer(
            const networking_message_peer_connect_ticket &ticket,
            networking_message_peer_ticket_verifier &verifier,
            networking_message_connection &connection) noexcept;
        int32_t open_connection(const networking_message_endpoint &remote,
            networking_message_connection &connection) noexcept;
        int32_t poll() noexcept;
        int32_t poll(int32_t timeout_milliseconds) noexcept;
        int32_t dispatch_callbacks() noexcept;
        int32_t set_event_callback(networking_message_event_callback callback,
            void *user_data) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t start_worker() noexcept;
        int32_t stop_worker() noexcept;
        ft_bool is_worker_running() const noexcept;
        int32_t receive_message(networking_received_message &message) noexcept;
        int32_t receive_messages(networking_received_message *messages,
            ft_size_t capacity, ft_size_t &received_count) noexcept;
        int32_t receive_event(networking_message_event &event) noexcept;
        int32_t get_statistics(uint64_t connection_id,
            networking_message_statistics &statistics) const noexcept;
        int32_t export_observability() const noexcept;
        int32_t get_connection(uint64_t connection_id,
            networking_message_connection &connection) noexcept;
};

#endif
