#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <inttypes.h>
#include "../Basic/basic.hpp"
#include "../File/file_utils.hpp"
#include "../Printf/printf.hpp"
#include "compatebility_stack_trace.hpp"

#if defined(_WIN32)
# include <windows.h>
# include <dbghelp.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
# include <execinfo.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
# include <dlfcn.h>
#endif

#if defined(__linux__)
static void    cmp_stack_trace_strip_line_ending(char *line)
{
    ft_size_t    line_length;

    if (line == NULL)
        return ;
    line_length = ft_strlen_size_t(line);
    while (line_length > 0
        && (line[line_length - 1] == '\n'
            || line[line_length - 1] == '\r'))
    {
        line[line_length - 1] = '\0';
        line_length -= 1;
    }
    return ;
}
static ft_size_t    cmp_shell_escape_double_quotes(char *destination,
        ft_size_t capacity, const char *source)
{
    ft_size_t    source_index;
    ft_size_t    destination_index;

    if (destination == NULL || capacity == 0 || source == NULL)
        return (0);
    source_index = 0;
    destination_index = 0;
    while (source[source_index] != '\0' && destination_index + 1 < capacity)
    {
        if ((source[source_index] == '"' || source[source_index] == '\\'
                || source[source_index] == '$'
                || source[source_index] == '`')
            && destination_index + 2 < capacity)
        {
            destination[destination_index] = '\\';
            destination_index += 1;
        }
        destination[destination_index] = source[source_index];
        destination_index += 1;
        source_index += 1;
    }
    destination[destination_index] = '\0';
    return (destination_index);
}

