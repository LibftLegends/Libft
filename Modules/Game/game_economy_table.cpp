#include "../PThread/pthread_internal.hpp"
#include "game_economy_table.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "game_currency_rate.hpp"
#include "game_price_definition.hpp"
#include "game_rarity_band.hpp"
#include "game_vendor_profile.hpp"

thread_local int32_t game_economy_table::_last_error = FT_ERR_SUCCESS;

game_economy_table::game_economy_table() noexcept
    : _price_definitions(), _rarity_bands(), _vendor_profiles(), _currency_rates(),
      _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_economy_table::~game_economy_table() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_economy_table::set_error(int32_t error_code) noexcept
{
    game_economy_table::_last_error = error_code;
    return (error_code);
}

int32_t game_economy_table::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_economy_table::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    int32_t error;
    error = this->_price_definitions.initialize();
    if (error)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(error);
        return (error);
    }
    error = this->_rarity_bands.initialize();
    if (error)
    {
        (void)this->_price_definitions.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(error);
        return (error);
    }
    error = this->_vendor_profiles.initialize();
    if (error)
    {
        (void)this->_rarity_bands.destroy();
        (void)this->_price_definitions.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(error);
        return (error);
    }
    error = this->_currency_rates.initialize();
    if (error)
    {
        (void)this->_vendor_profiles.destroy();
        (void)this->_rarity_bands.destroy();
        (void)this->_price_definitions.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(error);
        return (error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::initialize(const game_economy_table &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    ft_size_t count;
    ft_size_t index;
    const Pair<int32_t, game_price_definition> *price_entry;
    const Pair<int32_t, game_price_definition> *price_end;
    const Pair<int32_t, game_rarity_band> *rarity_entry;
    const Pair<int32_t, game_rarity_band> *rarity_end;
    const Pair<int32_t, game_vendor_profile> *vendor_entry;
    const Pair<int32_t, game_vendor_profile> *vendor_end;
    const Pair<int32_t, game_currency_rate> *currency_entry;
    const Pair<int32_t, game_currency_rate> *currency_end;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_economy_table::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->set_error(initialize_error);
            return (initialize_error);
        }
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    count = other._price_definitions.size();
    price_end = other._price_definitions.end();
    price_entry = price_end - count;
    index = 0;
    while (index < count)
    {
        this->_price_definitions.insert(price_entry->key, price_entry->value);
        price_entry++;
        index += 1;
    }
    count = other._rarity_bands.size();
    rarity_end = other._rarity_bands.end();
    rarity_entry = rarity_end - count;
    index = 0;
    while (index < count)
    {
        this->_rarity_bands.insert(rarity_entry->key, rarity_entry->value);
        rarity_entry++;
        index += 1;
    }
    count = other._vendor_profiles.size();
    vendor_end = other._vendor_profiles.end();
    vendor_entry = vendor_end - count;
    index = 0;
    while (index < count)
    {
        this->_vendor_profiles.insert(vendor_entry->key, vendor_entry->value);
        vendor_entry++;
        index += 1;
    }
    count = other._currency_rates.size();
    currency_end = other._currency_rates.end();
    currency_entry = currency_end - count;
    index = 0;
    while (index < count)
    {
        this->_currency_rates.insert(currency_entry->key, currency_entry->value);
        currency_entry++;
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::initialize(game_economy_table &&other) noexcept
{
    int32_t initialize_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_economy_table::initialize(move)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize(static_cast<const game_economy_table &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::move(game_economy_table &other) noexcept
{
    int32_t initialize_error;

    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_economy_table::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->initialize(static_cast<const game_economy_table &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::destroy() noexcept
{
    int32_t first_error;
    int32_t map_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    first_error = FT_ERR_SUCCESS;
    map_error = this->_price_definitions.destroy();
    if (map_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = map_error;
    map_error = this->_rarity_bands.destroy();
    if (map_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = map_error;
    map_error = this->_vendor_profiles.destroy();
    if (map_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = map_error;
    map_error = this->_currency_rates.destroy();
    if (map_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = map_error;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(static_cast<uint32_t>(first_error));
    return (first_error);
}

int32_t game_economy_table::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_economy_table::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_economy_table::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr && this->_mutex != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::lock");
    int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_economy_table::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_map<int32_t, game_price_definition> &game_economy_table::get_price_definitions() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_price_definitions");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_price_definitions);
}

const ft_map<int32_t, game_price_definition> &game_economy_table::get_price_definitions() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_price_definitions const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_price_definitions);
}

ft_map<int32_t, game_rarity_band> &game_economy_table::get_rarity_bands() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_rarity_bands");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_rarity_bands);
}

const ft_map<int32_t, game_rarity_band> &game_economy_table::get_rarity_bands() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_rarity_bands const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_rarity_bands);
}

ft_map<int32_t, game_vendor_profile> &game_economy_table::get_vendor_profiles() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_vendor_profiles");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_vendor_profiles);
}

const ft_map<int32_t, game_vendor_profile> &game_economy_table::get_vendor_profiles() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_vendor_profiles const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_vendor_profiles);
}

ft_map<int32_t, game_currency_rate> &game_economy_table::get_currency_rates() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_currency_rates");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_currency_rates);
}

const ft_map<int32_t, game_currency_rate> &game_economy_table::get_currency_rates() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::get_currency_rates const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_currency_rates);
}

