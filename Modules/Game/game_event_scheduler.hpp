#ifndef GAME_EVENT_SCHEDULER_HPP
# define GAME_EVENT_SCHEDULER_HPP

#include "game_event.hpp"
#include "../Template/priority_queue.hpp"
#include "../Template/vector.hpp"
#include "../Template/shared_ptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include "../Time/time.hpp"
#include <cstddef>
#include <stdint.h>

struct json_group;
class game_world;

typedef struct s_event_scheduler_profile
{
    int64_t update_count;
    int64_t events_processed;
    int64_t events_rescheduled;
    ft_size_t max_queue_depth;
    ft_size_t max_ready_batch;
    int64_t total_processing_ns;
    int64_t last_update_processing_ns;
    int32_t last_error_code;
}   t_event_scheduler_profile;

struct game_event_compare_ptr
{
    ft_bool operator()(const ft_sharedptr<game_event> *left, const ft_sharedptr<game_event> *right) const noexcept;
};

class game_event_scheduler
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        mutable ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> _events;
        static thread_local int32_t _last_error;
        mutable pt_recursive_mutex *_mutex;
        mutable ft_bool _profiling_enabled;
        mutable t_event_scheduler_profile _profile;
        mutable ft_vector<ft_sharedptr<game_event> *> _ready_cache;
        uint8_t _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        void reset_profile_locked() const noexcept;
        void record_profile_locked(ft_size_t ready_count,
                ft_size_t rescheduled_count,
                ft_size_t queue_depth,
                int64_t duration_ns) const noexcept;
        void finalize_update(ft_vector<ft_sharedptr<game_event> *> &events,
                ft_size_t ready_count,
                ft_size_t rescheduled_count,
                ft_size_t queue_depth,
                ft_bool profiling_active,
                ft_bool start_valid,
                t_high_resolution_time_point start_time) noexcept;

    public:
        game_event_scheduler() noexcept;
        game_event_scheduler(const game_event_scheduler &other) noexcept = delete;
        game_event_scheduler(game_event_scheduler &&other) noexcept = delete;
        ~game_event_scheduler() noexcept;
        game_event_scheduler &operator=(const game_event_scheduler &other) noexcept = delete;
        game_event_scheduler &operator=(game_event_scheduler &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_event_scheduler &other) noexcept;
        void schedule_event(const ft_sharedptr<game_event> &event) noexcept;
        void cancel_event(int32_t id) noexcept;
        void reschedule_event(int32_t id, int32_t new_duration) noexcept;
        void update_events(ft_sharedptr<game_world> &world, int32_t ticks, const char *log_file_path = ft_nullptr, ft_string *log_buffer = ft_nullptr) noexcept;

        void enable_profiling(ft_bool enabled) noexcept;
        ft_bool profiling_enabled() const noexcept;
        void reset_profile() noexcept;
        void snapshot_profile(t_event_scheduler_profile &out) const noexcept;

        void dump_events(ft_vector<ft_sharedptr<game_event> > &out) const noexcept;
        ft_size_t size() const noexcept;
        void clear() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

int32_t log_event_to_file(const game_event &event, const char *file_path) noexcept;
void log_event_to_buffer(const game_event &event, ft_string &buffer) noexcept;

json_group *serialize_event_scheduler(const ft_sharedptr<game_event_scheduler> &scheduler);
int32_t deserialize_event_scheduler(ft_sharedptr<game_event_scheduler> &scheduler, json_group *group);

#endif
