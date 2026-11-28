#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_cmp_path_translation_preserves_separator_rules)
{
    const char *paths[] = {"", "/", "\\", "//host/share", "\\root/leaf", "C:\\world/region"};
#if defined(_WIN32) || defined(_WIN64)
    const char *native_paths[] = {"", "", "", "\\host\\share", "root\\leaf", "C:\\world\\region"};
    const char *portable_paths[] = {"", "/", "/", "//host/share", "/root/leaf", "C:/world/region"};
#else
    const char *const *native_paths = paths;
    const char *const *portable_paths = paths;
#endif
    ft_size_t index = 0;
    while (index < sizeof(paths) / sizeof(paths[0]))
    {
        char *translated = ft_nullptr;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_translate_path_to_native(paths[index], &translated));
        FT_ASSERT_STR_EQ(native_paths[index], translated);
        cma_free(translated);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_translate_path_to_portable(paths[index], &translated));
        FT_ASSERT_STR_EQ(portable_paths[index], translated);
        cma_free(translated);
        ++index;
    }
    return (1);
}

FT_TEST(test_cmp_path_translation_clears_output_on_failure)
{
    char sentinel = '\0';
    char *translated = &sentinel;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cmp_translate_path_to_native(ft_nullptr, &translated));
    FT_ASSERT(translated == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, cmp_translate_path_to_portable("world", ft_nullptr));
    return (1);
}
