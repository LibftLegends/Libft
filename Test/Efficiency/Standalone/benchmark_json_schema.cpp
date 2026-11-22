#include "../../../Modules/JSon/json.hpp"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

static int32_t measure_validation(uint32_t size, uint32_t iterations)
{
    std::vector<std::string> keys(size);
    std::vector<json_item> items(size);
    std::vector<json_schema_field> fields(size);
    char number[] = "18446744073709551616";
    uint32_t index = 0U;
    while (index < size)
    {
        keys[index] = "field-" + std::to_string(index);
        items[index].key = keys[index].data();
        items[index].value = number;
        if (index + 1U < size)
            items[index].next = &items[index + 1U];
        ++index;
    }
    index = 0U;
    while (index < size)
    {
        fields[index].key = keys[size - index - 1U].c_str();
        fields[index].type = JSON_NUMBER;
        fields[index].required = FT_TRUE;
        if (index + 1U < size)
            fields[index].next = &fields[index + 1U];
        ++index;
    }
    json_group group{};
    group.items = items.data();
    json_schema schema{};
    schema.fields = fields.data();
    if (json_group_enable_thread_safety(&group) != FT_ERR_SUCCESS)
        return (1);
    ft_bool valid = json_validate_schema(&group, schema);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations && valid)
    {
        valid = json_validate_schema(&group, schema);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    index = 0U;
    while (index < size)
    {
        (void)json_schema_field_disable_thread_safety(&fields[index]);
        ++index;
    }
    (void)json_schema_disable_thread_safety(&schema);
    (void)json_group_disable_thread_safety(&group);
    if (!valid)
        return (1);
    std::printf("schema_validation fields=%u us/op=%.3f\n", size,
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations);
    return (0);
}

int32_t main()
{
    if (measure_validation(8U, 5000U) != 0)
        return (1);
    if (measure_validation(16U, 2000U) != 0
        || measure_validation(32U, 1000U) != 0
        || measure_validation(64U, 500U) != 0)
        return (1);
    return (measure_validation(1024U, 40U));
}
