#include "../test_internal.hpp"
#include "../../Modules/Game/game_region_definition.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_game_region_definition_defaults)
{
    game_region_definition region;
    ft_string empty;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(0, region.get_region_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    FT_ASSERT_STR_EQ(empty.c_str(), region.get_name().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_STR_EQ(empty.c_str(), region.get_description().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(0, region.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    return (1);
}

FT_TEST(test_game_region_definition_move_transfers_text_without_allocating)
{
    game_region_definition source;
    game_region_definition destination;
    ft_string name;
    ft_string description;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize(256U, 'n'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, description.initialize(4096U, 'd'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(8, name, description, 17));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    const char *stored_name = source.get_name().c_str();
    const char *stored_description = source.get_description().c_str();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT(stored_name == destination.get_name().c_str());
    FT_ASSERT(stored_description == destination.get_description().c_str());
    FT_ASSERT_EQ(8, destination.get_region_id());
    FT_ASSERT_EQ(17, destination.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_name().size());
    return (1);
}

FT_TEST(test_game_region_definition_text_failure_keeps_valid_state)
{
    game_region_definition source;
    game_region_definition destination;
    ft_string name;
    ft_string description;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("cave"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, description.initialize(4096U, 'd'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(8, name, description, 17));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    destination.set_description(name);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(128U));
    destination.set_description(description);
    const int32_t setter_error = destination.get_error();
    const ft_bool old_description_preserved = destination.get_description() == name;
    const int32_t copy_error = destination.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, setter_error);
    FT_ASSERT_EQ(FT_TRUE, old_description_preserved);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(4096U, source.get_description().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(4096U, destination.get_description().size());
    return (1);
}

FT_TEST(test_game_region_definition_reinitializes_from_borrowed_text)
{
    game_region_definition region;
    ft_string name;
    ft_string description;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("reef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, description.initialize("shallows"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(1, name, description, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(2, region.get_description(),
        region.get_name(), 6));
    FT_ASSERT_STR_EQ("shallows", region.get_name().c_str());
    FT_ASSERT_STR_EQ("reef", region.get_description().c_str());
    FT_ASSERT_EQ(2, region.get_region_id());
    FT_ASSERT_EQ(6, region.get_recommended_level());
    return (1);
}

FT_TEST(test_game_region_definition_initialize_with_fields_records_fields)
{
    game_region_definition region;

    ft_string canyon;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, canyon.initialize("canyon"));
    ft_string rocky_pass;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rocky_pass.initialize("rocky pass"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(9, canyon,
        rocky_pass, 14));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(9, region.get_region_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_STR_EQ("canyon", region.get_name().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_STR_EQ("rocky pass", region.get_description().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(14, region.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    return (1);
}

FT_TEST(test_game_region_definition_setters_update_fields)
{
    game_region_definition region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    region.set_region_id(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    ft_string spire;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spire.initialize("spire"));
    region.set_name(spire);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    ft_string towering_peak;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, towering_peak.initialize("towering peak"));
    region.set_description(towering_peak);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    region.set_recommended_level(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(5, region.get_region_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_STR_EQ("spire", region.get_name().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_STR_EQ("towering peak", region.get_description().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(20, region.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    return (1);
}

FT_TEST(test_game_region_definition_initialize_copy_restores_values)
{
    game_region_definition source;
    game_region_definition clone;

    ft_string reef;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reef.initialize("reef"));
    ft_string shallow_waters;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shallow_waters.initialize("shallow waters"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(7, reef,
        shallow_waters, 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(7, clone.get_region_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_STR_EQ("reef", clone.get_name().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_STR_EQ("shallow waters", clone.get_description().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_EQ(6, clone.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    return (1);
}
