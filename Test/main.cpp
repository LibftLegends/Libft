#include "../Modules/System_utils/test_system_utils_runner.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>
#if !defined(_WIN32) && !defined(_WIN64)
# include <filesystem>
#endif
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <dbghelp.h>
# include <cwchar>
#endif

namespace
{
#if !defined(_WIN32) && !defined(_WIN64)
    typedef std::filesystem::path test_path;
#endif

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

#if !defined(_WIN32) && !defined(_WIN64)
    static void test_remove_path(const test_path &path)
    {
        std::error_code error_code;
        std::filesystem::remove_all(path, error_code);
        return ;
    }
#endif

#if !defined(_WIN32) && !defined(_WIN64)
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
#endif

#if defined(_WIN32) || defined(_WIN64)
    static void test_windows_report_cleanup_failure(
        const std::wstring &path, const wchar_t *operation, DWORD error_code)
    {
        std::fwprintf(stderr,
            L"[Windows test cleanup] %ls failed for %ls (error %lu)\n",
            operation, path.c_str(), static_cast<unsigned long>(error_code));
        return ;
    }

    static int32_t test_windows_get_module_path(std::wstring *path_out)
    {
        std::vector<wchar_t> path_buffer;
        DWORD path_length;
        DWORD buffer_size;

        if (path_out == NULL)
            return (0);
        buffer_size = 512U;
        while (buffer_size <= 32768U)
        {
            path_buffer.resize(buffer_size);
            path_length = GetModuleFileNameW(NULL, path_buffer.data(),
                buffer_size);
            if (path_length == 0U)
                return (0);
            if (path_length + 1U < buffer_size)
            {
                path_out->assign(path_buffer.data(), path_length);
                return (1);
            }
            buffer_size *= 2U;
        }
        return (0);
    }

    static int32_t test_windows_make_full_path(std::wstring *path)
    {
        std::vector<wchar_t> path_buffer;
        DWORD path_length;
        DWORD buffer_size;

        if (path == NULL || path->empty())
            return (0);
        buffer_size = 512U;
        while (buffer_size <= 32768U)
        {
            path_buffer.resize(buffer_size);
            path_length = GetFullPathNameW(path->c_str(), buffer_size,
                path_buffer.data(), NULL);
            if (path_length == 0U)
                return (0);
            if (path_length < buffer_size)
            {
                path->assign(path_buffer.data(), path_length);
                return (1);
            }
            buffer_size = path_length + 1U;
        }
        return (0);
    }

    static std::wstring test_windows_extended_path(const std::wstring &path)
    {
        if (path.compare(0, 4, L"\\\\?\\") == 0)
            return (path);
        if (path.compare(0, 2, L"\\\\") == 0)
            return (L"\\\\?\\UNC\\" + path.substr(2));
        return (L"\\\\?\\" + path);
    }

    static int32_t test_windows_get_trusted_root(std::wstring *root_out)
    {
        std::wstring module_path;
        std::wstring test_directory;
        std::wstring directory_name;
        std::wstring root_path;
        std::wstring::size_type separator_index;
        std::wstring::size_type parent_separator_index;

        if (root_out == NULL)
            return (0);
        if (test_windows_get_module_path(&module_path) == 0
            || test_windows_make_full_path(&module_path) == 0)
            return (0);
        separator_index = module_path.find_last_of(L"\\/");
        if (separator_index == std::wstring::npos)
            return (0);
        test_directory = module_path.substr(0, separator_index);
        parent_separator_index = test_directory.find_last_of(L"\\/");
        if (parent_separator_index == std::wstring::npos)
            return (0);
        directory_name = test_directory.substr(parent_separator_index + 1);
        if (CompareStringOrdinal(directory_name.c_str(),
                static_cast<int>(directory_name.size()), L"Test", 4, TRUE)
            != CSTR_EQUAL)
            return (0);
        root_path = test_directory.substr(0, parent_separator_index);
        if (root_path.empty())
            return (0);
        *root_out = test_windows_extended_path(root_path);
        return (1);
    }

    static int32_t test_windows_path_is_within_root(
        const std::wstring &path, const std::wstring &root_path)
    {
        std::wstring::size_type root_length;
        int32_t comparison_result;

        root_length = root_path.size();
        if (path.size() < root_length)
            return (0);
        comparison_result = CompareStringOrdinal(path.c_str(),
            static_cast<int>(root_length), root_path.c_str(),
            static_cast<int>(root_length), TRUE);
        if (comparison_result != CSTR_EQUAL)
            return (0);
        if (path.size() == root_length)
            return (1);
        if (path[root_length] != L'\\' && path[root_length] != L'/')
            return (0);
        return (1);
    }