void game_economy_table::set_price_definitions(
    const ft_map<int32_t, game_price_definition> &price_definitions) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::set_price_definitions");
    this->_price_definitions.clear();
    {
        ft_size_t count;
        ft_size_t index;
        const Pair<int32_t, game_price_definition> *entry;
        const Pair<int32_t, game_price_definition> *entry_end;

        count = price_definitions.size();
        entry_end = price_definitions.end();
        entry = entry_end - count;
        index = 0;
        while (index < count)
        {
            this->_price_definitions.insert(entry->key, entry->value);
            entry++;
            index += 1;
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_economy_table::set_rarity_bands(
    const ft_map<int32_t, game_rarity_band> &rarity_bands) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::set_rarity_bands");
    this->_rarity_bands.clear();
    {
        ft_size_t count;
        ft_size_t index;
        const Pair<int32_t, game_rarity_band> *entry;
        const Pair<int32_t, game_rarity_band> *entry_end;

        count = rarity_bands.size();
        entry_end = rarity_bands.end();
        entry = entry_end - count;
        index = 0;
        while (index < count)
        {
            this->_rarity_bands.insert(entry->key, entry->value);
            entry++;
            index += 1;
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_economy_table::set_vendor_profiles(
    const ft_map<int32_t, game_vendor_profile> &vendor_profiles) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::set_vendor_profiles");
    this->_vendor_profiles.clear();
    {
        ft_size_t count;
        ft_size_t index;
        const Pair<int32_t, game_vendor_profile> *entry;
        const Pair<int32_t, game_vendor_profile> *entry_end;

        count = vendor_profiles.size();
        entry_end = vendor_profiles.end();
        entry = entry_end - count;
        index = 0;
        while (index < count)
        {
            this->_vendor_profiles.insert(entry->key, entry->value);
            entry++;
            index += 1;
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_economy_table::set_currency_rates(
    const ft_map<int32_t, game_currency_rate> &currency_rates) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::set_currency_rates");
    this->_currency_rates.clear();
    {
        ft_size_t count;
        ft_size_t index;
        const Pair<int32_t, game_currency_rate> *entry;
        const Pair<int32_t, game_currency_rate> *entry_end;

        count = currency_rates.size();
        entry_end = currency_rates.end();
        entry = entry_end - count;
        index = 0;
        while (index < count)
        {
            this->_currency_rates.insert(entry->key, entry->value);
            entry++;
            index += 1;
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_economy_table::register_price_definition(
    const game_price_definition &definition) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::register_price_definition");
    this->_price_definitions.insert(definition.get_item_id(), definition);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::register_rarity_band(const game_rarity_band &band) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::register_rarity_band");
    this->_rarity_bands.insert(band.get_rarity(), band);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::register_vendor_profile(
    const game_vendor_profile &profile) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::register_vendor_profile");
    this->_vendor_profiles.insert(profile.get_vendor_id(), profile);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::register_currency_rate(const game_currency_rate &rate) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::register_currency_rate");
    this->_currency_rates.insert(rate.get_currency_id(), rate);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::fetch_price_definition(int32_t item_id,
    game_price_definition &definition) const noexcept
{
    const Pair<int32_t, game_price_definition> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::fetch_price_definition");
    entry = this->_price_definitions.find(item_id);
    if (entry == this->_price_definitions.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    definition.set_item_id(entry->value.get_item_id());
    definition.set_rarity(entry->value.get_rarity());
    definition.set_base_value(entry->value.get_base_value());
    definition.set_minimum_value(entry->value.get_minimum_value());
    definition.set_maximum_value(entry->value.get_maximum_value());
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::fetch_rarity_band(int32_t rarity,
    game_rarity_band &band) const noexcept
{
    const Pair<int32_t, game_rarity_band> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::fetch_rarity_band");
    entry = this->_rarity_bands.find(rarity);
    if (entry == this->_rarity_bands.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    band.set_rarity(entry->value.get_rarity());
    band.set_value_multiplier(entry->value.get_value_multiplier());
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::fetch_vendor_profile(int32_t vendor_id,
    game_vendor_profile &profile) const noexcept
{
    const Pair<int32_t, game_vendor_profile> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::fetch_vendor_profile");
    entry = this->_vendor_profiles.find(vendor_id);
    if (entry == this->_vendor_profiles.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    profile.initialize(entry->value);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::fetch_currency_rate(int32_t currency_id,
    game_currency_rate &rate) const noexcept
{
    const Pair<int32_t, game_currency_rate> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_economy_table::fetch_currency_rate");
    entry = this->_currency_rates.find(currency_id);
    if (entry == this->_currency_rates.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    rate.set_currency_id(entry->value.get_currency_id());
    rate.set_rate_to_base(entry->value.get_rate_to_base());
    rate.set_display_precision(entry->value.get_display_precision());
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}


int32_t game_economy_table::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_economy_table::get_error");
    return (game_economy_table::_last_error);
}

const char *game_economy_table::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_economy_table::get_error_str");
    return (ft_strerror(this->get_error()));
}
