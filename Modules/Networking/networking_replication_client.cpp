#include "networking_replication_client.hpp"

networking_replication_client::networking_replication_client() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _server_instance_id(0U), _session_id(0U), _revision_tracker(),
      _apply_budget(), _snapshot_callback(ft_nullptr),
      _block_delta_callback(ft_nullptr), _light_delta_callback(ft_nullptr),
      _callback_user_data(ft_nullptr)
{
    return ;
}

networking_replication_client::~networking_replication_client() noexcept
{
    if (this->destroy() != FT_ERR_SUCCESS)
        return ;
    return ;
}

int32_t networking_replication_client::initialize(uint64_t server_instance_id,
    uint64_t session_id, uint32_t maximum_messages,
    uint32_t maximum_payload_bytes, uint32_t maximum_operations) noexcept
{
    int32_t error_code;
    int32_t cleanup_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (server_instance_id == 0U || session_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->_revision_tracker.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = networking_replication_apply_budget_initialize(
            this->_apply_budget, maximum_messages, maximum_payload_bytes,
            maximum_operations);
    if (error_code != FT_ERR_SUCCESS)
    {
        cleanup_error = this->_revision_tracker.destroy();
        if (error_code == FT_ERR_SUCCESS && cleanup_error != FT_ERR_SUCCESS)
            error_code = cleanup_error;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_server_instance_id = server_instance_id;
    this->_session_id = session_id;
    this->_snapshot_callback = ft_nullptr;
    this->_block_delta_callback = ft_nullptr;
    this->_light_delta_callback = ft_nullptr;
    this->_callback_user_data = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_client::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    error_code = this->_revision_tracker.destroy();
    this->_server_instance_id = 0U;
    this->_session_id = 0U;
    this->_snapshot_callback = ft_nullptr;
    this->_block_delta_callback = ft_nullptr;
    this->_light_delta_callback = ft_nullptr;
    this->_callback_user_data = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (error_code);
}

int32_t networking_replication_client::set_callbacks(
    networking_replication_apply_callback snapshot,
    networking_replication_apply_callback block_delta,
    networking_replication_apply_callback light_delta,
    void *user_data) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (snapshot == ft_nullptr || block_delta == ft_nullptr
        || light_delta == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    this->_snapshot_callback = snapshot;
    this->_block_delta_callback = block_delta;
    this->_light_delta_callback = light_delta;
    this->_callback_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_client::reset_budget() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (networking_replication_apply_budget_reset(this->_apply_budget));
}

int32_t networking_replication_client::apply_payload(
    const ft_byte_buffer &payload, uint32_t operations,
    networking_replication_apply_callback callback) noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (payload.is_initialised() == FT_FALSE || operations == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (callback == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (payload.size() > NETWORKING_REPLICATION_MAX_PAYLOAD
        || payload.size() > UINT32_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    if (networking_replication_apply_budget_can_consume(
            this->_apply_budget, static_cast<uint32_t>(payload.size()),
            operations) == FT_FALSE)
        return (FT_ERR_FULL);
    error_code = callback(payload, this->_callback_user_data);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (networking_replication_apply_budget_consume(this->_apply_budget,
        static_cast<uint32_t>(payload.size()), operations));
}

int32_t networking_replication_client::apply_snapshot(
    uint64_t block_revision, uint64_t light_revision,
    uint64_t snapshot_generation, const ft_byte_buffer &payload,
    uint32_t operations) noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_revision_tracker.can_accept_snapshot(block_revision,
        light_revision, snapshot_generation) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->apply_payload(payload, operations,
        this->_snapshot_callback);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_revision_tracker.accept_snapshot(block_revision,
        light_revision, snapshot_generation));
}

int32_t networking_replication_client::apply_block_delta(
    uint64_t base_revision, uint64_t final_revision,
    const ft_byte_buffer &payload, uint32_t operations) noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_revision_tracker.can_accept_block_delta(base_revision,
        final_revision) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = this->apply_payload(payload, operations,
        this->_block_delta_callback);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_revision_tracker.accept_block_delta(base_revision,
        final_revision));
}

int32_t networking_replication_client::apply_light_delta(
    uint64_t base_revision, uint64_t final_revision,
    uint64_t source_block_revision, const ft_byte_buffer &payload,
    uint32_t operations) noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_revision_tracker.can_accept_light_delta(base_revision,
        final_revision, source_block_revision) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = this->apply_payload(payload, operations,
        this->_light_delta_callback);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_revision_tracker.accept_light_delta(base_revision,
        final_revision, source_block_revision));
}

uint64_t networking_replication_client::get_block_revision() const noexcept
{
    return (this->_revision_tracker.get_block_revision());
}

uint64_t networking_replication_client::get_light_revision() const noexcept
{
    return (this->_revision_tracker.get_light_revision());
}

uint64_t networking_replication_client::get_snapshot_generation() const noexcept
{
    return (this->_revision_tracker.get_snapshot_generation());
}

ft_bool networking_replication_client::is_snapshot_ready() const noexcept
{
    return (this->_revision_tracker.is_snapshot_ready());
}
