#include "../test_internal.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/File/open_dir.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <cstdio>
#include <cerrno>
#include <string>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
# include <unistd.h>
#endif

static void remove_directory_if_present(const char *directory_path)
{
    int directory_exists;
    int status;
    int error_code;

    directory_exists = 0;
    status = cmp_directory_exists(directory_path, &directory_exists, &error_code);
    if (status != FT_ERR_SUCCESS || directory_exists != 1)
        return ;
#if defined(_WIN32) || defined(_WIN64)
    RemoveDirectoryA(directory_path);
#else
    rmdir(directory_path);
#endif
    return ;
}

static void cleanup_paths(const char *directory_path, const char *file_path)
{
    file_delete(file_path);
    remove_directory_if_present(directory_path);
    return ;
}

static void create_kv_store_file(const char *file_path)
{
    FILE *file_pointer;

    file_pointer = ft_fopen(file_path, "w");
    if (file_pointer == ft_nullptr)
        return ;
    std::fputs("{\n  \"kv_store\": {\n    \"__placeholder__\": \"\"\n  }\n}\n", file_pointer);
    ft_fclose(file_pointer);
    return ;
}

static std::string read_file_contents(const char *file_path)
{
    FILE *file_pointer;
    long file_size;
    std::string file_content;
    std::size_t bytes_read;

    file_pointer = ft_fopen(file_path, "rb");
    if (file_pointer == ft_nullptr)
        return (std::string());
    if (std::fseek(file_pointer, 0, SEEK_END) != 0)
    {
        ft_fclose(file_pointer);
        return (std::string());
    }
    file_size = std::ftell(file_pointer);
    if (file_size < 0)
    {
        ft_fclose(file_pointer);
        return (std::string());
    }
    if (std::fseek(file_pointer, 0, SEEK_SET) != 0)
    {
        ft_fclose(file_pointer);
        return (std::string());
    }
    file_content.assign(static_cast<std::size_t>(file_size), '\0');
    if (file_size == 0)
    {
        ft_fclose(file_pointer);
        return (file_content);
    }
    bytes_read = std::fread(&file_content[0], 1, file_content.size(), file_pointer);
    ft_fclose(file_pointer);
    if (bytes_read != file_content.size())
        return (std::string());
    return (file_content);
}

