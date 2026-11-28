#ifndef FT_JSON_INTERNAL_HPP
#define FT_JSON_INTERNAL_HPP

#include "json.hpp"

// Transfer the CMA buffers to the node on success. The caller retains them on failure.
json_item *json_create_item_owned(char *key, char *value);
json_group *json_create_group_owned(char *name);

// The caller holds the group mutex when one is configured.
json_item *json_find_item_locked(json_group *group, const char *key);

int32_t json_build_item_number(const char *value, ft_big_number *&number);
int32_t json_update_item_checked(json_group *group, const char *key, const char *value);
int32_t json_update_item_checked(json_group *group, const char *key, int32_t value);
int32_t json_update_item_checked(json_group *group, const char *key, ft_bool value);
int32_t json_update_item_checked(json_group *group, const char *key, const ft_big_number &value);

int32_t json_write_buffer(const json_group *groups, char *&output, ft_size_t &length);
// The caller owns the returned CMA buffer. Length excludes the terminator.
char *json_write_to_string_with_length(const json_group *groups, ft_size_t &length);
int32_t json_write_to_descriptor(int32_t descriptor, const json_group *groups);
int32_t json_document_serialize_buffer(const json_document &document,
    char *&output, ft_size_t &length) noexcept;
// Transfer the group list to the document on success.
int32_t json_document_replace_groups(json_document &document, json_group *groups) noexcept;

#endif
