#ifndef JSON_SCHEMA_HPP
#define JSON_SCHEMA_HPP

#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"

struct json_group;

typedef enum json_type
{
    JSON_STRING,
    JSON_NUMBER, // Signed or unsigned decimal integer text.
    JSON_BOOL
} json_type;

typedef struct json_schema_field
{
    const char *key;
    json_type   type;
    ft_bool        required;
    struct json_schema_field *next;
    mutable pt_recursive_mutex *_mutex;
    mutable int32_t _error_code;
} json_schema_field;

typedef struct json_schema
{
    json_schema_field *fields;
    mutable pt_recursive_mutex *_mutex;
    mutable int32_t _error_code;
} json_schema;

ft_bool        json_validate_schema(json_group *group, const json_schema &schema);

#endif
