#ifndef PTHREAD_LOCK_TRACKING_HPP
# define PTHREAD_LOCK_TRACKING_HPP

#include <cstdlib>
#include <mutex>
#include "pthread.hpp"
#include "pt_buffer.hpp"

class pt_mutex;


typedef pt_buffer<const void *> pt_mutex_vector;
typedef pt_buffer<pt_thread_id_type> pt_thread_vector;

struct s_pt_lock_wait_snapshot
{
    const void *mutex_pointer;
    pt_thread_id_type owner_thread;
    pt_thread_id_type waiting_thread;
    int64_t wait_started_ms;
};

typedef pt_buffer<s_pt_lock_wait_snapshot> pt_lock_wait_snapshot_vector;

struct s_pt_thread_lock_info
{
    pt_thread_id_type thread_identifier;
    pt_mutex_vector owned_mutexes;
    const void *waiting_mutex;
    int64_t wait_started_ms;
};

struct s_pt_lock_tracking_thread_state
{
    pt_thread_id_type thread_identifier;
    pt_mutex_vector owned_mutexes;
    const void *waiting_mutex;
    int64_t wait_started_ms;
};

class pt_lock_tracking
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        static std::mutex *get_registry_mutex(void);
        static pt_buffer<s_pt_thread_lock_info> *get_thread_infos(int *error_code);
        static bool ensure_registry_mutex_initialised(int *error_code);

        static s_pt_thread_lock_info *find_thread_info
            (pt_thread_id_type thread_identifier, int *error_code);
        static s_pt_thread_lock_info *lookup_thread_info
            (pt_thread_id_type thread_identifier, int *error_code);
        static bool vector_contains_mutex(const pt_mutex_vector &mutexes,
                const void *mutex_pointer);
        static bool vector_contains_thread(const pt_thread_vector &thread_identifiers,
                pt_thread_id_type thread_identifier);
        static int detect_cycle(const s_pt_thread_lock_info *origin,
                const void *requested_mutex, pt_mutex_vector *visited_mutexes,
                pt_thread_vector *visited_threads, bool *cycle_detected);
        static int lock_registry_mutex(void);
        static int unlock_registry_mutex(void);

    public:
        pt_lock_tracking();
        ~pt_lock_tracking();

        static pt_mutex_vector get_owned_mutexes
            (pt_thread_id_type thread_identifier, int *error_code);
        static int notify_wait(pt_thread_id_type thread_identifier,
                const void *requested_mutex);
        static int notify_acquired(pt_thread_id_type thread_identifier,
                const void *mutex_pointer);
        static int notify_released(pt_thread_id_type thread_identifier,
                const void *mutex_pointer);
        static int notify_thread_exit(pt_thread_id_type thread_identifier);
        static int snapshot_waiters(pt_lock_wait_snapshot_vector &snapshot);
        static int get_thread_state(pt_thread_id_type thread_identifier,
                s_pt_lock_tracking_thread_state &state);
};

#ifdef LIBFT_TEST_BUILD
extern std::atomic<int> pt_lock_tracking_notify_acquired_override_error_code;
extern std::atomic<int> pt_lock_tracking_detect_cycle_override_error_code;
#endif

#endif
