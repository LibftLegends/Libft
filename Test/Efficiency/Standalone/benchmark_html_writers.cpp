#include "../../../Modules/HTML/html_parser.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/System_utils/system_utils.hpp"
#include "../../../Modules/System_utils/test_system_utils_runner.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>
#include <string>

static ft_size_t intercepted_writes;
static ft_size_t intercepted_bytes;

static int64_t count_html_write(int32_t, const void *, ft_size_t size)
{
    ++intercepted_writes;
    intercepted_bytes += size;
    return (static_cast<int64_t>(size));
}

int main()
{
    constexpr uint32_t NODE_COUNT = 128;
    constexpr uint32_t WRITES = 16;
    constexpr uint32_t SERIALIZATIONS = 128;
    constexpr char OUTPUT_PATH[] = "benchmark-html-writer.tmp";
    char root_tag[] = "main";
    char child_tag[] = "p";
    char attribute_key[] = "class";
    char attribute_value[] = "record";
    std::string text(512, 'x');
    html_attr attribute{attribute_key, attribute_value, nullptr};
    html_node children[NODE_COUNT]{};
    uint32_t index = 0;
    while (index < NODE_COUNT - 1)
    {
        children[index] = {child_tag, text.data(), &attribute, nullptr, &children[index + 1]};
        ++index;
    }
    children[index] = {child_tag, text.data(), &attribute, nullptr, nullptr};
    html_node root{root_tag, nullptr, nullptr, children, nullptr};

    ft_size_t bytes = 0;
    const auto string_start = std::chrono::steady_clock::now();
    index = 0;
    while (index < SERIALIZATIONS)
    {
        char *serialized = html_write_to_string(&root);
        if (serialized == nullptr)
            return (1);
        bytes = ft_strlen_size_t(serialized);
        cma_free(serialized);
        ++index;
    }
    const auto string_elapsed = std::chrono::steady_clock::now() - string_start;

    su_set_write_syscall_hook(count_html_write);
    const auto file_start = std::chrono::steady_clock::now();
    index = 0;
    while (index < WRITES && html_write_to_file(OUTPUT_PATH, &root) == FT_ERR_SUCCESS)
        ++index;
    const auto file_elapsed = std::chrono::steady_clock::now() - file_start;
    su_reset_write_syscall_hook();
    if (std::remove(OUTPUT_PATH) != 0 || index != WRITES || intercepted_bytes != bytes * WRITES)
        return (1);
    std::printf("string=%.3f us file=%.3f us writes=%" PRIu64 " bytes=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(string_elapsed).count() / SERIALIZATIONS,
        std::chrono::duration<double, std::micro>(file_elapsed).count() / WRITES,
        static_cast<uint64_t>(intercepted_writes / WRITES), static_cast<uint64_t>(bytes));
    return (0);
}
