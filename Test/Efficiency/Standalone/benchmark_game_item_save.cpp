#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/Game/game_quest.hpp"
#include "../../../Modules/JSon/json.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

json_group *serialize_equipment(const game_character &character);
json_group *serialize_quest(const game_quest &quest);
int32_t deserialize_equipment(game_character &character, json_group *group);
int32_t deserialize_quest(game_quest &quest, json_group *group);

static void print_measurement(const char *label, const char *operation, double time,
    const char *encoded)
{
    const ft_size_t length = ft_strlen_size_t(encoded);
    uint64_t checksum = 0U;
    ft_size_t offset = 0U;
    while (offset < length)
    {
        checksum = checksum * 31U + static_cast<uint8_t>(encoded[offset]);
        ++offset;
    }
    std::printf("%s %.3f us/%s bytes=%" PRIu64 " checksum=%" PRIu64 "\n",
        label, time, operation, static_cast<uint64_t>(length), checksum);
    return ;
}

template <typename Subject>
static int32_t measure_save(const char *label, const Subject &subject,
    json_group *(*serialize)(const Subject &), uint32_t iterations)
{
    json_group *group = ft_nullptr;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        json_free_groups(group);
        group = serialize(subject);
        if (group == ft_nullptr)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    char *encoded = json_write_to_string(group);
    json_free_groups(group);
    if (encoded == ft_nullptr)
        return (1);
    print_measurement(label, "save", elapsed / iterations, encoded);
    cma_free(encoded);
    return (0);
}

template <typename Subject>
static int32_t measure_load(const char *label, const Subject &source,
    json_group *(*serialize)(const Subject &),
    int32_t (*deserialize)(Subject &, json_group *), uint32_t iterations)
{
    Subject restored;
    if (restored.initialize() != FT_ERR_SUCCESS)
        return (1);
    json_group *group = serialize(source);
    if (group == ft_nullptr)
        return (1);
    int32_t error = FT_ERR_SUCCESS;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations && error == FT_ERR_SUCCESS)
    {
        error = deserialize(restored, group);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    char *expected = json_write_to_string(group);
    json_free_groups(group);
    group = serialize(restored);
    char *actual = ft_nullptr;
    if (group != ft_nullptr)
        actual = json_write_to_string(group);
    json_free_groups(group);
    const ft_bool matches = error == FT_ERR_SUCCESS && expected != ft_nullptr
        && actual != ft_nullptr && ft_strcmp(expected, actual) == 0;
    if (matches == FT_TRUE)
        print_measurement(label, "load", elapsed / iterations, actual);
    cma_free(expected);
    cma_free(actual);
    return (matches == FT_FALSE);
}

int main(int argument_count, char **arguments)
{
    game_character character;
    game_quest quest;
    if (character.initialize() != FT_ERR_SUCCESS || quest.initialize() != FT_ERR_SUCCESS)
        return (1);
    constexpr int32_t SLOTS[] = {EQUIP_HEAD, EQUIP_CHEST, EQUIP_WEAPON,
        EQUIP_LEGS, EQUIP_FEET, EQUIP_OFF_HAND};
    uint32_t index = 0U;
    while (index < 128U)
    {
        ft_sharedptr<game_item> item(new (std::nothrow) game_item());
        if (!item || item->initialize() != FT_ERR_SUCCESS)
            return (1);
        item->set_item_id(static_cast<int32_t>(index + 1U));
        item->set_instance_id(UINT64_MAX - index);
        item->set_rarity(2);
        item->set_max_stack(1);
        item->set_stack_size(1);
        if (index < sizeof(SLOTS) / sizeof(SLOTS[0])
            && character.equip_item(SLOTS[index], item) != FT_ERR_SUCCESS)
            return (1);
        if (quest.get_reward_items().push_back(item) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--load") == 0)
    {
        if (measure_load("equipment", character, serialize_equipment,
                deserialize_equipment, 256U) != 0
            || measure_load("quest", quest, serialize_quest, deserialize_quest, 16U) != 0)
            return (1);
        return (0);
    }
    if (measure_save("equipment", character, serialize_equipment, 512U) != 0
        || measure_save("quest", quest, serialize_quest, 16U) != 0)
        return (1);
    return (0);
}