    static int32_t test_remove_windows_path(const std::wstring &path,
        const std::wstring &root_path)
    {
        DWORD file_attributes;
        WIN32_FIND_DATAW find_data;
        HANDLE find_handle;
        std::wstring search_path;
        std::wstring child_path;
        int32_t cleanup_status;
        DWORD last_error;

        if (test_windows_path_is_within_root(path, root_path) == 0)
        {
            test_windows_report_cleanup_failure(path,
                L"rejecting path outside trusted root", ERROR_ACCESS_DENIED);
            return (0);
        }
        file_attributes = GetFileAttributesW(path.c_str());
        if (file_attributes == INVALID_FILE_ATTRIBUTES)
        {
            last_error = GetLastError();
            if (last_error == ERROR_FILE_NOT_FOUND
                || last_error == ERROR_PATH_NOT_FOUND)
                return (1);
            test_windows_report_cleanup_failure(path,
                L"GetFileAttributesW", last_error);
            return (0);
        }
        if ((file_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U)
        {
            if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)
                cleanup_status = RemoveDirectoryW(path.c_str());
            else
                cleanup_status = DeleteFileW(path.c_str());
            if (cleanup_status == 0)
            {
                last_error = GetLastError();
                test_windows_report_cleanup_failure(path,
                    L"removing reparse point", last_error);
                return (0);
            }
            return (1);
        }
        if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U)
        {
            if ((file_attributes & FILE_ATTRIBUTE_READONLY) != 0U
                && SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL) == 0)
            {
                last_error = GetLastError();
                test_windows_report_cleanup_failure(path,
                    L"SetFileAttributesW", last_error);
                return (0);
            }
            cleanup_status = DeleteFileW(path.c_str());
            if (cleanup_status == 0)
            {
                last_error = GetLastError();
                test_windows_report_cleanup_failure(path,
                    L"DeleteFileW", last_error);
                return (0);
            }
            return (1);
        }
        search_path = path + L"\\*";
        find_handle = FindFirstFileW(search_path.c_str(), &find_data);
        if (find_handle == INVALID_HANDLE_VALUE)
        {
            last_error = GetLastError();
            test_windows_report_cleanup_failure(path,
                L"FindFirstFileW", last_error);
            return (0);
        }
        cleanup_status = 1;
        do
        {
            if (std::wcscmp(find_data.cFileName, L".") != 0
                && std::wcscmp(find_data.cFileName, L"..") != 0)
            {
                child_path = path + L"\\" + find_data.cFileName;
                if (test_remove_windows_path(child_path, root_path) == 0)
                    cleanup_status = 0;
            }
        }
        while (FindNextFileW(find_handle, &find_data) != 0);
        last_error = GetLastError();
        if (last_error != ERROR_NO_MORE_FILES)
        {
            test_windows_report_cleanup_failure(path,
                L"FindNextFileW", last_error);
            cleanup_status = 0;
        }
        if (FindClose(find_handle) == 0)
        {
            last_error = GetLastError();
            test_windows_report_cleanup_failure(path,
                L"FindClose", last_error);
            cleanup_status = 0;
        }
        if (SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL) == 0)
        {
            last_error = GetLastError();
            test_windows_report_cleanup_failure(path,
                L"SetFileAttributesW", last_error);
            cleanup_status = 0;
        }
        if (RemoveDirectoryW(path.c_str()) == 0)
        {
            last_error = GetLastError();
            test_windows_report_cleanup_failure(path,
                L"RemoveDirectoryW", last_error);
            cleanup_status = 0;
        }
        return (cleanup_status);
    }

    static int32_t test_cleanup_runtime_artifacts(void)
    {
        const char *preserve_failure_log;
        std::wstring root_path;
        std::wstring artifact_path;
        int32_t cleanup_status;

        if (test_windows_get_trusted_root(&root_path) == 0)
        {
            std::fwprintf(stderr,
                L"[Windows test cleanup] could not determine trusted root\n");
            return (0);
        }
        cleanup_status = 1;
        preserve_failure_log = std::getenv("FT_TEST_PRESERVE_FAILURE_LOG");
        if (preserve_failure_log == NULL || std::string(preserve_failure_log) != "1")
        {
            artifact_path = root_path + L"\\test_failures.log";
            if (test_remove_windows_path(artifact_path, root_path) == 0)
                cleanup_status = 0;
        }
        artifact_path = root_path + L"\\test_file_io.txt";
        if (test_remove_windows_path(artifact_path, root_path) == 0)
            cleanup_status = 0;
        artifact_path = root_path + L"\\test_cmp_system_io.txt";
        if (test_remove_windows_path(artifact_path, root_path) == 0)
            cleanup_status = 0;
        artifact_path = root_path + L"\\test_su_file_stream.txt";
        if (test_remove_windows_path(artifact_path, root_path) == 0)
            cleanup_status = 0;
        artifact_path = root_path + L"\\Test\\tmp_json_stream_reader.json";
        if (test_remove_windows_path(artifact_path, root_path) == 0)
            cleanup_status = 0;
        artifact_path = root_path + L"\\Test\\tmp";
        if (test_remove_windows_path(artifact_path, root_path) == 0)
            cleanup_status = 0;
        return (cleanup_status);
    }

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
# define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2U
#endif

    FT_TEST(test_windows_cleanup_reparse_point_safety)
    {
        std::wstring root_path;
        std::wstring temporary_directory;
        std::wstring target_directory;
        std::wstring target_file;
        std::wstring link_path;
        HANDLE file_handle;
        DWORD last_error;
        DWORD file_attributes;
        int32_t test_status;

        if (test_windows_get_trusted_root(&root_path) == 0)
            return (0);
        temporary_directory.resize(MAX_PATH);
        if (GetTempPathW(static_cast<DWORD>(temporary_directory.size()),
                &temporary_directory[0]) == 0)
            return (0);
        temporary_directory.resize(std::wcslen(temporary_directory.c_str()));
        target_directory = temporary_directory + L"libft_cleanup_target_"
            + std::to_wstring(GetCurrentProcessId());
        target_file = target_directory + L"\\sentinel.txt";
        link_path = root_path + L"\\Test\\tmp\\windows_cleanup_link_"
            + std::to_wstring(GetCurrentProcessId());
        (void)CreateDirectoryW((root_path + L"\\Test").c_str(), NULL);
        (void)CreateDirectoryW((root_path + L"\\Test\\tmp").c_str(), NULL);
        (void)RemoveDirectoryW(link_path.c_str());
        (void)RemoveDirectoryW(target_directory.c_str());
        if (CreateDirectoryW(target_directory.c_str(), NULL) == 0)
            return (0);
        file_handle = CreateFileW(target_file.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            (void)RemoveDirectoryW(target_directory.c_str());
            return (0);
        }
        (void)CloseHandle(file_handle);
        if (CreateSymbolicLinkW(link_path.c_str(), target_directory.c_str(),
                SYMBOLIC_LINK_FLAG_DIRECTORY
                | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0)
        {
            last_error = GetLastError();
            std::fwprintf(stderr,
                L"[Windows test cleanup] symbolic-link test skipped (error %lu)\n",
                static_cast<unsigned long>(last_error));
            (void)DeleteFileW(target_file.c_str());
            (void)RemoveDirectoryW(target_directory.c_str());
            return (1);
        }
        test_status = test_remove_windows_path(link_path, root_path);
        file_attributes = GetFileAttributesW(target_file.c_str());
        if (file_attributes == INVALID_FILE_ATTRIBUTES)
            test_status = 0;
        if (GetFileAttributesW(link_path.c_str()) != INVALID_FILE_ATTRIBUTES)
            test_status = 0;
        (void)DeleteFileW(target_file.c_str());
        (void)RemoveDirectoryW(target_directory.c_str());
        FT_ASSERT(test_status != 0);
        return (1);
    }
#else
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
#endif

}

int main(int argc, char **argv)
{
#if !defined(_WIN32) && !defined(_WIN64)
    test_path executable_path;
    test_path root_path;
#endif
    if (argc > 0 && argv != NULL && argv[0] != NULL)
    {
#if !defined(_WIN32) && !defined(_WIN64)
        executable_path = test_path(argv[0]);
#endif
    }
#if defined(_WIN32) || defined(_WIN64)
    (void)SetUnhandledExceptionFilter(test_windows_unhandled_exception);
#endif
    const int test_status = ft_run_registered_tests();
#if !defined(_WIN32) && !defined(_WIN64)
    root_path = test_find_libft_root(executable_path);
    test_cleanup_runtime_artifacts(root_path);
#else
    (void)test_cleanup_runtime_artifacts();
#endif
    return (test_status);
}
