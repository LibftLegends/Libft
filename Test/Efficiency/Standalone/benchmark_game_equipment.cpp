#include "../../../Modules/Game/game_equipment.hpp"
#include <chrono>
#include <cstdio>
#include <new>

int main()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    game_equipment first;
    game_equipment second;
    ft_sharedptr<game_item> item;
    game_item *item_pointer = new (std::nothrow) game_item();
    if (item_pointer == ft_nullptr)
        return (1);
    if (item.initialize(item_pointer) != FT_ERR_SUCCESS)
    {
        delete item_pointer;
        return (1);
    }
    if (item_pointer->initialize() != FT_ERR_SUCCESS || first.initialize() != FT_ERR_SUCCESS)
        return (1);

    uint32_t iteration = 0U;
    int32_t slot = EQUIP_HEAD;
    const auto update_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (first.equip(slot, item) != FT_ERR_SUCCESS)
            return (1);
        first.unequip(slot);
        if (first.get_error() != FT_ERR_SUCCESS)
            return (1);
        if (slot == EQUIP_OFF_HAND)
            slot = EQUIP_HEAD;
        else
            ++slot;
        ++iteration;
    }
    const double update_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - update_start).count();
    if (item.use_count() != 1)
        return (1);

    slot = EQUIP_HEAD;
    while (slot <= EQUIP_OFF_HAND)
    {
        if (first.equip(slot, item) != FT_ERR_SUCCESS)
            return (1);
        ++slot;
    }
    game_equipment *source = &first;
    game_equipment *destination = &second;
    iteration = 0U;
    const auto move_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_equipment *previous = source;
        source = destination;
        destination = previous;
        ++iteration;
    }
    const double move_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - move_start).count();
    if (item.use_count() != 7 || source->destroy() != FT_ERR_SUCCESS || item.use_count() != 1)
        return (1);
    std::printf("equipment_update %.3f\nequipment_move %.3f\n",
        update_elapsed / ITERATIONS, move_elapsed / ITERATIONS);
    return (0);
}
