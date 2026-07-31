#include "../test_internal.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/JSon/document.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct json_serializer_character
{
    char name[32];
    char role[32];
};

static int32_t json_serializer_write_character(
    json_document &document, void *user_data) noexcept
{
    json_serializer_character *character;
    json_group *group;
    json_item *name_item;
    json_item *role_item;

    character = static_cast<json_serializer_character *>(user_data);
    if (character == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    group = document.create_group("character");
    if (group == ft_nullptr)
        return (document.get_error());
    name_item = document.create_item("name", character->name);
    if (name_item == ft_nullptr)
        return (document.get_error());
    role_item = document.create_item("role", character->role);
    if (role_item == ft_nullptr)
        return (document.get_error());
    document.append_group(group);
    document.add_item(group, name_item);
    document.add_item(group, role_item);
    return (FT_ERR_SUCCESS);
}

static int32_t json_serializer_read_character(
    const json_document &document, void *user_data) noexcept
{
    json_serializer_character *character;
    const char *name_value;
    const char *role_value;

    character = static_cast<json_serializer_character *>(user_data);
    if (character == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    name_value = document.get_value_by_pointer("/character/name");
    if (name_value == ft_nullptr)
        return (document.get_error());
    role_value = document.get_value_by_pointer("/character/role");
    if (role_value == ft_nullptr)
        return (document.get_error());
    std::strncpy(character->name, name_value, sizeof(character->name) - 1);
    character->name[sizeof(character->name) - 1] = '\0';
    std::strncpy(character->role, role_value, sizeof(character->role) - 1);
    character->role[sizeof(character->role) - 1] = '\0';
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_json_serializer_to_string_and_from_string)
{
    json_serializer_character input_character;
    json_serializer_character output_character;
    ft_string serialized_content;

    std::strcpy(input_character.name, "Ada");
    std::strcpy(input_character.role, "wizard");
    std::memset(&output_character, 0, sizeof(output_character));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized_content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_serialize_to_string(
            json_serializer_write_character, &input_character,
            serialized_content));
    FT_ASSERT(std::strstr(serialized_content.c_str(), "\"character\"")
        != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_deserialize_from_string(
            serialized_content.c_str(), json_serializer_read_character,
            &output_character));
    FT_ASSERT_EQ(0, std::strcmp(output_character.name, "Ada"));
    FT_ASSERT_EQ(0, std::strcmp(output_character.role, "wizard"));
    return (1);
}

FT_TEST(test_json_serializer_rejects_null_callbacks)
{
    ft_string serialized_content;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized_content.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        json_serialize_to_string(ft_nullptr, ft_nullptr, serialized_content));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        json_deserialize_from_string("{}", ft_nullptr, ft_nullptr));
    return (1);
}
