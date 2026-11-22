#ifndef JSON_DOCUMENT_HPP
#define JSON_DOCUMENT_HPP

#include "json.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <cstdint>

class json_document
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        json_group *_groups;
        mutable int32_t _error_code;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        void set_error_unlocked(int32_t error_code) const noexcept;
        void set_error(int32_t error_code) const noexcept;
        void clear_unlocked() noexcept;
        char *write_to_string_unlocked(ft_size_t &length) const noexcept;
        friend int32_t json_document_serialize_buffer(const json_document &document,
            char *&output, ft_size_t &length) noexcept;
        friend int32_t json_document_replace_groups(json_document &document,
            json_group *groups) noexcept;
        json_item *find_item_by_pointer_unlocked(const char *pointer) const noexcept;
        template <typename Value>
        void update_item_checked(json_group *group, const char *key, const Value &value) noexcept;

    public:
        json_document() noexcept;
        json_document(const json_document &other) noexcept = delete;
        json_document(json_document &&other) noexcept = delete;
        ~json_document() noexcept;

        json_document &operator=(const json_document &) = delete;
        json_document &operator=(json_document &&) = delete;

        int32_t          initialize() noexcept;
        int32_t          initialize(const json_document &other) noexcept;
        int32_t          initialize(json_document &&other) noexcept;
        int32_t          destroy() noexcept;
        uint32_t         move(json_document &other) noexcept;
        int32_t          enable_thread_safety() noexcept;
        int32_t          disable_thread_safety() noexcept;
        ft_bool         is_thread_safe() const noexcept;

        json_group   *create_group(const char *name) noexcept;
        json_item    *create_item(const char *key, const char *value) noexcept;
        json_item    *create_item(const char *key, const ft_big_number &value) noexcept;
        json_item    *create_item(const char *key, const int32_t value) noexcept;
        json_item    *create_item(const char *key, const ft_bool value) noexcept;
        void         add_item(json_group *group, json_item *item) noexcept;
        void         append_group(json_group *group) noexcept;
        int32_t          write_to_file(const char *file_path) const noexcept;
        int32_t          write_to_backend(ft_document_sink &sink) const noexcept;
        char         *write_to_string() const noexcept;
        int32_t          read_from_file(const char *file_path) noexcept;
        int32_t          read_from_file_streaming(const char *file_path, ft_size_t buffer_capacity) noexcept;
        int32_t          read_from_backend(ft_document_source &source) noexcept;
        int32_t          read_from_string(const char *content) noexcept;
        json_group   *find_group(const char *name) const noexcept;
        json_item    *find_item(json_group *group, const char *key) const noexcept;
        json_item    *find_item_by_pointer(const char *pointer) const noexcept;
        const char   *get_value_by_pointer(const char *pointer) const noexcept;
        void         remove_group(const char *name) noexcept;
        void         remove_item(json_group *group, const char *key) noexcept;
        void         update_item(json_group *group, const char *key, const char *value) noexcept;
        void         update_item(json_group *group, const char *key, const int32_t value) noexcept;
        void         update_item(json_group *group, const char *key, const ft_bool value) noexcept;
        void         update_item(json_group *group, const char *key, const ft_big_number &value) noexcept;
        void         clear() noexcept;
        json_group   *get_groups() const noexcept;
        void         set_manual_error(int32_t error_code) noexcept;
        int32_t          get_error() const noexcept;
        const char   *get_error_str() const noexcept;
};

#endif
