#include "networking_simulator.hpp"
#ifdef LIBFT_TEST_BUILD
# include "../../Test/Test/networking_test_hooks.hpp"
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
      _configuration(), _pending(), _script(), _now(0U), _random_state(1U),
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
    this->_send_ordinal = 0U;
    this->_bandwidth_window_start = 0U;
    this->_bandwidth_window_bytes = 0U;
    if (this->_pending.initialize() != FT_ERR_SUCCESS
        || this->_script.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_pending.destroy();
        (void)this->_script.destroy();
        return (FT_ERR_INITIALIZATION_FAILED);
    }
    while (!this->_pending.empty())
        delete this->_pending.pop_front();
    this->_script.clear();
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
    (void)this->_script.destroy();
    this->_underlying = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::move(networking_simulated_datagram_io &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_underlying = other._underlying;
    this->_configuration = other._configuration;
    if (this->_pending.move(other._pending) != FT_ERR_SUCCESS
        || this->_script.move(other._script) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    this->_now = other._now;
    this->_random_state = other._random_state;
    this->_send_ordinal = other._send_ordinal;
    this->_bandwidth_window_start = other._bandwidth_window_start;
    this->_bandwidth_window_bytes = other._bandwidth_window_bytes;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._underlying = ft_nullptr;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::clear_script() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_script.clear();
    return (FT_ERR_SUCCESS);
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
    if (this->_script.push_back(entry) != FT_ERR_SUCCESS)
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
        jitter = this->next_random() % (this->_configuration.jitter_milliseconds + 1U);
    return (this->_now + this->_configuration.fixed_latency_milliseconds + jitter);
}

int32_t networking_simulated_datagram_io::flush_ready_datagrams() noexcept
{
    ft_size_t pending_count;
    ft_size_t index;

    pending_count = this->_pending.size();
    index = 0U;
    while (index < pending_count)
    {
        pending_datagram *datagram = this->_pending.front();
        this->_pending.pop_front();
        if (datagram->delivery_time > this->_now)
        {
            this->_pending.push_back(datagram);
            if (this->_pending.get_error() != FT_ERR_SUCCESS)
            {
                delete datagram;
                return (FT_ERR_NO_MEMORY);
            }
            index += 1U;
            continue ;
        }
        const uint8_t *payload_data = ft_nullptr;
        if (datagram->payload.size() != 0U)
            payload_data = &datagram->payload[0];
        if (this->_configuration.blackhole_outgoing == FT_FALSE
            && this->_underlying->send_datagram(datagram->destination,
                payload_data, datagram->payload.size()) != FT_ERR_SUCCESS)
        {
            delete datagram;
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
    ft_size_t script_index;

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
    script_index = 0U;
    while (script_index < this->_script.size())
    {
        if (this->_script[script_index].packet_ordinal == this->_send_ordinal)
        {
            if (this->_script[script_index].action
                == networking_simulator_script_action::SCRIPT_DROP)
                script_drop = FT_TRUE;
            else if (this->_script[script_index].action
                == networking_simulator_script_action::SCRIPT_DUPLICATE)
                script_duplicate = FT_TRUE;
            else if (this->_script[script_index].action
                == networking_simulator_script_action::SCRIPT_CORRUPT)
                script_corruption = FT_TRUE;
            else if (this->_script[script_index].action
                == networking_simulator_script_action::SCRIPT_DELAY)
                script_delay = this->_script[script_index].delay_milliseconds;
            else if (this->_script[script_index].action
                == networking_simulator_script_action::SCRIPT_DISCONNECT)
                return (FT_ERR_SOCKET_SEND_FAILED);
            else if (this->_script[script_index].action
                == networking_simulator_script_action::SCRIPT_MTU_DROP)
                return (FT_ERR_OUT_OF_RANGE);
            break ;
        }
        script_index += 1U;
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
    this->_pending.push_back(datagram);
    if (this->_pending.get_error() != FT_ERR_SUCCESS)
    {
        delete datagram;
        return (FT_ERR_NO_MEMORY);
    }
    if (script_duplicate != FT_FALSE
        || (this->_configuration.duplicate_parts_per_million != 0U
        && (this->next_random() % 1000000U)
            < this->_configuration.duplicate_parts_per_million))
    {
        duplicate = new (std::nothrow) pending_datagram();
        if (duplicate == ft_nullptr)
        {
            delete duplicate;
            return (FT_ERR_NO_MEMORY);
        }
        if (duplicate->payload.initialize(datagram->payload) != FT_ERR_SUCCESS)
        {
            delete duplicate;
            return (FT_ERR_NO_MEMORY);
        }
        duplicate->destination = datagram->destination;
        duplicate->delivery_time = datagram->delivery_time + 1U;
        this->_pending.push_back(duplicate);
        if (this->_pending.get_error() != FT_ERR_SUCCESS)
        {
            delete duplicate;
            return (FT_ERR_NO_MEMORY);
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_simulated_datagram_io::receive_datagram(networking_message_endpoint &source,
    uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept
{
    int32_t flush_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_underlying == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_configuration.disconnect != FT_FALSE
        || this->_configuration.blackhole_incoming != FT_FALSE)
        return (FT_ERR_EMPTY);
    flush_result = this->flush_ready_datagrams();
    if (flush_result != FT_ERR_SUCCESS)
        return (flush_result);
    return (this->_underlying->receive_datagram(source, data, capacity, received_size));
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
