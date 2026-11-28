#include "../test_internal.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_kv_store_entry_defaults_without_expiration)
{
    kv_store_entry entry;
    ft_string value_copy;
    ft_bool has_expiration;
    int64_t expiration_timestamp;
    const char *value_pointer;
    ft_bool expired;

    has_expiration = true;
    expiration_timestamp = -1;
    value_pointer = reinterpret_cast<const char *>(1);
    expired = true;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize());
    FT_ASSERT_EQ(0, entry.copy_value(value_copy));
    FT_ASSERT(value_copy.empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.has_expiration(has_expiration));
    FT_ASSERT_EQ(false, has_expiration);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.get_expiration(expiration_timestamp));
    FT_ASSERT_EQ(0, expiration_timestamp);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.get_value_pointer(&value_pointer));
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT(*value_pointer == '\0');
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.is_expired(42, expired));
    FT_ASSERT_EQ(false, expired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);
    return (1);
}

FT_TEST(test_kv_store_entry_set_value_validates_input)
{
    kv_store_entry entry;
    ft_string copied_value;
    const char *value_pointer;

    value_pointer = ft_nullptr;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize());
    FT_ASSERT_EQ(0, entry.set_value("initial"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, entry.set_value(static_cast<const char *>(ft_nullptr)));

    FT_ASSERT_EQ(0, entry.copy_value(copied_value));
    FT_ASSERT(copied_value == "initial");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    ft_string updated_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, updated_value.initialize("updated"));
    FT_ASSERT_EQ(0, entry.set_value(updated_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.get_value_pointer(&value_pointer));
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT(ft_strcmp(value_pointer, "updated") == 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);
    return (1);
}

FT_TEST(test_kv_store_entry_expiration_controls)
{
    kv_store_entry entry;
    ft_bool has_expiration;
    int64_t expiration_timestamp;
    ft_bool expired;

    has_expiration = false;
    expiration_timestamp = 0;
    expired = false;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize());
    FT_ASSERT_EQ(0, entry.configure_expiration(true, 120));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.has_expiration(has_expiration));
    FT_ASSERT_EQ(true, has_expiration);
    FT_ASSERT_EQ(0, entry.get_expiration(expiration_timestamp));
    FT_ASSERT_EQ(120, expiration_timestamp);

    FT_ASSERT_EQ(0, entry.is_expired(60, expired));
    FT_ASSERT_EQ(false, expired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    FT_ASSERT_EQ(0, entry.is_expired(180, expired));
    FT_ASSERT_EQ(true, expired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    expired = true;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, entry.is_expired(-5, expired));
    FT_ASSERT_EQ(false, expired);

    FT_ASSERT_EQ(0, entry.configure_expiration(false, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);

    expired = true;
    FT_ASSERT_EQ(0, entry.is_expired(300, expired));
    FT_ASSERT_EQ(false, expired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, FT_ERR_SUCCESS);
    return (1);
}

FT_TEST(test_kv_store_entry_failed_replacement_preserves_existing_value)
{
    kv_store_entry entry;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.set_value("original"));
    cma_set_alloc_limit(1);
    const int32_t error_code = entry.set_value("a larger replacement");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    const char *value = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_value_pointer(&value));
    FT_ASSERT_EQ(0, ft_strcmp(value, "original"));
    return (1);
}
