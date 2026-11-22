#include "../test_internal.hpp"
#include "../../Modules/Template/trie.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/unordered_map.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_trie_insert_nullptr_sets_errno)
{
    ft_trie<int> trie;
    int stored_value = 42;
    size_t valid_key_length = ft_strlen_size_t("valid");
    const char *valid_key = "valid";

    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.initialize());
    FT_ASSERT_EQ(0, trie.insert(valid_key, &stored_value));
    const auto *stored_node = trie.search(valid_key);
    FT_ASSERT(stored_node != ft_nullptr);
    FT_ASSERT_EQ(valid_key_length, stored_node->_key_length);
    FT_ASSERT_EQ(0, stored_node->_unset_value);
    FT_ASSERT_EQ(&stored_value, stored_node->_value_pointer);

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, trie.insert(ft_nullptr, &stored_value));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, trie.get_error());
    FT_ASSERT_EQ(valid_key_length, stored_node->_key_length);
    FT_ASSERT_EQ(0, stored_node->_unset_value);
    FT_ASSERT_EQ(&stored_value, stored_node->_value_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.destroy());
    return (1);
}

FT_TEST(test_trie_thread_safety_controls_reset_errno)
{
    ft_trie<int> trie;
    ft_bool lock_acquired;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.initialize());
    FT_ASSERT_EQ(FT_FALSE, trie.is_thread_safe());
    FT_ASSERT_EQ(0, trie.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, trie.is_thread_safe());
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, trie.lock(&lock_acquired));
    FT_ASSERT_EQ(FT_TRUE, lock_acquired);
    trie.unlock(lock_acquired);
    int stored_value = 7;
    FT_ASSERT_EQ(0, trie.insert("key", &stored_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.get_error());
    const auto *node = trie.search("key");
    FT_ASSERT(node != ft_nullptr);
    FT_ASSERT_EQ(&stored_value, node->_value_pointer);
    trie.disable_thread_safety();
    FT_ASSERT_EQ(FT_FALSE, trie.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.destroy());
    return (1);
}

FT_TEST(test_trie_preserves_values_across_full_byte_fanout_and_replacement)
{
    ft_trie<uint32_t> trie;
    uint32_t first_value = 11U;
    uint32_t replacement_value = 22U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.insert("", &first_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.insert("z!", &first_value));
    const auto *retained = trie.search("z!");
    FT_ASSERT(retained != ft_nullptr);
    uint32_t index = 1U;
    while (index <= 255U)
    {
        const char character = static_cast<char>((index * 73U) % 256U);
        const char key[] = {character, 'x', '\0'};
        FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.insert(key, &first_value, static_cast<int32_t>(index)));
        ++index;
    }
    index = 1U;
    while (index <= 255U)
    {
        const char character = static_cast<char>((index * 73U) % 256U);
        const char key[] = {character, 'x', '\0'};
        const auto *found = trie.search(key);
        FT_ASSERT(found != ft_nullptr);
        FT_ASSERT_EQ(2U, found->_key_length);
        FT_ASSERT_EQ(static_cast<int32_t>(index), found->_unset_value);
        FT_ASSERT_EQ(&first_value, found->_value_pointer);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.insert("z!", &replacement_value, -1));
    FT_ASSERT_EQ(retained, trie.search("z!"));
    FT_ASSERT_EQ(&replacement_value, retained->_value_pointer);
    const auto *empty_key = trie.search("");
    FT_ASSERT(empty_key != ft_nullptr);
    FT_ASSERT_EQ(&first_value, empty_key->_value_pointer);
    FT_ASSERT(trie.search("z") == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.get_error());
    FT_ASSERT(trie.search("zz") == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, trie.get_error());
    return (1);
}

FT_TEST(test_trie_failed_branch_growth_preserves_existing_keys)
{
    ft_trie<uint32_t> trie;
    uint32_t value = 17U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.insert("a", &value));
    const auto *existing = trie.search("a");
    FT_ASSERT(existing != ft_nullptr);
    cma_set_alloc_limit(1U);
    const int32_t insert_error = trie.insert("b", &value);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, insert_error);
    FT_ASSERT_EQ(existing, trie.search("a"));
    FT_ASSERT(trie.search("b") == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, trie.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trie.insert("b", &value));
    FT_ASSERT(trie.search("b") != ft_nullptr);
    return (1);
}
