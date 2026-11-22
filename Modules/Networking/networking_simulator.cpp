#include "networking_simulator.hpp"
#ifdef LIBFT_TEST_BUILD
# include "networking_test_hooks.hpp"
#else
# define NETWORKING_TEST_SHOULD_FAIL(point) FT_FALSE
#endif

networking_simulator_config::networking_simulator_config() noexcept
    : fixed_latency_milliseconds(0U), jitter_milliseconds(0U), loss_parts_per_million(0U),
      duplicate_parts_per_million(0U), corruption_parts_per_million(0U),
      reorder_parts_per_million(0U), maximum_datagram_size(65536U),
      maximum_pending_datagrams(4096U), bandwidth_bytes_per_second(0U),
      bandwidth_burst_bytes(0U), disconnect(FT_FALSE), blackhole_outgoing(FT_FALSE),
      blackhole_incoming(FT_FALSE), random_seed(1U)
{
    return ;
}

networking_simulator_config::~networking_simulator_config() noexcept
{
    return ;
}

networking_simulator_script_entry::networking_simulator_script_entry() noexcept
    : packet_ordinal(0U), action(networking_simulator_script_action::SCRIPT_DROP),
      delay_milliseconds(0U)
{
    return ;
}

networking_simulator_script_entry::~networking_simulator_script_entry() noexcept
{
    return ;
}

networking_simulated_datagram_io::networking_simulated_datagram_io() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _underlying(ft_nullptr),
      _configuration(), _pending(), _script(), _now(0U), _next_delivery_time(UINT64_MAX),
      _random_state(1U),
      _send_ordinal(0U),
      _bandwidth_window_start(0U), _bandwidth_window_bytes(0U)
{
    return ;
}

networking_simulated_datagram_io::~networking_simulated_datagram_io() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_simulated_datagram_io::initialize(networking_datagram_io &underlying,
    const networking_simulator_config &configuration) noexcept
{
    int32_t result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (configuration.loss_parts_per_million > 1000000U
        || configuration.duplicate_parts_per_million > 1000000U
        || configuration.corruption_parts_per_million > 1000000U
        || configuration.reorder_parts_per_million > 1000000U
        || configuration.maximum_datagram_size == 0U
        || configuration.maximum_pending_datagrams == 0U)
        return (FT_ERR_CONFIGURATION);
    this->_underlying = &underlying;
    this->_configuration = configuration;
    this->_random_state = configuration.random_seed;
    if (this->_random_state == 0U)
        this->_random_state = 1U;
    this->_now = 0U;
    this->_next_delivery_time = UINT64_MAX;
    this->_send_ordinal = 0U;
    this->_bandwidth_window_start = 0U;
    this->_bandwidth_window_bytes = 0U;
    result = this->_pending.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_script.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_pending.destroy();
        (void)this->_script.destroy();
        return (result);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    while (!this->_pending.empty())
        delete this->_pending.pop_front();
    const int32_t pending_result = this->_pending.destroy();
    const int32_t script_result = this->_script.destroy();
    this->_underlying = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (pending_result != FT_ERR_SUCCESS)
        return (pending_result);
    return (script_result);
}

int32_t networking_simulated_datagram_io::move(networking_simulated_datagram_io &other) noexcept
{
    int32_t result;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_underlying = other._underlying;
    this->_configuration = other._configuration;
    result = this->_pending.move(other._pending);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_script.move(other._script);
    if (result != FT_ERR_SUCCESS)
    {
        (void)other._pending.move(this->_pending);
        return (result);
    }
    this->_now = other._now;
    this->_next_delivery_time = other._next_delivery_time;
    this->_random_state = other._random_state;
    this->_send_ordinal = other._send_ordinal;
    this->_bandwidth_window_start = other._bandwidth_window_start;
    this->_bandwidth_window_bytes = other._bandwidth_window_bytes;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._underlying = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::clear_script() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_script.clear();
    return (FT_ERR_SUCCESS);
}

static ft_size_t lower_bound_script_action(const networking_simulator_script_entry *entries,
    ft_size_t count, uint64_t packet_ordinal) noexcept
{
    ft_size_t first = 0U;
    ft_size_t end = count;
    while (first < end)
    {
        const ft_size_t middle = first + (end - first) / 2U;
        if (entries[middle].packet_ordinal < packet_ordinal)
            first = middle + 1U;
        else
            end = middle;
    }
    return (first);
}

