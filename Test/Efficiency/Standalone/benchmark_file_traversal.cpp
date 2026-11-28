#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>

static constexpr uint32_t DIRECTORY_COUNT = 8;
static constexpr uint32_t FILES_PER_DIRECTORY = 48;

static int32_t create_fixture(const char *root)
{
    char relative_path[48];
    uint32_t directory = 0;
    while (directory < DIRECTORY_COUNT)
    {
        std::snprintf(relative_path, sizeof(relative_path), "group-%u", directory);
        ft_string *path = file_path_join(root, relative_path);
        if (path == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        const int32_t create_error = file_create_directories(path->c_str(), 0700);
        delete path;
        if (create_error != FT_ERR_SUCCESS)
            return (create_error);
        uint32_t file_index = 0;
        while (file_index < FILES_PER_DIRECTORY)
        {
            std::snprintf(relative_path, sizeof(relative_path), "group-%u/region-%u.bin",
                directory, file_index);
            path = file_path_join(root, relative_path);
            if (path == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            const int32_t write_error = file_write_all(path->c_str(), "x", 1);
            delete path;
            if (write_error != FT_ERR_SUCCESS)
                return (write_error);
            ++file_index;
        }
        ++directory;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t count_entry(const char *, ft_bool, void *context)
{
    ++*static_cast<uint32_t *>(context);
    return (FT_ERR_SUCCESS);
}

static int32_t measure_traversal(const char *root, ft_bool use_listing)
{
    constexpr uint32_t ITERATIONS = 24;
    constexpr uint32_t EXPECTED_ENTRIES = DIRECTORY_COUNT * (1U + FILES_PER_DIRECTORY);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (use_listing == FT_TRUE)
        {
            ft_vector<ft_string> entries;
            if (entries.initialize() != FT_ERR_SUCCESS
                || file_list_directory_recursive(root, entries, FILE_LIST_INCLUDE_ALL) != FT_ERR_SUCCESS
                || entries.size() != EXPECTED_ENTRIES)
                return (1);
        }
        else
        {
            uint32_t entries = 0;
            if (filesystem_walk_recursive(root, count_entry, &entries) != FT_ERR_SUCCESS
                || entries != EXPECTED_ENTRIES)
                return (1);
        }
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const char *operation = "walk";
    if (use_listing == FT_TRUE)
        operation = "list";
    std::printf("file_%s_recursive entries=%u ms/op=%.3f\n", operation, EXPECTED_ENTRIES,
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}

int main()
{
    ft_string root;
    if (root.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_benchmark_directory", "dir", &root) != FT_ERR_SUCCESS
        || file_create_directories(root.c_str(), 0700) != FT_ERR_SUCCESS)
        return (1);
    int32_t result = create_fixture(root.c_str());
    if (result == FT_ERR_SUCCESS)
        result = measure_traversal(root.c_str(), FT_TRUE);
    if (result == FT_ERR_SUCCESS)
        result = measure_traversal(root.c_str(), FT_FALSE);
    if (file_delete_recursive(root.c_str()) != FT_ERR_SUCCESS || result != FT_ERR_SUCCESS)
        return (1);
    return (0);
}
