#ifndef HTTP2_CLIENT_HPP
#define HTTP2_CLIENT_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Template/map.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <cstdint>
#include <cstddef>
#include "openssl_support.hpp"

#if NETWORKING_HAS_OPENSSL

class http2_header_field
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t             _initialised_state;
        ft_string           _name;
        ft_string           _value;
        mutable pt_recursive_mutex   *_mutex;

        friend ft_bool http2_compress_headers(
            const ft_vector<http2_header_field> &headers, ft_string &out_block,
            int32_t &error_code) noexcept;

        int32_t     lock(ft_bool *lock_acquired) const noexcept;
        void    unlock(ft_bool lock_acquired) const noexcept;

    public:
        http2_header_field() noexcept;
        ~http2_header_field() noexcept;

        http2_header_field(const http2_header_field &other) noexcept = delete;
        http2_header_field(http2_header_field &&other) noexcept = delete;
        http2_header_field &operator=(const http2_header_field &other) noexcept = delete;
        http2_header_field &operator=(http2_header_field &&other) noexcept = delete;
        int32_t move(http2_header_field &other) noexcept;
        int32_t     initialize() noexcept;
        int32_t initialize(const http2_header_field &other) noexcept;
        int32_t initialize(http2_header_field &&other) noexcept;
        int32_t     destroy() noexcept;
        int32_t     enable_thread_safety() noexcept;
        int32_t     disable_thread_safety() noexcept;
        ft_bool    is_thread_safe() const noexcept;

        ft_bool    set_name(const ft_string &name_value) noexcept;
        ft_bool    set_name_from_cstr(const char *name_value) noexcept;
        ft_bool    set_name_from_buffer(const char *buffer, ft_size_t length) noexcept;

        ft_bool    set_value(const ft_string &value_value) noexcept;
        ft_bool    set_value_from_cstr(const char *value_value) noexcept;
        ft_bool    set_value_from_buffer(const char *buffer, ft_size_t length) noexcept;

        ft_bool    assign(const ft_string &name_value, const ft_string &value_value) noexcept;
        ft_bool    assign_from_cstr(const char *name_value, const char *value_value) noexcept;
        ft_bool    assign_from_buffers(const char *name_buffer, ft_size_t name_length,
                    const char *value_buffer, ft_size_t value_length) noexcept;

        ft_bool    copy_name(ft_string &out_name) const noexcept;
        ft_bool    copy_value(ft_string &out_value) const noexcept;

        void    clear() noexcept;
};

class http2_frame
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t             _initialised_state;
        uint8_t             _type;
        uint8_t             _flags;
        uint32_t            _stream_identifier;
        ft_string           _payload;
        mutable pt_recursive_mutex   *_mutex;
        static thread_local int32_t _last_error;

        friend class http2_settings_state;
        friend ft_bool http2_encode_frame(const http2_frame &frame,
            ft_string &out_buffer, int32_t &error_code) noexcept;
        friend ft_bool http2_decode_frame(const unsigned char *buffer,
            ft_size_t buffer_size, ft_size_t &offset, http2_frame &out_frame,
            int32_t &error_code) noexcept;

        int32_t     lock(ft_bool *lock_acquired) const noexcept;
        void    unlock(ft_bool lock_acquired) const noexcept;
        int32_t set_error(int32_t error_code) const noexcept;

    public:
        http2_frame() noexcept;
        ~http2_frame() noexcept;

        http2_frame(const http2_frame &other) noexcept = delete;
        http2_frame(http2_frame &&other) noexcept = delete;
        http2_frame &operator=(const http2_frame &other) noexcept = delete;
        http2_frame &operator=(http2_frame &&other) noexcept = delete;
        int32_t move(http2_frame &other) noexcept;
        int32_t     initialize() noexcept;
        int32_t initialize(const http2_frame &other) noexcept;
        int32_t initialize(http2_frame &&other) noexcept;
        int32_t     destroy() noexcept;

        int32_t     enable_thread_safety() noexcept;
        int32_t     disable_thread_safety() noexcept;
        ft_bool    is_thread_safe() const noexcept;

        ft_bool    set_type(uint8_t type_value) noexcept;
        ft_bool    get_type(uint8_t &out_type) const noexcept;

        ft_bool    set_flags(uint8_t flags_value) noexcept;
        ft_bool    get_flags(uint8_t &out_flags) const noexcept;

        ft_bool    set_stream_identifier(uint32_t stream_identifier_value) noexcept;
        ft_bool    get_stream_identifier(uint32_t &out_stream_identifier) const noexcept;

        ft_bool    set_payload(const ft_string &payload_value) noexcept;
        ft_bool    set_payload_from_buffer(const char *buffer, ft_size_t length) noexcept;
        ft_bool    copy_payload(ft_string &out_payload) const noexcept;
        void    clear_payload() noexcept;
        int32_t get_error(void) const noexcept;
        const char *get_error_str(void) const noexcept;
};

struct http2_stream_state
{
    ft_string   buffer;
    uint32_t    dependency_identifier;
    uint8_t     weight;
    ft_bool        exclusive_dependency;
    uint32_t    remote_window;
    uint32_t    local_window;

    http2_stream_state() noexcept;
    ~http2_stream_state() noexcept;
};