int32_t networking_simulated_datagram_io::add_script_action(
    uint64_t packet_ordinal, networking_simulator_script_action action,
    uint32_t delay_milliseconds) noexcept
{
    networking_simulator_script_entry entry;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (packet_ordinal == 0U
        || static_cast<uint8_t>(action)
            > static_cast<uint8_t>(networking_simulator_script_action::SCRIPT_MTU_DROP))
        return (FT_ERR_INVALID_ARGUMENT);
    entry.packet_ordinal = packet_ordinal;
    entry.action = action;
    entry.delay_milliseconds = delay_milliseconds;
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_SIMULATOR_QUEUE)
        != FT_FALSE)
        return (FT_ERR_NO_MEMORY);
    const ft_size_t count = this->_script.size();
    networking_simulator_script_entry *entries = this->_script.begin();
    if (count == 0U || entries[count - 1U].packet_ordinal < packet_ordinal)
        (void)this->_script.push_back(entry);
    else
    {
        const ft_size_t position = lower_bound_script_action(entries, count, packet_ordinal);
        // The first action registered for a packet ordinal takes precedence.
        if (entries[position].packet_ordinal == packet_ordinal)
            return (FT_ERR_SUCCESS);
        this->_script.insert(entries + position, entry);
    }
    if (this->_script.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

uint32_t networking_simulated_datagram_io::next_random() noexcept
{
    this->_random_state = this->_random_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (static_cast<uint32_t>(this->_random_state >> 32U));
}

ft_bool networking_simulated_datagram_io::should_drop() noexcept
{
    if ((this->next_random() % 1000000U)
        < this->_configuration.loss_parts_per_million)
        return (FT_TRUE);
    return (FT_FALSE);
}

uint64_t networking_simulated_datagram_io::calculate_delivery_time() noexcept
{
    uint32_t jitter;

    jitter = 0U;
    if (this->_configuration.jitter_milliseconds != 0U)
        jitter = static_cast<uint32_t>(this->next_random()
            % (static_cast<uint64_t>(this->_configuration.jitter_milliseconds) + 1U));
    return (this->_now + this->_configuration.fixed_latency_milliseconds + jitter);
}

int32_t networking_simulated_datagram_io::queue_datagram(pending_datagram *datagram) noexcept
{
    this->_pending.push_back(datagram);
    if (this->_pending.get_error() != FT_ERR_SUCCESS)
    {
        delete datagram;
        return (FT_ERR_NO_MEMORY);
    }
    if (datagram->delivery_time < this->_next_delivery_time)
        this->_next_delivery_time = datagram->delivery_time;
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::flush_ready_datagrams() noexcept
{
    if (this->_now < this->_next_delivery_time)
        return (FT_ERR_SUCCESS);
    // Scan ready packets in queue order, even when several deadlines have elapsed.
    const ft_size_t pending_count = this->_pending.size();
    this->_next_delivery_time = UINT64_MAX;
    ft_size_t index = 0U;
    while (index < pending_count)
    {
        pending_datagram *datagram = this->_pending.pop_front();
        if (datagram->delivery_time > this->_now)
        {
            if (this->queue_datagram(datagram) != FT_ERR_SUCCESS)
            {
                this->_next_delivery_time = 0U;
                return (FT_ERR_NO_MEMORY);
            }
            index += 1U;
            continue ;
        }
        if (this->_configuration.blackhole_outgoing == FT_FALSE
            && this->_underlying->send_datagram(datagram->destination,
                datagram->payload.begin(), datagram->payload.size()) != FT_ERR_SUCCESS)
        {
            delete datagram;
            this->_next_delivery_time = 0U;
            return (FT_ERR_SOCKET_SEND_FAILED);
        }
        delete datagram;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::send_datagram(
    const networking_message_endpoint &destination, const uint8_t *data,
    ft_size_t size) noexcept
{
    pending_datagram *datagram;
    pending_datagram *duplicate;
    ft_bool script_drop;
    ft_bool script_duplicate;
    ft_bool script_corruption;
    uint32_t script_delay;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_underlying == ft_nullptr || data == ft_nullptr || size == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_configuration.disconnect != FT_FALSE)
        return (FT_ERR_SOCKET_SEND_FAILED);
    if (size > this->_configuration.maximum_datagram_size)
        return (FT_ERR_OUT_OF_RANGE);
    if (this->_configuration.blackhole_outgoing != FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_pending.size() >= this->_configuration.maximum_pending_datagrams)
        return (FT_ERR_FULL);
    this->_send_ordinal += 1U;
    script_drop = FT_FALSE;
    script_duplicate = FT_FALSE;
    script_corruption = FT_FALSE;
    script_delay = 0U;
    const ft_size_t script_count = this->_script.size();
    if (script_count != 0U)
    {
        const networking_simulator_script_entry *entries = this->_script.begin();
        const ft_size_t position = lower_bound_script_action(entries, script_count,
            this->_send_ordinal);
        if (position < script_count && entries[position].packet_ordinal == this->_send_ordinal)
        {
            const networking_simulator_script_entry &entry = entries[position];
            if (entry.action == networking_simulator_script_action::SCRIPT_DROP)
                script_drop = FT_TRUE;
            else if (entry.action == networking_simulator_script_action::SCRIPT_DUPLICATE)
                script_duplicate = FT_TRUE;
            else if (entry.action == networking_simulator_script_action::SCRIPT_CORRUPT)
                script_corruption = FT_TRUE;
            else if (entry.action == networking_simulator_script_action::SCRIPT_DELAY)
                script_delay = entry.delay_milliseconds;
            else if (entry.action == networking_simulator_script_action::SCRIPT_DISCONNECT)
                return (FT_ERR_SOCKET_SEND_FAILED);
            else if (entry.action == networking_simulator_script_action::SCRIPT_MTU_DROP)
                return (FT_ERR_OUT_OF_RANGE);
        }
    }
    if (script_drop != FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_configuration.bandwidth_bytes_per_second != 0U)
    {
        uint64_t budget;
        uint64_t used;

        if (this->_now >= this->_bandwidth_window_start + 1000U)
        {
            this->_bandwidth_window_start = this->_now;
            this->_bandwidth_window_bytes = 0U;
        }
        budget = this->_configuration.bandwidth_bytes_per_second
            + this->_configuration.bandwidth_burst_bytes;
        if (budget < this->_configuration.bandwidth_bytes_per_second)
            budget = UINT64_MAX;
        used = this->_bandwidth_window_bytes;
        if (used > budget)
            used = budget;
        if (size > budget - used)
            return (FT_ERR_FULL);
        this->_bandwidth_window_bytes += size;
    }
    if (this->should_drop() != FT_FALSE)
        return (FT_ERR_SUCCESS);
    datagram = new (std::nothrow) pending_datagram();
    if (datagram == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (datagram->initialize() != FT_ERR_SUCCESS)
    {
        delete datagram;
        return (FT_ERR_NO_MEMORY);
    }
    datagram->destination = destination;
    datagram->payload.resize(size);
    if (datagram->payload.get_error() != FT_ERR_SUCCESS)
    {
        delete datagram;
        return (FT_ERR_NO_MEMORY);
    }
    ft_memcpy(&datagram->payload[0], data, size);
    if (script_corruption != FT_FALSE
        || (this->_configuration.corruption_parts_per_million != 0U
        && (this->next_random() % 1000000U)
            < this->_configuration.corruption_parts_per_million))
        datagram->payload[this->next_random() % datagram->payload.size()] ^= 0x01U;
    datagram->delivery_time = this->calculate_delivery_time();
    datagram->delivery_time += script_delay;
    if (this->_configuration.reorder_parts_per_million != 0U
        && (this->next_random() % 1000000U)
            < this->_configuration.reorder_parts_per_million)
        datagram->delivery_time += this->_configuration.fixed_latency_milliseconds + 1U;
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_SIMULATOR_QUEUE)
        != FT_FALSE)
    {
        delete datagram;
        return (FT_ERR_NO_MEMORY);
    }
    if (this->queue_datagram(datagram) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (script_duplicate != FT_FALSE
        || (this->_configuration.duplicate_parts_per_million != 0U
        && (this->next_random() % 1000000U)
            < this->_configuration.duplicate_parts_per_million))
    {
        duplicate = new (std::nothrow) pending_datagram();
        if (duplicate == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (duplicate->payload.initialize(datagram->payload) != FT_ERR_SUCCESS)
        {
            delete duplicate;
            return (FT_ERR_NO_MEMORY);
        }
        duplicate->destination = datagram->destination;
        duplicate->delivery_time = datagram->delivery_time + 1U;
        return (this->queue_datagram(duplicate));
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::receive_datagram(networking_message_endpoint &source,
    uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept
{
    int32_t flush_result;
    int32_t receive_result;
    networking_message_endpoint staged_source;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_underlying == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    if (data == ft_nullptr || received_size == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    *received_size = 0U;
    if (this->_configuration.disconnect != FT_FALSE
        || this->_configuration.blackhole_incoming != FT_FALSE)
        return (FT_ERR_EMPTY);
    flush_result = this->flush_ready_datagrams();
    if (flush_result != FT_ERR_SUCCESS)
        return (flush_result);
    ft_memset(&staged_source, 0, sizeof(staged_source));
    receive_result = this->_underlying->receive_datagram(staged_source,
        data, capacity, received_size);
    if (receive_result == FT_ERR_SUCCESS && *received_size > capacity)
    {
        *received_size = 0U;
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (receive_result == FT_ERR_SUCCESS)
        source = staged_source;
    else
        *received_size = 0U;
    return (receive_result);
}

uint64_t networking_simulated_datagram_io::now_milliseconds() const noexcept
{
    return (this->_now);
}

void networking_simulated_datagram_io::advance(uint64_t milliseconds) noexcept
{
    this->_now += milliseconds;
    return ;
}
