#include "../test_internal.hpp"
#include "../../Modules/XML/xml.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct xml_serializer_character
{
    char name[32];
};

static int32_t xml_serializer_write_character(
    xml_document &document, void *user_data) noexcept
{
    xml_serializer_character *character;

    character = static_cast<xml_serializer_character *>(user_data);
    if (character == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (std::strcmp(character->name, "Ada") != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    return (document.load_from_string("<character><name>Ada</name></character>"));
}

static int32_t xml_serializer_read_character(
    const xml_document &document, void *user_data) noexcept
{
    xml_serializer_character *character;
    xml_node *root_node;
    xml_node *name_node;

    character = static_cast<xml_serializer_character *>(user_data);
    if (character == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    root_node = document.get_root();
    if (root_node == ft_nullptr || root_node->children.size() == 0)
        return (FT_ERR_NOT_FOUND);
    name_node = root_node->children[0];
    if (name_node == ft_nullptr || name_node->text == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    std::strncpy(character->name, name_node->text,
        sizeof(character->name) - 1);
    character->name[sizeof(character->name) - 1] = '\0';
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_xml_serializer_to_string_and_from_string)
{
    xml_serializer_character input_character;
    xml_serializer_character output_character;
    ft_string serialized_content;

    std::strcpy(input_character.name, "Ada");
    std::memset(&output_character, 0, sizeof(output_character));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized_content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, xml_serialize_to_string(
            xml_serializer_write_character, &input_character,
            serialized_content));
    FT_ASSERT(std::strstr(serialized_content.c_str(), "<character>")
        != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, xml_deserialize_from_string(
            serialized_content.c_str(), xml_serializer_read_character,
            &output_character));
    FT_ASSERT_EQ(0, std::strcmp(output_character.name, "Ada"));
    return (1);
}

FT_TEST(test_xml_serializer_rejects_null_callbacks)
{
    ft_string serialized_content;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized_content.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        xml_serialize_to_string(ft_nullptr, ft_nullptr, serialized_content));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        xml_deserialize_from_string("<root/>", ft_nullptr, ft_nullptr));
    return (1);
}