class http2_stream_manager
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t                                     _initialised_state;
        ft_bool        reserve_remote_connection_window(uint32_t length) noexcept;
        ft_bool        record_connection_send(uint32_t length) noexcept;
        ft_bool        update_initial_window(uint32_t new_window, uint32_t &initial_window,
                        uint32_t http2_stream_state::*stream_window) noexcept;

        ft_map<uint32_t, http2_stream_state>    _streams;
        uint32_t                                _initial_remote_window;
        uint32_t                                _initial_local_window;
        uint32_t                                _connection_remote_window;
        uint32_t                                _connection_local_window;
        mutable pt_recursive_mutex   *_mutex;
        static thread_local int32_t             _last_error;

        int32_t         lock(ft_bool *lock_acquired) const noexcept;
        void        unlock(ft_bool lock_acquired) const noexcept;
        int32_t         set_error(int32_t error_code) const noexcept;

    public:
        http2_stream_manager() noexcept;
        http2_stream_manager(const http2_stream_manager &other) noexcept = delete;
        http2_stream_manager(http2_stream_manager &&other) noexcept = delete;
        ~http2_stream_manager() noexcept;

        http2_stream_manager &operator=(const http2_stream_manager &other) = delete;
        http2_stream_manager &operator=(http2_stream_manager &&other) = delete;
        int32_t move(http2_stream_manager &other) noexcept;
        int32_t         initialize() noexcept;
        int32_t initialize(const http2_stream_manager &other) noexcept;
        int32_t initialize(http2_stream_manager &&other) noexcept;
        int32_t         destroy() noexcept;

        int32_t         enable_thread_safety() noexcept;
        int32_t         disable_thread_safety() noexcept;
        ft_bool        is_thread_safe() const noexcept;
        int32_t         get_error(void) const noexcept;
        const char      *get_error_str(void) const noexcept;

        ft_bool        open_stream(uint32_t stream_identifier) noexcept;
        ft_bool        append_data(uint32_t stream_identifier, const char *data,
                        ft_size_t length) noexcept;
        ft_bool        close_stream(uint32_t stream_identifier) noexcept;
        ft_bool        get_stream_buffer(uint32_t stream_identifier,
                        ft_string &out_buffer) const noexcept;
        ft_bool        update_priority(uint32_t stream_identifier,
                        uint32_t dependency_identifier, uint8_t weight,
                        ft_bool exclusive) noexcept;
        ft_bool        get_priority(uint32_t stream_identifier,
                        uint32_t &dependency_identifier, uint8_t &weight,
                        ft_bool &exclusive) const noexcept;
        ft_bool        update_remote_initial_window(uint32_t new_window) noexcept;
        ft_bool        update_local_initial_window(uint32_t new_window) noexcept;
        uint32_t    get_local_window(uint32_t stream_identifier) const noexcept;
        uint32_t    get_remote_window(uint32_t stream_identifier) const noexcept;
        ft_bool        increase_local_window(uint32_t stream_identifier,
                        uint32_t increment) noexcept;
        ft_bool        increase_remote_window(uint32_t stream_identifier,
                        uint32_t increment) noexcept;
        ft_bool        reserve_send_window(uint32_t stream_identifier,
                        uint32_t length) noexcept;
        ft_bool        update_connection_local_window(uint32_t increment) noexcept;
        ft_bool        update_connection_remote_window(uint32_t increment) noexcept;
        uint32_t    get_connection_local_window() const noexcept;
        uint32_t    get_connection_remote_window() const noexcept;
};

class http2_settings_state
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t     _initialised_state;
        ft_bool        apply_single_setting(uint16_t identifier, uint32_t value,
                        http2_stream_manager &streams) noexcept;

        uint32_t    _header_table_size;
        ft_bool        _enable_push;
        uint32_t    _max_concurrent_streams;
        uint32_t    _initial_local_window;
        uint32_t    _initial_remote_window;
        uint32_t    _max_frame_size;
        uint32_t    _max_header_list_size;

    public:
        http2_settings_state() noexcept;
        http2_settings_state(const http2_settings_state &other) noexcept = delete;
        http2_settings_state(http2_settings_state &&other) noexcept = delete;
        ~http2_settings_state() noexcept;
        http2_settings_state &operator=(const http2_settings_state &other) noexcept = delete;
        http2_settings_state &operator=(http2_settings_state &&other) noexcept = delete;
        int32_t move(http2_settings_state &other) noexcept;
        int32_t         initialize() noexcept;
        int32_t initialize(const http2_settings_state &other) noexcept;
        int32_t initialize(http2_settings_state &&other) noexcept;
        int32_t         destroy() noexcept;

        ft_bool        apply_remote_settings(const http2_frame &frame,
                        http2_stream_manager &streams) noexcept;
        ft_bool        update_local_initial_window(uint32_t new_window,
                        http2_stream_manager &streams) noexcept;
        ft_bool        update_remote_initial_window(uint32_t new_window,
                        http2_stream_manager &streams) noexcept;
        uint32_t    get_header_table_size() const noexcept;
        ft_bool        get_enable_push() const noexcept;
        uint32_t    get_max_concurrent_streams() const noexcept;
        uint32_t    get_initial_local_window() const noexcept;
        uint32_t    get_initial_remote_window() const noexcept;
        uint32_t    get_max_frame_size() const noexcept;
        uint32_t    get_max_header_list_size() const noexcept;
};

ft_bool    http2_encode_frame(const http2_frame &frame, ft_string &out_buffer,
            int32_t &error_code) noexcept;
ft_bool    http2_decode_frame(const unsigned char *buffer, ft_size_t buffer_size,
            ft_size_t &offset, http2_frame &out_frame, int32_t &error_code) noexcept;
ft_bool    http2_compress_headers(const ft_vector<http2_header_field> &headers,
            ft_string &out_block, int32_t &error_code) noexcept;
ft_bool    http2_decompress_headers(const ft_string &block,
            ft_vector<http2_header_field> &out_headers, int32_t &error_code) noexcept;
ft_bool    http2_select_alpn_protocol(SSL *ssl_session, ft_bool &selected_http2,
            int32_t &error_code) noexcept;

#endif
#endif