FT_TEST(test_kv_store_flush_propagates_json_writer_errno)
{
    const char *directory_path;
    const char *file_path;
    int flush_result;

    directory_path = "kv_store_flush_failure_directory";
    file_path = "kv_store_flush_failure_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(0, store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, store.kv_set("key", "value"));
    FT_ASSERT_EQ(0, file_delete(file_path));
    remove_directory_if_present(directory_path);
    flush_result = store.kv_flush();
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, flush_result);
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_encrypted_round_trip)
{
    const char      *directory_path;
    const char      *file_path;
    const char      *encryption_key;

    directory_path = "kv_store_encrypted_directory";
    file_path = "kv_store_encrypted_directory/kv_store.json";
    encryption_key = "sixteen-byte-key";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store encrypted_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encrypted_store.initialize(file_path, encryption_key, true));
    FT_ASSERT_EQ(0, encrypted_store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, encrypted_store.kv_set("secret", "value"));
    FT_ASSERT_EQ(0, encrypted_store.kv_flush());
    kv_store reloaded_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reloaded_store.initialize(file_path, encryption_key, true));
    FT_ASSERT(reloaded_store.kv_get("secret") != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(reloaded_store.kv_get("secret"), "value"));
    std::string file_content = read_file_contents(file_path);
    FT_ASSERT(file_content.find("__encryption__") != std::string::npos);
    FT_ASSERT(file_content.find("value") == std::string::npos);
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_wrong_key_fails_to_decrypt)
{
    const char  *directory_path;
    const char  *file_path;
    const char  *encryption_key;
    const char  *wrong_key;

    directory_path = "kv_store_wrong_key_directory";
    file_path = "kv_store_wrong_key_directory/kv_store.json";
    encryption_key = "sixteen-byte-key";
    wrong_key = "sixteen-byte-zzz";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store encrypted_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encrypted_store.initialize(file_path, encryption_key, true));
    FT_ASSERT_EQ(0, encrypted_store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, encrypted_store.kv_set("secret", "value"));
    FT_ASSERT_EQ(0, encrypted_store.kv_flush());
    kv_store failing_store;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, failing_store.initialize(file_path, wrong_key, true));
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_configure_encryption_validates_key)
{
    const char  *directory_path;
    const char  *file_path;

    directory_path = "kv_store_configure_directory";
    file_path = "kv_store_configure_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store configurable_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, configurable_store.initialize(file_path));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, configurable_store.configure_encryption(ft_nullptr, true));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, configurable_store.configure_encryption("short", true));
    FT_ASSERT_EQ(0, configurable_store.configure_encryption("sixteen-byte-key", true));
    FT_ASSERT_EQ(0, configurable_store.configure_encryption(ft_nullptr, false));
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_ttl_persistence)
{
    const char  *directory_path;
    const char  *file_path;

    directory_path = "kv_store_ttl_directory";
    file_path = "kv_store_ttl_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(0, store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, store.kv_set("session", "token", 120));
    FT_ASSERT_EQ(0, store.kv_flush());
    std::string file_content = read_file_contents(file_path);
    FT_ASSERT(file_content.find("__ttl__session") != std::string::npos);
    kv_store reloaded_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reloaded_store.initialize(file_path));
    FT_ASSERT(reloaded_store.kv_get("session") != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(reloaded_store.kv_get("session"), "token"));
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_encrypted_ttl_persistence)
{
    const char  *directory_path;
    const char  *file_path;
    const char  *encryption_key;

    directory_path = "kv_store_encrypted_ttl_directory";
    file_path = "kv_store_encrypted_ttl_directory/kv_store.json";
    encryption_key = "sixteen-byte-key";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store encrypted_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encrypted_store.initialize(file_path, encryption_key, true));
    FT_ASSERT_EQ(0, encrypted_store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, encrypted_store.kv_set("session", "token", 90));
    FT_ASSERT_EQ(0, encrypted_store.kv_flush());
    std::string file_content = read_file_contents(file_path);
    FT_ASSERT(file_content.find("__ttl__session") != std::string::npos);
    kv_store reloaded_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reloaded_store.initialize(file_path, encryption_key, true));
    FT_ASSERT(reloaded_store.kv_get("session") != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(reloaded_store.kv_get("session"), "token"));
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_transactional_batch_commit)
{
    const char              *directory_path;
    const char              *file_path;
    kv_store_operation       set_alpha;
    kv_store_operation       set_beta;
    kv_store_operation       delete_placeholder;
    ft_vector<kv_store_operation> operations;
    const char              *beta_value;

    directory_path = "kv_store_transaction_directory";
    file_path = "kv_store_transaction_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(0, store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    FT_ASSERT_EQ(0, kv_store_init_set_operation(set_alpha, "alpha", "one"));
    FT_ASSERT_EQ(0, kv_store_init_set_operation(set_beta, "beta", "two", 60));
    FT_ASSERT_EQ(0, kv_store_init_delete_operation(delete_placeholder, "alpha"));
    operations.push_back(set_alpha);
    operations.push_back(set_beta);
    operations.push_back(delete_placeholder);
    FT_ASSERT_EQ(0, store.kv_apply(operations));
    FT_ASSERT_EQ(ft_nullptr, store.kv_get("alpha"));
    beta_value = store.kv_get("beta");
    FT_ASSERT(beta_value != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(beta_value, "two"));
    FT_ASSERT_EQ(0, store.kv_flush());
    kv_store reloaded;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reloaded.initialize(file_path));
    FT_ASSERT_EQ(ft_nullptr, reloaded.kv_get("alpha"));
    ft_vector<kv_store_snapshot_entry> saved_entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reloaded.export_snapshot(saved_entries));
    FT_ASSERT_EQ(1U, saved_entries.size());
    FT_ASSERT(saved_entries[0].key == "beta" && saved_entries[0].value == "two");
    FT_ASSERT_EQ(FT_TRUE, saved_entries[0].has_expiration);
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_transactional_batch_rolls_back_on_error)
{
    const char              *directory_path;
    const char              *file_path;
    kv_store_operation       set_gamma;
    kv_store_operation       delete_missing;
    ft_vector<kv_store_operation> operations;

    directory_path = "kv_store_transaction_failure_directory";
    file_path = "kv_store_transaction_failure_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(0, store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    FT_ASSERT_EQ(0, kv_store_init_set_operation(set_gamma, "gamma", "value"));
    FT_ASSERT_EQ(0, kv_store_init_delete_operation(delete_missing, "does-not-exist"));
    operations.push_back(set_gamma);
    operations.push_back(delete_missing);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_apply(operations));
    FT_ASSERT_EQ(ft_nullptr, store.kv_get("gamma"));
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_compare_and_swap_behaviour)
{
    const char *directory_path;
    const char *file_path;
    const char *value_pointer;

    directory_path = "kv_store_cas_directory";
    file_path = "kv_store_cas_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(0, store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, store.kv_set("target", "initial"));
    FT_ASSERT_EQ(0, store.kv_compare_and_swap("target", "initial", "updated"));
    value_pointer = store.kv_get("target");
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(value_pointer, "updated"));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_compare_and_swap("target", "wrong", "replacement"));
    value_pointer = store.kv_get("target");
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(value_pointer, "updated"));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_compare_and_swap("fresh", ft_nullptr, "created"));
    FT_ASSERT_EQ(ft_nullptr, store.kv_get("fresh"));
    FT_ASSERT_EQ(0, store.kv_set("fresh", "created"));
    FT_ASSERT_EQ(0, store.kv_compare_and_swap("fresh", "created", ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, store.kv_get("fresh"));
    FT_ASSERT_EQ(0, store.kv_set("ttl", "temp"));
    FT_ASSERT_EQ(0, store.kv_compare_and_swap("ttl", "temp", "renewed", 0));
    FT_ASSERT_EQ(ft_nullptr, store.kv_get("ttl"));
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_expired_entries_are_evicted)
{
    const char  *directory_path;
    const char  *file_path;

    directory_path = "kv_store_expiration_directory";
    file_path = "kv_store_expiration_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    create_kv_store_file(file_path);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(0, store.kv_delete("__placeholder__"));
    FT_ASSERT_EQ(0, store.kv_set("temporary", "value", 1));
#if defined(_WIN32) || defined(_WIN64)
    Sleep(1500);
#else
    sleep(2);
#endif
    FT_ASSERT(store.kv_get("temporary") == ft_nullptr);
    cleanup_paths(directory_path, file_path);
    return (1);
}

FT_TEST(test_kv_store_loads_legacy_format)
{
    const char  *directory_path;
    const char  *file_path;
    FILE        *file_pointer;

    directory_path = "kv_store_legacy_directory";
    file_path = "kv_store_legacy_directory/kv_store.json";
    cleanup_paths(directory_path, file_path);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0700));
    file_pointer = ft_fopen(file_path, "w");
    FT_ASSERT(file_pointer != ft_nullptr);
    std::fputs("{\n  \"kv_store\": {\n    \"legacy\": \"data\"\n  }\n}\n", file_pointer);
    ft_fclose(file_pointer);
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT(store.kv_get("legacy") != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("legacy"), "data"));
    FT_ASSERT_EQ(0, store.kv_flush());
    std::string file_content = read_file_contents(file_path);
    FT_ASSERT(file_content.find("__ttl__legacy") == std::string::npos);
    cleanup_paths(directory_path, file_path);
    return (1);
}
