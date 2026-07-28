#include "../Modules/System_utils/test_system_utils_runner.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{
    typedef std::filesystem::path test_path;

    static void test_remove_path(const test_path &path)
    {
        std::error_code error_code;
        std::filesystem::remove_all(path, error_code);
        return ;
    }

    static test_path test_find_libft_root(const test_path &executable_path)
    {
        std::error_code error_code;
        const test_path current_path = std::filesystem::current_path(
            error_code);
        if (!error_code && std::filesystem::is_directory(current_path / "Test")
            && std::filesystem::is_directory(current_path / "Modules"))
        {
            return (current_path);
        }

        const test_path executable_parent = executable_path.parent_path();
        if (!executable_parent.empty()
            && executable_parent.filename() == "Test")
        {
            return (executable_parent.parent_path());
        }
        if (current_path.filename() == "Test")
        {
            return (current_path.parent_path());
        }
        return (current_path);
    }

    static void test_cleanup_runtime_artifacts(const test_path &root_path)
    {
        const char *preserve_failure_log; // CI_DIAGNOSTIC: Hold the optional failure-log preservation flag.

        preserve_failure_log = std::getenv("FT_TEST_PRESERVE_FAILURE_LOG"); // CI_DIAGNOSTIC: Preserve failure details for CI diagnosis.
        if (preserve_failure_log == NULL || std::string(preserve_failure_log) != "1") // CI_DIAGNOSTIC: Keep normal local cleanup unchanged.
            test_remove_path(root_path / "test_failures.log"); // CI_DIAGNOSTIC: Remove the diagnostic log outside CI diagnosis.
        test_remove_path(root_path / "test_file_io.txt");
        test_remove_path(root_path / "test_cmp_system_io.txt");
        test_remove_path(root_path / "test_su_file_stream.txt");
        test_remove_path(root_path / "Test" / "tmp_json_stream_reader.json");
        test_remove_path(root_path / "Test" / "tmp");
        return ;
    }

}

int main(int argc, char **argv)
{
    std::error_code error_code;
    test_path executable_path;
    // TEMP_DIAGNOSTIC_REMOVE: Delete main-entry tracing after diagnosis.
    ft_test_runner_write_startup_diagnostic("main:entered\n");
    if (argc > 0 && argv != NULL && argv[0] != NULL)
    {
        executable_path = std::filesystem::absolute(argv[0], error_code);
    }
    const test_path root_path = test_find_libft_root(executable_path);
    const int test_status = ft_run_registered_tests();
    test_cleanup_runtime_artifacts(root_path);
    return (test_status);
}
