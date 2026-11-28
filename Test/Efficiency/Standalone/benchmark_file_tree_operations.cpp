#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>

static int32_t create_fixture(const char *root)
{
    constexpr uint32_t DIRECTORY_COUNT = 4;
    constexpr uint32_t FILES_PER_DIRECTORY = 24;
    char relative_path[64];
    uint32_t directory_index = 0;
    while (directory_index < DIRECTORY_COUNT)
    {
        std::snprintf(relative_path, sizeof(relative_path), "region/group-%u", directory_index);
        ft_string *directory = file_path_join(root, relative_path);
        if (directory == ft_nullptr)
            return (1);
        const int32_t create_error = file_create_directories(directory->c_str(), 0700);
        if (create_error != FT_ERR_SUCCESS)
        {
            delete directory;
            return (1);
        }
        uint32_t file_index = 0;
        while (file_index < FILES_PER_DIRECTORY)
        {
            std::snprintf(relative_path, sizeof(relative_path), "r.%u.bin", file_index);
            ft_string *path = file_path_join(directory->c_str(), relative_path);
            int32_t write_error = FT_ERR_NO_MEMORY;
            if (path != ft_nullptr)
            {
                write_error = file_write_all(path->c_str(), "fixture", 7);
                delete path;
            }
            if (write_error != FT_ERR_SUCCESS)
            {
                delete directory;
                return (1);
            }
            ++file_index;
        }
        delete directory;
        ++directory_index;
    }
    return (0);
}

static int32_t measure_existing_directories(const char *root)
{
    constexpr uint32_t ITERATIONS = 1000;
    ft_string *path = file_path_join(root, "region/group-0");
    if (path == ft_nullptr)
        return (1);
    uint32_t iteration = 0;
    int32_t result = FT_ERR_SUCCESS;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && result == FT_ERR_SUCCESS)
    {
        result = file_create_directories(path->c_str(), 0700);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    delete path;
    if (result != FT_ERR_SUCCESS)
        return (1);
    std::printf("file_create_directories_existing us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_tree_transfer(const char *source, const char *destination)
{
    constexpr uint32_t ITERATIONS = 5;
    std::chrono::steady_clock::duration copy_time{};
    std::chrono::steady_clock::duration delete_time{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        const auto copy_start = std::chrono::steady_clock::now();
        if (file_copy_directory(source, destination) != FT_ERR_SUCCESS)
            return (1);
        copy_time += std::chrono::steady_clock::now() - copy_start;
        ft_string *sample = file_path_join(destination, "region/group-3/r.23.bin");
        if (sample == ft_nullptr)
            return (1);
        const file_type type = file_get_type(sample->c_str());
        delete sample;
        if (type != FILE_TYPE_REGULAR)
            return (1);
        const auto delete_start = std::chrono::steady_clock::now();
        if (file_delete_recursive(destination) != FT_ERR_SUCCESS)
            return (1);
        delete_time += std::chrono::steady_clock::now() - delete_start;
        ++iteration;
    }
    std::printf("file_copy_directory ms/op=%.3f\n",
        std::chrono::duration<double, std::milli>(copy_time).count() / ITERATIONS);
    std::printf("file_delete_recursive ms/op=%.3f\n",
        std::chrono::duration<double, std::milli>(delete_time).count() / ITERATIONS);
    return (0);
}

int main()
{
    ft_string source;
    ft_string destination;
    if (filesystem_temp_path("libft_tree_source", "dir", &source) != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_tree_destination", "dir", &destination) != FT_ERR_SUCCESS)
        return (1);
    int32_t result = create_fixture(source.c_str());
    if (result == 0)
        result = measure_existing_directories(source.c_str());
    if (result == 0)
        result = measure_tree_transfer(source.c_str(), destination.c_str());
    const int32_t source_cleanup = file_delete_recursive(source.c_str());
    const int32_t destination_cleanup = file_delete_recursive(destination.c_str());
    if (source_cleanup != FT_ERR_SUCCESS || destination_cleanup != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
