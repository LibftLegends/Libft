#include "../Modules/System_utils/test_system_utils_runner.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <dbghelp.h>
#endif

namespace
{
    typedef std::filesystem::path test_path;

#if defined(_WIN32) || defined(_WIN64)
    static void test_write_windows_crash_text(HANDLE file_handle,
        const char *text)
    {
        DWORD text_length;
        DWORD bytes_written;

        if (file_handle == INVALID_HANDLE_VALUE || text == NULL)
            return ;
        text_length = static_cast<DWORD>(std::strlen(text));
        if (text_length == 0)
            return ;
        (void)WriteFile(file_handle, text, text_length, &bytes_written,
            NULL);
        return ;
    }

    static LONG WINAPI test_windows_unhandled_exception(
        EXCEPTION_POINTERS *exception_info)
    {
        HANDLE file_handle;
        HANDLE process_handle;
        char message[256];
        void *stack_frames[64];
        USHORT frame_count;
        USHORT frame_index;
        DWORD64 address;
        char symbol_storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
        SYMBOL_INFO *symbol_info;
        DWORD64 symbol_displacement;

        file_handle = CreateFileA("windows_crash_stack_trace.log",
            GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE)
            return (EXCEPTION_CONTINUE_SEARCH);
        if (exception_info != NULL && exception_info->ExceptionRecord != NULL)
        {
            (void)std::snprintf(message, sizeof(message),
                "Windows exception code: 0x%08lX\r\n",
                exception_info->ExceptionRecord->ExceptionCode);
            test_write_windows_crash_text(file_handle, message);
            (void)std::snprintf(message, sizeof(message),
                "Exception address: 0x%p\r\n",
                exception_info->ExceptionRecord->ExceptionAddress);
            test_write_windows_crash_text(file_handle, message);
        }
        process_handle = GetCurrentProcess();
        if (SymInitialize(process_handle, NULL, TRUE) == FALSE)
        {
            test_write_windows_crash_text(file_handle,
                "SymInitialize failed; raw stack follows.\r\n");
        }
        frame_count = CaptureStackBackTrace(0, 64, stack_frames, NULL);
        symbol_info = reinterpret_cast<SYMBOL_INFO *>(symbol_storage);
        symbol_info->MaxNameLen = MAX_SYM_NAME;
        symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
        frame_index = 0;
        while (frame_index < frame_count)
        {
            address = reinterpret_cast<DWORD64>(stack_frames[frame_index]);
            symbol_displacement = 0;
            if (SymFromAddr(process_handle, address, &symbol_displacement,
                    symbol_info) != FALSE)
            {
                (void)std::snprintf(message, sizeof(message),
                    "#%u 0x%llX %s+0x%llX\r\n",
                    static_cast<unsigned int>(frame_index),
                    static_cast<unsigned long long>(address),
                    symbol_info->Name,
                    static_cast<unsigned long long>(symbol_displacement));
            }
            else
            {
                (void)std::snprintf(message, sizeof(message),
                    "#%u 0x%llX\r\n",
                    static_cast<unsigned int>(frame_index),
                    static_cast<unsigned long long>(address));
            }
            test_write_windows_crash_text(file_handle, message);
            frame_index++;
        }
        (void)SymCleanup(process_handle);
        (void)CloseHandle(file_handle);
        return (EXCEPTION_EXECUTE_HANDLER);
    }
#endif

    static void test_remove_path(const test_path &path)
    {
        std::error_code error_code;
        std::filesystem::remove_all(path, error_code);
        return ;
    }

    static test_path test_find_libft_root(const test_path &executable_path)
    {
        const test_path executable_parent = executable_path.parent_path();

        if (!executable_parent.empty()
            && executable_parent.filename() == "Test")
        {
            return (executable_parent.parent_path());
        }
        return (test_path());
    }

    static void test_cleanup_runtime_artifacts(const test_path &root_path)
    {
        const char *preserve_failure_log; // CI_DIAGNOSTIC: Hold the optional failure-log preservation flag.

        if (root_path.empty())
            return ;
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
    test_path executable_path;
    test_path root_path;
    if (argc > 0 && argv != NULL && argv[0] != NULL)
    {
        executable_path = test_path(argv[0]);
    }
#if defined(_WIN32) || defined(_WIN64)
    (void)SetUnhandledExceptionFilter(test_windows_unhandled_exception);
#endif
    const int test_status = ft_run_registered_tests();
    root_path = test_find_libft_root(executable_path);
    test_cleanup_runtime_artifacts(root_path);
    return (test_status);
}