static int32_t    cmp_stack_trace_copy_text(char *destination,
        ft_size_t capacity, const char *source)
{
    int32_t    formatted_length;

    if (destination == NULL || source == NULL || capacity == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    formatted_length = pf_snprintf(destination, capacity, "%s", source);
    if (formatted_length < 0)
        return (FT_ERR_SYSTEM);
    if (static_cast<ft_size_t>(formatted_length) >= capacity)
        return (FT_ERR_OUT_OF_RANGE);
    return (FT_ERR_SUCCESS);
}

static int32_t    cmp_stack_trace_symbolize_linux(const void *address,
        char *symbol_buffer, ft_size_t symbol_buffer_size,
        char *location_buffer, ft_size_t location_buffer_size)
{
    Dl_info          info;
    uintptr_t        frame_address;
    uintptr_t        base_address;
    uintptr_t        relative_address;
    char             escaped_path[1024];
    char             command[1280];
    char             line_buffer[1024];
    FILE             *pipe_file;

    if (address == NULL || symbol_buffer == NULL || location_buffer == NULL
        || symbol_buffer_size == 0 || location_buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    symbol_buffer[0] = '\0';
    location_buffer[0] = '\0';
    if (dladdr(address, &info) == 0 || info.dli_fname == NULL)
        return (FT_ERR_NOT_FOUND);
    frame_address = reinterpret_cast<uintptr_t>(address);
    base_address = reinterpret_cast<uintptr_t>(info.dli_fbase);
    if (frame_address >= base_address)
        relative_address = frame_address - base_address;
    else
        relative_address = frame_address;
    cmp_shell_escape_double_quotes(escaped_path, sizeof(escaped_path),
        info.dli_fname);
    if (pf_snprintf(command, sizeof(command),
        "addr2line -f -C -e \"%s\" 0x%" PRIxPTR " 2>/dev/null",
        escaped_path, relative_address) < 0)
        return (FT_ERR_SYSTEM);
    if (ft_strlen_size_t(command) >= sizeof(command))
        return (FT_ERR_SYSTEM);
    pipe_file = popen(command, "r");
    if (pipe_file == NULL)
        return (FT_ERR_SYSTEM);
    if (ft_fgets(line_buffer, sizeof(line_buffer), pipe_file) == NULL)
    {
        pclose(pipe_file);
        return (FT_ERR_NOT_FOUND);
    }
    cmp_stack_trace_strip_line_ending(line_buffer);
    if (cmp_stack_trace_copy_text(symbol_buffer, symbol_buffer_size,
            line_buffer) != FT_ERR_SUCCESS)
    {
        pclose(pipe_file);
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (ft_fgets(line_buffer, sizeof(line_buffer), pipe_file) == NULL)
    {
        pclose(pipe_file);
        return (FT_ERR_NOT_FOUND);
    }
    cmp_stack_trace_strip_line_ending(line_buffer);
    if (cmp_stack_trace_copy_text(location_buffer, location_buffer_size,
            line_buffer) != FT_ERR_SUCCESS)
    {
        pclose(pipe_file);
        return (FT_ERR_OUT_OF_RANGE);
    }
    pclose(pipe_file);
    if (std::strcmp(symbol_buffer, "??") == 0
        && std::strcmp(location_buffer, "??:0") == 0)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

static void    cmp_stack_trace_print_linux_addr2line(FILE *output_file,
        void *frame, ft_size_t frame_index)
{
    char             symbol_buffer[1024];
    char             location_buffer[1024];

    if (cmp_stack_trace_symbolize_address(frame, symbol_buffer,
            sizeof(symbol_buffer), location_buffer,
            sizeof(location_buffer)) != FT_ERR_SUCCESS)
    {
        ft_fprintf(output_file, "    #%zu %p\n", frame_index, frame);
        return ;
    }
    ft_fprintf(output_file, "    #%zu %s\n       %s\n", frame_index,
        symbol_buffer, location_buffer);
    return ;
}
#endif

#if defined(_WIN32)
void    dbg_print_stack_trace(void) noexcept;
static INIT_ONCE    g_cmp_stack_trace_windows_symbols_once = {};
static ft_bool      g_cmp_stack_trace_windows_symbols_ready = FT_FALSE;

static int32_t    cmp_stack_trace_symbolize_windows_addr2line(const void *address,
        char *symbol_buffer, ft_size_t symbol_buffer_size,
        char *location_buffer, ft_size_t location_buffer_size)
{
    char    executable_path[MAX_PATH];
    char    command[1280];
    char    line_buffer[1024];
    DWORD64 module_base;
    DWORD64 preferred_base;
    DWORD64 frame_address;
    DWORD64 relative_address;
    FILE    *pipe_file;
    ft_size_t line_length;
    int32_t formatted_length;

    if (address == NULL || symbol_buffer == NULL || location_buffer == NULL
        || symbol_buffer_size == 0 || location_buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    symbol_buffer[0] = '\0';
    location_buffer[0] = '\0';
    executable_path[0] = '\0';
    if (GetModuleFileNameA(NULL, executable_path, MAX_PATH) == 0)
        return (FT_ERR_NOT_FOUND);
    module_base = reinterpret_cast<DWORD64>(GetModuleHandleA(NULL));
#if defined(_WIN64)
    preferred_base = 0x0000000140000000ULL;
#else
    preferred_base = 0x00400000ULL;
#endif

    frame_address = reinterpret_cast<DWORD64>(address);
    relative_address = frame_address;
    if (module_base != 0 && frame_address >= module_base)
        relative_address = frame_address - module_base + preferred_base;
    formatted_length = pf_snprintf(command, sizeof(command),
        "addr2line -f -C -e \"%s\" 0x%" PRIx64 " 2>NUL",
        executable_path, static_cast<uint64_t>(relative_address));
    if (formatted_length < 0)
        return (FT_ERR_SYSTEM);
    if (ft_strlen_size_t(command) >= sizeof(command))
        return (FT_ERR_SYSTEM);
    pipe_file = popen(command, "r");
    if (pipe_file == NULL)
        return (FT_ERR_NOT_FOUND);
    if (ft_fgets(line_buffer, sizeof(line_buffer), pipe_file) == NULL)
    {
        pclose(pipe_file);
        return (FT_ERR_NOT_FOUND);
    }
    line_length = ft_strlen_size_t(line_buffer);
    while (line_length > 0
        && (line_buffer[line_length - 1] == '\n'
            || line_buffer[line_length - 1] == '\r'))
    {
        line_buffer[line_length - 1] = '\0';
        line_length -= 1;
    }
    formatted_length = pf_snprintf(symbol_buffer, symbol_buffer_size, "%s",
            line_buffer);
    if (formatted_length < 0
        || static_cast<ft_size_t>(formatted_length) >= symbol_buffer_size)
    {
        pclose(pipe_file);
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (ft_fgets(line_buffer, sizeof(line_buffer), pipe_file) == NULL)
    {
        pclose(pipe_file);
        return (FT_ERR_NOT_FOUND);
    }
    line_length = ft_strlen_size_t(line_buffer);
    while (line_length > 0
        && (line_buffer[line_length - 1] == '\n'
            || line_buffer[line_length - 1] == '\r'))
    {
        line_buffer[line_length - 1] = '\0';
        line_length -= 1;
    }
    formatted_length = pf_snprintf(location_buffer, location_buffer_size, "%s",
            line_buffer);
    if (formatted_length < 0
        || static_cast<ft_size_t>(formatted_length) >= location_buffer_size)
    {
        pclose(pipe_file);
        return (FT_ERR_OUT_OF_RANGE);
    }
    pclose(pipe_file);
    return (FT_ERR_SUCCESS);
}

static BOOL CALLBACK    cmp_stack_trace_initialize_windows_symbols(
        PINIT_ONCE, PVOID, PVOID *)
{
    HANDLE    process_handle;

    process_handle = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if (SymInitialize(process_handle, NULL, TRUE) == FALSE)
        g_cmp_stack_trace_windows_symbols_ready = FT_FALSE;
    else
        g_cmp_stack_trace_windows_symbols_ready = FT_TRUE;
    return (TRUE);
}

static ft_bool    cmp_stack_trace_windows_symbols_are_ready(void)
{
    if (InitOnceExecuteOnce(&g_cmp_stack_trace_windows_symbols_once,
            cmp_stack_trace_initialize_windows_symbols, NULL, NULL) == FALSE)
        return (FT_FALSE);
    return (g_cmp_stack_trace_windows_symbols_ready);
}

static ft_size_t    cmp_stack_trace_capture_windows(void **frames,
        ft_size_t capacity, ft_size_t skip_count)
{
    USHORT    captured_count;
    ULONG     capture_capacity;

    if (frames == NULL || capacity == 0)
        return (0);
    if (capacity > static_cast<ft_size_t>(USHRT_MAX))
        capture_capacity = static_cast<ULONG>(USHRT_MAX);
    else
        capture_capacity = static_cast<ULONG>(capacity);
    if (skip_count > static_cast<ft_size_t>(ULONG_MAX))
        skip_count = static_cast<ft_size_t>(ULONG_MAX);
    captured_count = RtlCaptureStackBackTrace(static_cast<ULONG>(skip_count),
            capture_capacity, reinterpret_cast<PVOID *>(frames), NULL);
    return (static_cast<ft_size_t>(captured_count));
}

static int32_t    cmp_stack_trace_symbolize_windows(const void *address,
        char *symbol_buffer, ft_size_t symbol_buffer_size,
        char *location_buffer, ft_size_t location_buffer_size)
{
    HANDLE            process_handle;
    DWORD64           frame_address;
    DWORD64           symbol_displacement;
    DWORD             line_displacement;
    IMAGEHLP_LINE64   line_information;
    unsigned char     symbol_storage[sizeof(SYMBOL_INFO)
                        + MAX_SYM_NAME * sizeof(char)];
    PSYMBOL_INFO      symbol_information;
    int32_t           formatted_length;

    if (address == NULL || symbol_buffer == NULL || location_buffer == NULL
        || symbol_buffer_size == 0 || location_buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    symbol_buffer[0] = '\0';
    location_buffer[0] = '\0';
    if (cmp_stack_trace_symbolize_windows_addr2line(address, symbol_buffer,
            symbol_buffer_size, location_buffer, location_buffer_size)
        == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    if (cmp_stack_trace_windows_symbols_are_ready() == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    process_handle = GetCurrentProcess();
    frame_address = reinterpret_cast<DWORD64>(address);
    ZeroMemory(symbol_storage, sizeof(symbol_storage));
    symbol_information = reinterpret_cast<PSYMBOL_INFO>(symbol_storage);
    symbol_information->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol_information->MaxNameLen = MAX_SYM_NAME;
    symbol_displacement = 0;
    if (SymFromAddr(process_handle, frame_address, &symbol_displacement,
            symbol_information) == FALSE)
        symbol_information = NULL;
    if (symbol_information != NULL)
    {
        if (symbol_displacement > 0)
        {
            formatted_length = pf_snprintf(symbol_buffer, symbol_buffer_size,
                    "%s + 0x%" PRIx64, symbol_information->Name,
                    static_cast<uint64_t>(symbol_displacement));
        }
        else
        {
            formatted_length = pf_snprintf(symbol_buffer, symbol_buffer_size,
                    "%s", symbol_information->Name);
        }
        if (formatted_length < 0
            || static_cast<ft_size_t>(formatted_length) >= symbol_buffer_size)
            return (FT_ERR_OUT_OF_RANGE);
    }
    else
    {
        formatted_length = pf_snprintf(symbol_buffer, symbol_buffer_size,
                "%p", address);
        if (formatted_length < 0
            || static_cast<ft_size_t>(formatted_length) >= symbol_buffer_size)
            return (FT_ERR_OUT_OF_RANGE);
    }
    ZeroMemory(&line_information, sizeof(line_information));
    line_information.SizeOfStruct = sizeof(line_information);
    line_displacement = 0;
    if (SymGetLineFromAddr64(process_handle, frame_address, &line_displacement,
            &line_information) != FALSE && line_information.FileName != NULL)
    {
        if (line_information.LineNumber > 0)
        {
            formatted_length = pf_snprintf(location_buffer,
                    location_buffer_size, "%s:%lu",
                    line_information.FileName,
                    static_cast<unsigned long>(line_information.LineNumber));
        }
        else
        {
            formatted_length = pf_snprintf(location_buffer,
                    location_buffer_size, "%s", line_information.FileName);
        }
        if (formatted_length < 0
            || static_cast<ft_size_t>(formatted_length) >= location_buffer_size)
            return (FT_ERR_OUT_OF_RANGE);
    }
    return (FT_ERR_SUCCESS);
}
#endif

#if defined(__APPLE__)
static int32_t    cmp_stack_trace_symbolize_macos(const void *address,
        char *symbol_buffer, ft_size_t symbol_buffer_size,
        char *location_buffer, ft_size_t location_buffer_size)
{
    Dl_info info;
    const char *symbol_name;
    int32_t formatted_length;

    if (address == NULL || symbol_buffer == NULL || location_buffer == NULL
        || symbol_buffer_size == 0 || location_buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    symbol_buffer[0] = '\0';
    location_buffer[0] = '\0';
    if (dladdr(address, &info) == 0 || info.dli_sname == NULL)
        return (FT_ERR_NOT_FOUND);
    symbol_name = info.dli_sname;
    if (symbol_name[0] == '_')
        symbol_name += 1;
    formatted_length = pf_snprintf(symbol_buffer, symbol_buffer_size,
        "%s", symbol_name);
    if (formatted_length < 0
        || static_cast<ft_size_t>(formatted_length) >= symbol_buffer_size)
        return (FT_ERR_OUT_OF_RANGE);
    return (FT_ERR_SUCCESS);
}
#endif

ft_size_t    cmp_stack_trace_capture(void **frames, ft_size_t capacity,
        ft_size_t skip_count)
{
#if defined(__linux__) || defined(__APPLE__)
    void    *captured_frames[CMP_STACK_TRACE_MAX_FRAMES + 8];
    ft_size_t    capture_capacity;
    int32_t    captured_count;
    ft_size_t    output_index;

    if (frames == NULL || capacity == 0)
        return (0);
    capture_capacity = capacity + skip_count;
    if (capture_capacity > CMP_STACK_TRACE_MAX_FRAMES + 8)
        capture_capacity = CMP_STACK_TRACE_MAX_FRAMES + 8;
    captured_count = backtrace(captured_frames, static_cast<int>(capture_capacity));
    if (captured_count <= 0)
        return (0);
    output_index = 0;
    while (static_cast<ft_size_t>(captured_count) > skip_count
        && output_index + skip_count < static_cast<ft_size_t>(captured_count)
        && output_index < capacity)
    {
        frames[output_index] = captured_frames[output_index + skip_count];
        output_index += 1;
    }
    return (output_index);
#elif defined(_WIN32)
    return (cmp_stack_trace_capture_windows(frames, capacity, skip_count));
#else
    (void)frames;
    (void)capacity;
    (void)skip_count;
    return (0);
#endif
}

void    cmp_stack_trace_print(FILE *output_file, void *const *frames,
        ft_size_t frame_count)
{
#if defined(__linux__)
    ft_size_t    frame_index;

    if (output_file == NULL || frames == NULL || frame_count == 0)
        return ;
    frame_index = 0;
    while (frame_index < frame_count)
    {
        cmp_stack_trace_print_linux_addr2line(output_file, frames[frame_index],
            frame_index);
        frame_index += 1;
    }
#elif defined(__APPLE__)
    char    **symbols;
    ft_size_t    frame_index;

    if (output_file == NULL || frames == NULL || frame_count == 0)
        return ;
    symbols = backtrace_symbols(const_cast<void *const *>(frames),
            static_cast<int>(frame_count));
    if (symbols == NULL)
    {
        frame_index = 0;
        while (frame_index < frame_count)
        {
            ft_fprintf(output_file, "    #%" PRIu64 " %p\n",
                static_cast<uint64_t>(frame_index),
                frames[frame_index]);
            frame_index += 1;
        }
        return ;
    }
    frame_index = 0;
    while (frame_index < frame_count)
    {
        ft_fprintf(output_file, "    #%" PRIu64 " %s\n",
            static_cast<uint64_t>(frame_index),
            symbols[frame_index]);
        frame_index += 1;
    }
    free(symbols);
#elif defined(_WIN32)
    char         symbol_buffer[1024];
    char         location_buffer[1024];
    ft_size_t    frame_index;

    if (output_file == NULL || frames == NULL || frame_count == 0)
        return ;
    frame_index = 0;
    while (frame_index < frame_count)
    {
        if (cmp_stack_trace_symbolize_address(frames[frame_index],
                symbol_buffer, sizeof(symbol_buffer), location_buffer,
                sizeof(location_buffer)) != FT_ERR_SUCCESS)
        {
            ft_fprintf(output_file, "    #%" PRIu64 " %p\n",
                static_cast<uint64_t>(frame_index), frames[frame_index]);
        }
        else if (location_buffer[0] != '\0')
        {
            ft_fprintf(output_file, "    #%" PRIu64 " %s\n       %s\n",
                static_cast<uint64_t>(frame_index), symbol_buffer,
                location_buffer);
        }
        else
        {
            ft_fprintf(output_file, "    #%" PRIu64 " %s\n",
                static_cast<uint64_t>(frame_index), symbol_buffer);
        }
        frame_index += 1;
    }
#else
    ft_size_t    frame_index;

    if (output_file == NULL || frames == NULL || frame_count == 0)
        return ;
    frame_index = 0;
    while (frame_index < frame_count)
    {
        ft_fprintf(output_file, "    #%" PRIu64 " %p\n",
            static_cast<uint64_t>(frame_index),
            frames[frame_index]);
        frame_index += 1;
    }
#endif
    return ;
}

int32_t    cmp_stack_trace_symbolize_address(const void *address,
        char *symbol_buffer, ft_size_t symbol_buffer_size,
        char *location_buffer, ft_size_t location_buffer_size)
{
#if defined(__linux__)
    if (address == NULL || symbol_buffer == NULL || location_buffer == NULL
        || symbol_buffer_size == 0 || location_buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    return (cmp_stack_trace_symbolize_linux(address, symbol_buffer,
        symbol_buffer_size, location_buffer, location_buffer_size));
#elif defined(_WIN32)
    return (cmp_stack_trace_symbolize_windows(address, symbol_buffer,
        symbol_buffer_size, location_buffer, location_buffer_size));
#elif defined(__APPLE__)
    return (cmp_stack_trace_symbolize_macos(address, symbol_buffer,
        symbol_buffer_size, location_buffer, location_buffer_size));
#else
    (void)address;
    if (symbol_buffer != NULL && symbol_buffer_size > 0)
        symbol_buffer[0] = '\0';
    if (location_buffer != NULL && location_buffer_size > 0)
        location_buffer[0] = '\0';
    return (FT_ERR_INVALID_OPERATION);
#endif
}
