#include "../PThread/pthread_internal.hpp"
#include "game_progress_tracker.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_achievement.hpp"
#include "game_quest.hpp"

thread_local int32_t game_progress_tracker::_last_error = FT_ERR_SUCCESS;

int32_t game_progress_tracker::set_error(int32_t error_code) noexcept
{
    game_progress_tracker::_last_error = error_code;
    return (error_code);
}

int32_t game_progress_tracker::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_progress_tracker::get_error");
    return (game_progress_tracker::_last_error);
}

const char *game_progress_tracker::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_progress_tracker::get_error_str");
    return (ft_strerror(game_progress_tracker::_last_error));
}

game_progress_tracker::game_progress_tracker() noexcept
    : _achievements(), _quests(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_progress_tracker::~game_progress_tracker() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_progress_tracker::initialize() noexcept
{
    int32_t error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_progress_tracker::initialize",
            "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    error = this->_achievements.initialize();
    if (error != FT_ERR_SUCCESS)
    {
        this->set_error(error);
        return (error);
    }
    error = this->_quests.initialize();
    if (error != FT_ERR_SUCCESS)
    {
        (void)this->_achievements.destroy();
        this->set_error(error);
        return (error);
    }
    this->_achievements.clear();
    this->_quests.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_progress_tracker::initialize(const game_progress_tracker &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_progress_tracker::initialize(copy)",
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
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_achievements.copy_from(other._achievements);
        if (result == FT_ERR_SUCCESS)
            result = this->_quests.copy_from(other._quests);
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    }
    return (this->set_error(result));
}

int32_t game_progress_tracker::initialize(game_progress_tracker &&other) noexcept
{
    return (this->move(other));
}

int32_t game_progress_tracker::move(game_progress_tracker &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_progress_tracker::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }

    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    ft_bool achievements_locked = FT_FALSE;
    ft_bool quests_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._achievements.lock(&achievements_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._quests.lock(&quests_locked);
    if (result == FT_ERR_SUCCESS)
        result = this->_achievements.move(other._achievements);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_quests.move(other._quests);
        if (result != FT_ERR_SUCCESS)
            (void)other._achievements.move(this->_achievements);
    }
    if (quests_locked)
        (void)other._quests.unlock(quests_locked);
    if (achievements_locked)
        (void)other._achievements.unlock(achievements_locked);
    if (source_locked)
        (void)other.unlock_internal(source_locked);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_achievements.destroy();
        (void)this->_quests.destroy();
        return (this->set_error(result));
    }

    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = other.destroy();
    if (result == FT_ERR_SUCCESS)
        this->set_error(source_error);
    else
        this->set_error(result);
    return (result);
}

int32_t game_progress_tracker::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t result = this->disable_thread_safety();
    const int32_t achievement_error = this->_achievements.destroy();
    if (result == FT_ERR_SUCCESS)
        result = achievement_error;
    const int32_t quest_error = this->_quests.destroy();
    if (result == FT_ERR_SUCCESS)
        result = quest_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(result));
}

int32_t game_progress_tracker::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::enable_thread_safety");
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

int32_t game_progress_tracker::disable_thread_safety() noexcept
{
    pt_recursive_mutex *old_mutex;
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_progress_tracker::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_progress_tracker::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_progress_tracker::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }

    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_progress_tracker::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::lock");
    return (this->lock_internal(lock_acquired));
}

void game_progress_tracker::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_map<int32_t, game_achievement> &game_progress_tracker::get_achievements() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::get_achievements");
    return (this->_achievements);
}

const ft_map<int32_t, game_achievement> &game_progress_tracker::get_achievements() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::get_achievements const");
    return (this->_achievements);
}

ft_map<int32_t, game_quest> &game_progress_tracker::get_quests() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::get_quests");
    return (this->_quests);
}

const ft_map<int32_t, game_quest> &game_progress_tracker::get_quests() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::get_quests const");
    return (this->_quests);
}

void game_progress_tracker::set_achievements(
    const ft_map<int32_t, game_achievement> &achievements) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::set_achievements");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_achievements.copy_from(achievements);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

void game_progress_tracker::set_quests(const ft_map<int32_t, game_quest> &quests) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::set_quests");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_quests.copy_from(quests);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_progress_tracker::register_achievement(
    const game_achievement &achievement) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::register_achievement");
    int32_t lock_error;

    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_achievements.insert(achievement.get_id(), achievement);
    const int32_t result = this->_achievements.get_error();
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_progress_tracker::register_quest(const game_quest &quest) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_progress_tracker::register_quest");
    int32_t lock_error;

    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_quests.insert(quest.get_id(), quest);
    const int32_t result = this->_quests.get_error();
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

template <typename Map, typename Visit>
int32_t game_progress_tracker::visit_live(Map &map, int32_t id, Visit visit) const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (this->set_error(map.visit_value(id, visit)));
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = map.visit_value(id, visit);
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_progress_tracker::update_goal_target(int32_t achievement_id, int32_t goal_id,
    int32_t target) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::update_goal_target");
    return (this->visit_live(this->_achievements, achievement_id,
        [goal_id, target](game_achievement &achievement) noexcept
        {
            achievement.set_goal(goal_id, target);
            return (achievement.get_error());
        }));
}

int32_t game_progress_tracker::update_goal_progress(int32_t achievement_id, int32_t goal_id,
    int32_t progress) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::update_goal_progress");
    return (this->visit_live(this->_achievements, achievement_id,
        [goal_id, progress](game_achievement &achievement) noexcept
        {
            achievement.set_progress(goal_id, progress);
            return (achievement.get_error());
        }));
}

int32_t game_progress_tracker::add_goal_progress(int32_t achievement_id, int32_t goal_id,
    int32_t value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::add_goal_progress");
    return (this->visit_live(this->_achievements, achievement_id,
        [goal_id, value](game_achievement &achievement) noexcept
        {
            achievement.add_progress(goal_id, value);
            return (achievement.get_error());
        }));
}

ft_bool game_progress_tracker::is_achievement_complete(int32_t achievement_id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::is_achievement_complete");
    ft_bool completed = FT_FALSE;
    (void)this->visit_live(this->_achievements, achievement_id,
        [&completed](const game_achievement &achievement) noexcept
        {
            completed = achievement.is_complete();
            return (achievement.get_error());
        });
    return (completed);
}

int32_t game_progress_tracker::set_quest_phase(int32_t quest_id, int32_t phase) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::set_quest_phase");
    return (this->visit_live(this->_quests, quest_id,
        [phase](game_quest &quest) noexcept
        {
            quest.set_current_phase(phase);
            return (quest.get_error());
        }));
}

int32_t game_progress_tracker::advance_quest_phase(int32_t quest_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::advance_quest_phase");
    return (this->visit_live(this->_quests, quest_id,
        [](game_quest &quest) noexcept
        {
            return (quest.advance_phase_internal(FT_TRUE));
        }));
}

ft_bool game_progress_tracker::is_quest_complete(int32_t quest_id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_progress_tracker::is_quest_complete");
    ft_bool completed = FT_FALSE;
    (void)this->visit_live(this->_quests, quest_id,
        [&completed](const game_quest &quest) noexcept
        {
            completed = quest.is_complete();
            return (quest.get_error());
        });
    return (completed);
}
