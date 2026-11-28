#ifndef GAME_HOOKS_HPP
# define GAME_HOOKS_HPP

#include "game_character.hpp"
#include "game_item.hpp"
#include "game_world.hpp"
#include "game_event.hpp"
#include "../Template/function.hpp"
#include "../Template/vector.hpp"
#include "../Template/map.hpp"
#include "../Template/shared_ptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include "../Basic/class_nullptr.hpp"
#include <stdint.h>

struct ft_game_hook_metadata
{
    ft_string hook_identifier;
    ft_string listener_name;
    ft_string description;
    ft_string argument_contract;

    int32_t initialize(const ft_game_hook_metadata &other) noexcept;
    int32_t initialize(ft_game_hook_metadata &&other) noexcept;
    int32_t destroy() noexcept;
};

struct ft_game_hook_context
{
    game_world *world;
    game_character *character;
    game_item *item;
    game_event *event;
    int32_t integer_payload;
    uint8_t small_payload;
};

struct ft_game_hook_listener_entry
{
    ft_game_hook_metadata metadata;
    int32_t priority;
    ft_function<void(ft_game_hook_context&)> callback;

    int32_t initialize(const ft_game_hook_listener_entry &other) noexcept;
    int32_t initialize(ft_game_hook_listener_entry &&other) noexcept;
    int32_t destroy() noexcept;
};

extern const char *ft_game_hook_item_crafted_identifier;
extern const char *ft_game_hook_character_damaged_identifier;
extern const char *ft_game_hook_event_triggered_identifier;

ft_function<void(ft_game_hook_context&)> ft_game_hook_make_character_item_adapter(ft_function<void(game_character&, game_item&)> &&callback) noexcept;
ft_function<void(ft_game_hook_context&)> ft_game_hook_make_character_damage_adapter(ft_function<void(game_character&, int32_t, uint8_t)> &&callback) noexcept;
ft_function<void(ft_game_hook_context&)> ft_game_hook_make_world_event_adapter(ft_function<void(game_world&, game_event&)> &&callback) noexcept;

class game_hooks
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        using hook_callback = ft_function<void(ft_game_hook_context &)>;
        using callback_owner = ft_sharedptr<hook_callback>;

        struct listener_record
        {
            ft_game_hook_metadata metadata;
            int32_t priority;
            callback_owner callback;

            int32_t initialize(const listener_record &other) noexcept;
            int32_t initialize(listener_record &&other) noexcept;
            int32_t set_callback(hook_callback &&function) noexcept;
            int32_t clone_callback() noexcept;
        };

        ft_function<void(game_character&, game_item&)> _legacy_item_crafted;
        ft_function<void(game_character&, int32_t, uint8_t)> _legacy_character_damaged;
        ft_function<void(game_world&, game_event&)> _legacy_event_triggered;
        ft_map<ft_string, ft_vector<listener_record> > _listener_catalog;
        ft_vector<ft_game_hook_metadata> _catalog_metadata;
        pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

        void remove_listener_unlocked(const ft_string &hook_identifier,
            const ft_string &listener_name) noexcept;
        ft_size_t find_metadata_index_unlocked(const ft_string &hook_identifier,
            const ft_string &listener_name) const noexcept;
        void insert_listener_unlocked(listener_record &entry) noexcept;
        int32_t insert_callback_unlocked(listener_record &entry,
            ft_bool &replaced) noexcept;
        int32_t clone_listener_callbacks_unlocked() noexcept;
        int32_t move_catalog_metadata_unlocked(game_hooks &other) noexcept;
        void invoke_listeners(const ft_vector<listener_record> &listeners,
            ft_bool lock_acquired, ft_game_hook_context &context) const noexcept;
        template <typename Callback>
        void set_legacy_callback(Callback &stored_callback, Callback &&callback,
            hook_callback (*make_adapter)(Callback &&) noexcept, const char *hook_identifier,
            const char *listener_name, const char *description, const char *argument_contract) noexcept;
        template <typename Callback>
        Callback get_legacy_callback(const Callback &stored_callback) const noexcept;
        template <typename Identifier>
        void invoke_hook_by_key(const Identifier &identifier,
            ft_game_hook_context &context) const noexcept;

    public:
        game_hooks() noexcept;
        ~game_hooks() noexcept;
        game_hooks(const game_hooks &other) noexcept = delete;
        game_hooks &operator=(const game_hooks &other) noexcept = delete;
        game_hooks(game_hooks &&other) noexcept = delete;
        game_hooks &operator=(game_hooks &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_hooks &other) noexcept;
        int32_t initialize(game_hooks &&other) noexcept;
        // Moving requires exclusive access to both registries.
        int32_t move(game_hooks &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        void set_on_item_crafted(ft_function<void(game_character&, game_item&)> &&callback) noexcept;
        void set_on_character_damaged(ft_function<void(game_character&, int32_t, uint8_t)> &&callback) noexcept;
        void set_on_event_triggered(ft_function<void(game_world&, game_event&)> &&callback) noexcept;

        ft_function<void(game_character&, game_item&)> get_on_item_crafted() const noexcept;
        ft_function<void(game_character&, int32_t, uint8_t)> get_on_character_damaged() const noexcept;
        ft_function<void(game_world&, game_event&)> get_on_event_triggered() const noexcept;

        void invoke_on_item_crafted(game_character &character, game_item &item) const noexcept;
        void invoke_on_character_damaged(game_character &character, int32_t damage, uint8_t type) const noexcept;
        void invoke_on_event_triggered(game_world &world, game_event &event) const noexcept;

        void register_listener(const ft_game_hook_metadata &metadata, int32_t priority,
            ft_function<void(ft_game_hook_context&)> &&callback) noexcept;
        void unregister_listener(const ft_string &hook_identifier,
            const ft_string &listener_name) noexcept;
        ft_vector<ft_game_hook_metadata> *get_catalog_metadata() const noexcept;
        ft_vector<ft_game_hook_metadata> *get_catalog_metadata_for(const ft_string &hook_identifier) const noexcept;
        // Listener changes take effect for subsequent invocations.
        void invoke_hook(const ft_string &hook_identifier,
            ft_game_hook_context &context) const noexcept;

        void reset() noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
