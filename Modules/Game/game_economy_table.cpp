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
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_economy_table::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));

    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_price_definitions.copy_from(other._price_definitions);
        if (result == FT_ERR_SUCCESS)
            result = this->_rarity_bands.copy_from(other._rarity_bands);
        if (result == FT_ERR_SUCCESS)
            result = this->_vendor_profiles.copy_from(other._vendor_profiles);
        if (result == FT_ERR_SUCCESS)
            result = this->_currency_rates.copy_from(other._currency_rates);
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::initialize(game_economy_table &&other) noexcept
{
    return (this->move(other));
}

int32_t game_economy_table::move(game_economy_table &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_economy_table::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));

    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    ft_bool prices_locked = FT_FALSE;
    ft_bool rarities_locked = FT_FALSE;
    ft_bool vendors_locked = FT_FALSE;
    ft_bool currencies_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._price_definitions.lock(&prices_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._rarity_bands.lock(&rarities_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._vendor_profiles.lock(&vendors_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._currency_rates.lock(&currencies_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_price_definitions.move(other._price_definitions);
        const ft_bool prices_moved = result == FT_ERR_SUCCESS;
        if (prices_moved)
            result = this->_rarity_bands.move(other._rarity_bands);
        const ft_bool rarities_moved = result == FT_ERR_SUCCESS;
        if (rarities_moved)
            result = this->_vendor_profiles.move(other._vendor_profiles);
        const ft_bool vendors_moved = result == FT_ERR_SUCCESS;
        if (vendors_moved)
            result = this->_currency_rates.move(other._currency_rates);
        if (result != FT_ERR_SUCCESS)
        {
            // Return transferred storage before releasing the source locks.
            if (vendors_moved)
                (void)other._vendor_profiles.move(this->_vendor_profiles);
            if (rarities_moved)
                (void)other._rarity_bands.move(this->_rarity_bands);
            if (prices_moved)
                (void)other._price_definitions.move(this->_price_definitions);
        }
    }
    if (currencies_locked)
        (void)other._currency_rates.unlock(currencies_locked);
    if (vendors_locked)
        (void)other._vendor_profiles.unlock(vendors_locked);
    if (rarities_locked)
        (void)other._rarity_bands.unlock(rarities_locked);
    if (prices_locked)
        (void)other._price_definitions.unlock(prices_locked);
    (void)other.unlock_internal(source_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (result == FT_ERR_SUCCESS)
        result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_economy_table::destroy() noexcept
{
    int32_t first_error;
    int32_t map_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    first_error = this->disable_thread_safety();
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
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(first_error));
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

template <typename Operation>
int32_t game_economy_table::with_table_lock(Operation operation) const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (this->set_error(operation()));
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = operation();
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

void game_economy_table::set_price_definitions(
    const ft_map<int32_t, game_price_definition> &price_definitions) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::set_price_definitions");
    (void)this->with_table_lock([this, &price_definitions]() noexcept
    {
        return (this->_price_definitions.copy_from(price_definitions));
    });
    return ;
}

void game_economy_table::set_rarity_bands(
    const ft_map<int32_t, game_rarity_band> &rarity_bands) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::set_rarity_bands");
    (void)this->with_table_lock([this, &rarity_bands]() noexcept
    {
        return (this->_rarity_bands.copy_from(rarity_bands));
    });
    return ;
}

void game_economy_table::set_vendor_profiles(
    const ft_map<int32_t, game_vendor_profile> &vendor_profiles) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::set_vendor_profiles");
    (void)this->with_table_lock([this, &vendor_profiles]() noexcept
    {
        return (this->_vendor_profiles.copy_from(vendor_profiles));
    });
    return ;
}

void game_economy_table::set_currency_rates(
    const ft_map<int32_t, game_currency_rate> &currency_rates) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::set_currency_rates");
    (void)this->with_table_lock([this, &currency_rates]() noexcept
    {
        return (this->_currency_rates.copy_from(currency_rates));
    });
    return ;
}

int32_t game_economy_table::register_price_definition(const game_price_definition &definition) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::register_price_definition");
    return (this->with_table_lock([this, &definition]() noexcept
    {
        this->_price_definitions.insert(definition.get_item_id(), definition);
        return (this->_price_definitions.get_error());
    }));
}

int32_t game_economy_table::register_rarity_band(const game_rarity_band &band) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::register_rarity_band");
    return (this->with_table_lock([this, &band]() noexcept
    {
        game_rarity_band snapshot;
        int32_t result = snapshot.initialize();
        if (result == FT_ERR_SUCCESS)
            result = snapshot.copy_values(band);
        if (result != FT_ERR_SUCCESS)
            return (result);
        this->_rarity_bands.insert(snapshot._rarity, snapshot);
        return (this->_rarity_bands.get_error());
    }));
}

int32_t game_economy_table::register_vendor_profile(const game_vendor_profile &profile) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::register_vendor_profile");
    return (this->with_table_lock([this, &profile]() noexcept
    {
        errno_abort_if_uninitialised_or_destroyed(profile._initialised_state,
            "game_economy_table::register_vendor_profile source");
        game_vendor_profile snapshot;
        const int32_t result = snapshot.initialize(profile);
        if (result != FT_ERR_SUCCESS)
            return (result);
        this->_vendor_profiles.insert(snapshot._vendor_id, snapshot);
        return (this->_vendor_profiles.get_error());
    }));
}

int32_t game_economy_table::register_currency_rate(const game_currency_rate &rate) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::register_currency_rate");
    return (this->with_table_lock([this, &rate]() noexcept
    {
        game_currency_rate snapshot;
        int32_t result = snapshot.initialize();
        if (result == FT_ERR_SUCCESS)
            result = snapshot.copy_values(rate);
        if (result != FT_ERR_SUCCESS)
            return (result);
        this->_currency_rates.insert(snapshot._currency_id, snapshot);
        return (this->_currency_rates.get_error());
    }));
}

int32_t game_economy_table::fetch_price_definition(int32_t item_id,
    game_price_definition &definition) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::fetch_price_definition");
    return (this->with_table_lock([this, item_id, &definition]() noexcept
    {
        return (this->_price_definitions.visit_value(item_id,
            [&definition](const game_price_definition &stored) noexcept
            {
                return (definition.copy_values(stored));
            }));
    }));
}

int32_t game_economy_table::fetch_rarity_band(int32_t rarity,
    game_rarity_band &band) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::fetch_rarity_band");
    return (this->with_table_lock([this, rarity, &band]() noexcept
    {
        return (this->_rarity_bands.visit_value(rarity,
            [&band](const game_rarity_band &stored) noexcept
            {
                return (band.copy_values(stored));
            }));
    }));
}

int32_t game_economy_table::fetch_vendor_profile(int32_t vendor_id,
    game_vendor_profile &profile) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::fetch_vendor_profile");
    return (this->with_table_lock([this, vendor_id, &profile]() noexcept
    {
        return (this->_vendor_profiles.visit_value(vendor_id,
            [&profile](const game_vendor_profile &stored) noexcept
            {
                return (profile.copy_values(stored));
            }));
    }));
}

int32_t game_economy_table::fetch_currency_rate(int32_t currency_id,
    game_currency_rate &rate) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_economy_table::fetch_currency_rate");
    return (this->with_table_lock([this, currency_id, &rate]() noexcept
    {
        return (this->_currency_rates.visit_value(currency_id,
            [&rate](const game_currency_rate &stored) noexcept
            {
                return (rate.copy_values(stored));
            }));
    }));
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
