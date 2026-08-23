#ifndef GAME_SERVER_HPP
# define GAME_SERVER_HPP

#include "game_world.hpp"
#include "game_event.hpp"
#include "../Networking/websocket_server.hpp"
#include "../JSon/json.hpp"
#include "../Template/map.hpp"
#include "../Template/shared_ptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

typedef int32_t (*game_server_message_callback)(int32_t client_handle,
    const char *message, void *user_data);

class game_server
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_websocket_server *_server;
        ft_sharedptr<game_world> _world;
        ft_map<int32_t, int32_t>   _clients;
        ft_string          _auth_token;
        void              (*_on_join)(int32_t);
        void              (*_on_leave)(int32_t);
        game_server_message_callback _on_message;
        void                         *_message_user_data;
        mutable pt_recursive_mutex    *_mutex;
        static thread_local int32_t _last_error;
        uint8_t                       _initialised_state;
        int32_t handle_message_locked(int32_t client_handle,
                const ft_string &message) noexcept;
        int32_t serialize_world_locked(ft_string &out) const noexcept;
        void join_client_locked(int32_t client_id, int32_t client_handle) noexcept;
        void leave_client_locked(int32_t client_id) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_server() noexcept;
        game_server(const game_server &other) noexcept = delete;
        game_server(game_server &&other) noexcept = delete;
        virtual ~game_server() noexcept;
        game_server &operator=(const game_server &other) noexcept = delete;
        game_server &operator=(game_server &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_sharedptr<game_world> &world,
            const char *auth_token = ft_nullptr) noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_server &other) noexcept;

        void set_join_callback(void (*callback)(int32_t)) noexcept;
        void set_leave_callback(void (*callback)(int32_t)) noexcept;
        void set_message_callback(game_server_message_callback callback,
            void *user_data) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t start(const char *ip, uint16_t port) noexcept;
        void run_once() noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
