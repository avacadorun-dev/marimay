#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mmay/platform/process.h"

#include <stdbool.h>
#include <io.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/utf8.h"
#include "mmay/base/vector.h"

MarimayProcessStatus mmay_process_executable_path(
    MarimayAllocator allocator,
    size_t max_path,
    MarimayOwnedBytes *path
) {
    wchar_t wide_path[32768];
    DWORD wide_length = 0;
    int byte_count = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    if (path == NULL || max_path == 0) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    memset(path, 0, sizeof(*path));
    wide_length = GetModuleFileNameW(
        NULL, wide_path, (DWORD)(sizeof(wide_path) / sizeof(wide_path[0]))
    );
    if (wide_length == 0) return MMAY_PROCESS_IO_FAILED;
    if ((size_t)wide_length
        >= sizeof(wide_path) / sizeof(wide_path[0])) {
        return MMAY_PROCESS_OUTPUT_LIMIT;
    }
    byte_count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, (int)wide_length,
        NULL, 0, NULL, NULL
    );
    if (byte_count <= 0) return MMAY_PROCESS_IO_FAILED;
    if ((size_t)byte_count > max_path
        || !mmay_size_add((size_t)byte_count, 1, &allocation_size)) {
        return MMAY_PROCESS_OUTPUT_LIMIT;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) return MMAY_PROCESS_ALLOCATION_FAILED;
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, (int)wide_length,
            (char *)bytes, byte_count, NULL, NULL
        ) != byte_count) {
        mmay_deallocate(&allocator, bytes, allocation_size);
        return MMAY_PROCESS_IO_FAILED;
    }
    bytes[byte_count] = 0;
    path->allocator = allocator;
    path->data = bytes;
    path->length = (size_t)byte_count;
    return MMAY_PROCESS_OK;
}

static void mmay_process_result_init(MarimayProcessResult *result, MarimayAllocator allocator) {
    result->output.allocator = allocator;
    result->output.data = NULL;
    result->output.length = 0;
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}

static bool mmay_windows_append_repeat(MarimayVector *command, char byte, size_t count) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        if (!mmay_vector_push(command, &byte)) {
            return false;
        }
    }
    return true;
}

static bool mmay_windows_append_argument(MarimayVector *command, const char *argument) {
    size_t length = strlen(argument);
    size_t index = 0;
    char quote = '"';
    if (!mmay_vector_push(command, &quote)) {
        return false;
    }
    while (index < length) {
        size_t backslashes = 0;
        while (index < length && argument[index] == '\\') {
            backslashes += 1;
            index += 1;
        }
        if (index == length) {
            size_t escaped = 0;
            if (!mmay_size_multiply(backslashes, 2, &escaped)) {
                return false;
            }
            return mmay_windows_append_repeat(command, '\\', escaped)
                && mmay_vector_push(command, &quote);
        }
        if (argument[index] == '"') {
            size_t escaped = 0;
            if (!mmay_size_multiply(backslashes, 2, &escaped)
                || !mmay_size_add(escaped, 1, &escaped)
                || !mmay_windows_append_repeat(command, '\\', escaped)
                || !mmay_vector_push(command, &quote)) {
                return false;
            }
        } else {
            if (!mmay_windows_append_repeat(command, '\\', backslashes)
                || !mmay_vector_push(command, &argument[index])) {
                return false;
            }
        }
        index += 1;
    }
    return mmay_vector_push(command, &quote);
}

static bool mmay_windows_command_line(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    wchar_t **wide_command,
    size_t *wide_allocation_size
) {
    MarimayVector command;
    size_t index = 0;
    char zero = '\0';
    int wide_count = 0;
    size_t allocation_size = 0;
    wchar_t *wide = NULL;
    if (!mmay_vector_init(&command, allocator, sizeof(char))) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (index != 0) {
            char space = ' ';
            if (!mmay_vector_push(&command, &space)) {
                mmay_vector_dispose(&command);
                return false;
            }
        }
        if (!mmay_windows_append_argument(&command, arguments[index])) {
            mmay_vector_dispose(&command);
            return false;
        }
    }
    if (!mmay_vector_push(&command, &zero) || command.length > (size_t)INT_MAX) {
        mmay_vector_dispose(&command);
        return false;
    }
    wide_count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        command.data,
        -1,
        NULL,
        0
    );
    if (wide_count <= 0 || wide_count > 32767
        || !mmay_size_multiply((size_t)wide_count, sizeof(wchar_t), &allocation_size)) {
        mmay_vector_dispose(&command);
        return false;
    }
    wide = mmay_allocate(&allocator, allocation_size);
    if (wide == NULL) {
        mmay_vector_dispose(&command);
        return false;
    }
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            command.data,
            -1,
            wide,
            wide_count
        ) != wide_count) {
        mmay_deallocate(&allocator, wide, allocation_size);
        mmay_vector_dispose(&command);
        return false;
    }
    mmay_vector_dispose(&command);
    *wide_command = wide;
    *wide_allocation_size = allocation_size;
    return true;
}

MarimayProcessStatus mmay_process_run(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    MarimayProcessResult *result
) {
    SECURITY_ATTRIBUTES security;
    HANDLE read_handle = INVALID_HANDLE_VALUE;
    HANDLE write_handle = INVALID_HANDLE_VALUE;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process = {0};
    wchar_t *command = NULL;
    size_t command_allocation_size = 0;
    MarimayVector output;
    unsigned char chunk[4096];
    MarimayProcessStatus status = MMAY_PROCESS_OK;
    bool process_started = false;
    size_t index = 0;

    if (result == NULL) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    mmay_process_result_init(result, allocator);
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL
            || !mmay_utf8_validate(arguments[index], strlen(arguments[index]), NULL)) {
            return MMAY_PROCESS_INVALID_ARGUMENT;
        }
    }
    if (!mmay_vector_init(&output, allocator, sizeof(unsigned char))) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    if (!mmay_windows_command_line(
            allocator,
            arguments,
            argument_count,
            &command,
            &command_allocation_size
        )) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }
    ZeroMemory(&security, sizeof(security));
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    if (!CreatePipe(&read_handle, &write_handle, &security, 0)
        || !SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0)) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = (DWORD)sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = write_handle;
    startup.hStdError = write_handle;
    if (!CreateProcessW(
            NULL,
            command,
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup,
            &process
        )) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    process_started = true;
    CloseHandle(write_handle);
    write_handle = INVALID_HANDLE_VALUE;
    for (;;) {
        DWORD count = 0;
        BOOL read_ok = ReadFile(read_handle, chunk, (DWORD)sizeof(chunk), &count, NULL);
        if (!read_ok) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                break;
            }
            status = MMAY_PROCESS_IO_FAILED;
            goto terminate;
        }
        if (count == 0) {
            break;
        }
        if (output.length > max_output || (size_t)count > max_output - output.length) {
            status = MMAY_PROCESS_OUTPUT_LIMIT;
            goto terminate;
        }
        if (!mmay_vector_append(&output, chunk, (size_t)count)) {
            status = MMAY_PROCESS_ALLOCATION_FAILED;
            goto terminate;
        }
    }
    if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0) {
        status = MMAY_PROCESS_WAIT_FAILED;
        goto terminate;
    }
    {
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
            status = MMAY_PROCESS_WAIT_FAILED;
            goto cleanup;
        }
        result->exit_code = (uint32_t)exit_code;
    }
    if (!mmay_owned_bytes_copy(&result->output, allocator, output.data, output.length)) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
    }
    goto cleanup;

terminate:
    (void)TerminateProcess(process.hProcess, 70U);
    (void)WaitForSingleObject(process.hProcess, INFINITE);

cleanup:
    if (process_started) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    if (read_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(read_handle);
    }
    if (write_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(write_handle);
    }
    if (command != NULL) {
        mmay_deallocate(&allocator, command, command_allocation_size);
    }
    mmay_vector_dispose(&output);
    if (status != MMAY_PROCESS_OK) {
        mmay_owned_bytes_dispose(&result->output);
    }
    return status;
}

static bool mmay_windows_inheritable_file(FILE *file, HANDLE *result) {
    intptr_t raw;
    if (file == NULL || result == NULL) return false;
    raw = _get_osfhandle(_fileno(file));
    if (raw == -1) return false;
    return DuplicateHandle(
        GetCurrentProcess(), (HANDLE)raw, GetCurrentProcess(), result,
        0, TRUE, DUPLICATE_SAME_ACCESS
    ) != 0;
}

MarimayProcessStatus mmay_process_run_stdio(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    FILE *standard_input,
    FILE *standard_output,
    FILE *standard_error,
    MarimayProcessResult *result
) {
    STARTUPINFOW startup;
    PROCESS_INFORMATION process = {0};
    HANDLE input_handle = INVALID_HANDLE_VALUE;
    HANDLE output_handle = INVALID_HANDLE_VALUE;
    HANDLE error_handle = INVALID_HANDLE_VALUE;
    wchar_t *command = NULL;
    size_t command_size = 0;
    size_t index = 0;
    bool process_started = false;
    MarimayProcessStatus status = MMAY_PROCESS_OK;
    if (result == NULL) return MMAY_PROCESS_INVALID_ARGUMENT;
    mmay_process_result_init(result, allocator);
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL || standard_input == NULL
        || standard_output == NULL || standard_error == NULL) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL || !mmay_utf8_validate(
                arguments[index], strlen(arguments[index]), NULL
            )) return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    if (!mmay_windows_command_line(
            allocator, arguments, argument_count, &command, &command_size
        )) return MMAY_PROCESS_ALLOCATION_FAILED;
    if (fflush(standard_output) != 0 || fflush(standard_error) != 0
        || !mmay_windows_inheritable_file(standard_input, &input_handle)
        || !mmay_windows_inheritable_file(standard_output, &output_handle)
        || !mmay_windows_inheritable_file(standard_error, &error_handle)) {
        status = MMAY_PROCESS_IO_FAILED;
        goto cleanup;
    }
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = (DWORD)sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input_handle;
    startup.hStdOutput = output_handle;
    startup.hStdError = error_handle;
    if (!CreateProcessW(
            NULL, command, NULL, NULL, TRUE, 0,
            NULL, NULL, &startup, &process
        )) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    process_started = true;
    if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0) {
        status = MMAY_PROCESS_WAIT_FAILED;
        goto terminate;
    }
    {
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
            status = MMAY_PROCESS_WAIT_FAILED;
            goto terminate;
        }
        result->exit_code = (uint32_t)exit_code;
    }
    goto cleanup;
terminate:
    (void)TerminateProcess(process.hProcess, 70U);
    (void)WaitForSingleObject(process.hProcess, INFINITE);
cleanup:
    if (process_started) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    if (input_handle != INVALID_HANDLE_VALUE) CloseHandle(input_handle);
    if (output_handle != INVALID_HANDLE_VALUE) CloseHandle(output_handle);
    if (error_handle != INVALID_HANDLE_VALUE) CloseHandle(error_handle);
    if (command != NULL) mmay_deallocate(&allocator, command, command_size);
    return status;
}

void mmay_process_result_dispose(MarimayProcessResult *result) {
    if (result != NULL) {
        mmay_owned_bytes_dispose(&result->output);
        result->exit_code = 0;
        result->signaled = false;
        result->signal_code = 0;
    }
}

static MarimayProcessStatus mmay_windows_capture_available(
    HANDLE handle,
    MarimayVector *bytes,
    size_t limit,
    bool *progress
) {
    unsigned char chunk[4096];
    DWORD available = 0;
    DWORD count = 0;
    DWORD requested = 0;
    if (!PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
        return GetLastError() == ERROR_BROKEN_PIPE
            ? MMAY_PROCESS_OK : MMAY_PROCESS_IO_FAILED;
    }
    if (available == 0) return MMAY_PROCESS_OK;
    requested = available < (DWORD)sizeof(chunk)
        ? available : (DWORD)sizeof(chunk);
    if (!ReadFile(handle, chunk, requested, &count, NULL)) {
        return MMAY_PROCESS_IO_FAILED;
    }
    if (bytes->length > limit || (size_t)count > limit - bytes->length) {
        return MMAY_PROCESS_OUTPUT_LIMIT;
    }
    if (!mmay_vector_append(bytes, chunk, (size_t)count)) {
        return MMAY_PROCESS_ALLOCATION_FAILED;
    }
    *progress = count != 0;
    return MMAY_PROCESS_OK;
}

static MarimayProcessStatus mmay_windows_stdin_file(
    const unsigned char *bytes,
    size_t length,
    HANDLE *handle
) {
    SECURITY_ATTRIBUTES security;
    wchar_t directory[MAX_PATH + 1];
    wchar_t path[MAX_PATH + 1];
    HANDLE file = INVALID_HANDLE_VALUE;
    size_t offset = 0;
    LARGE_INTEGER beginning;
    DWORD directory_length = 0;
    if (handle == NULL || (bytes == NULL && length != 0)) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    directory_length = GetTempPathW(
        (DWORD)(sizeof(directory) / sizeof(directory[0])), directory
    );
    if (directory_length == 0
        || directory_length >= sizeof(directory) / sizeof(directory[0])
        || GetTempFileNameW(directory, L"mmy", 0, path) == 0) {
        return MMAY_PROCESS_IO_FAILED;
    }
    ZeroMemory(&security, sizeof(security));
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    file = CreateFileW(
        path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &security, TRUNCATE_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        (void)DeleteFileW(path);
        return MMAY_PROCESS_IO_FAILED;
    }
    while (offset < length) {
        DWORD amount = length - offset > (size_t)UINT32_MAX
            ? (DWORD)UINT32_MAX : (DWORD)(length - offset);
        DWORD count = 0;
        if (!WriteFile(file, bytes + offset, amount, &count, NULL)
            || count == 0) {
            CloseHandle(file);
            return MMAY_PROCESS_IO_FAILED;
        }
        offset += (size_t)count;
    }
    beginning.QuadPart = 0;
    if (!SetFilePointerEx(file, beginning, NULL, FILE_BEGIN)) {
        CloseHandle(file);
        return MMAY_PROCESS_IO_FAILED;
    }
    *handle = file;
    return MMAY_PROCESS_OK;
}

MarimayProcessStatus mmay_process_run_capture_input(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    const unsigned char *standard_input,
    size_t standard_input_length,
    size_t max_output,
    size_t max_error,
    MarimayProcessCaptureResult *result
) {
    SECURITY_ATTRIBUTES security;
    HANDLE output_read = INVALID_HANDLE_VALUE;
    HANDLE output_write = INVALID_HANDLE_VALUE;
    HANDLE error_read = INVALID_HANDLE_VALUE;
    HANDLE error_write = INVALID_HANDLE_VALUE;
    HANDLE input_file = INVALID_HANDLE_VALUE;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process = {0};
    wchar_t *command = NULL;
    size_t command_size = 0;
    MarimayVector output;
    MarimayVector error;
    MarimayProcessStatus status = MMAY_PROCESS_OK;
    bool process_started = false;
    size_t index = 0;
    memset(&output, 0, sizeof(output));
    memset(&error, 0, sizeof(error));
    if (result == NULL) return MMAY_PROCESS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->standard_output.allocator = allocator;
    result->standard_error.allocator = allocator;
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL
        || (standard_input == NULL && standard_input_length != 0)) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL || !mmay_utf8_validate(
                arguments[index], strlen(arguments[index]), NULL
            )) return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    if (!mmay_vector_init(&output, allocator, sizeof(unsigned char))
        || !mmay_vector_init(&error, allocator, sizeof(unsigned char))) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!mmay_windows_command_line(
            allocator, arguments, argument_count,
            &command, &command_size
        )) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }
    status = mmay_windows_stdin_file(
        standard_input, standard_input_length, &input_file
    );
    if (status != MMAY_PROCESS_OK) goto cleanup;
    ZeroMemory(&security, sizeof(security));
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    if (!CreatePipe(&output_read, &output_write, &security, 0)
        || !SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0)
        || !CreatePipe(&error_read, &error_write, &security, 0)
        || !SetHandleInformation(error_read, HANDLE_FLAG_INHERIT, 0)) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = (DWORD)sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input_file;
    startup.hStdOutput = output_write;
    startup.hStdError = error_write;
    if (!CreateProcessW(
            NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
            NULL, NULL, &startup, &process
        )) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    process_started = true;
    CloseHandle(input_file);
    input_file = INVALID_HANDLE_VALUE;
    CloseHandle(output_write);
    output_write = INVALID_HANDLE_VALUE;
    CloseHandle(error_write);
    error_write = INVALID_HANDLE_VALUE;
    for (;;) {
        bool progress = false;
        DWORD wait_status;
        status = mmay_windows_capture_available(
            output_read, &output, max_output, &progress
        );
        if (status != MMAY_PROCESS_OK) goto terminate;
        status = mmay_windows_capture_available(
            error_read, &error, max_error, &progress
        );
        if (status != MMAY_PROCESS_OK) goto terminate;
        wait_status = WaitForSingleObject(process.hProcess, 0);
        if (wait_status == WAIT_FAILED) {
            status = MMAY_PROCESS_WAIT_FAILED;
            goto terminate;
        }
        if (wait_status == WAIT_OBJECT_0 && !progress) {
            DWORD output_available = 0;
            DWORD error_available = 0;
            if ((!PeekNamedPipe(
                    output_read, NULL, 0, NULL, &output_available, NULL
                ) && GetLastError() != ERROR_BROKEN_PIPE)
                || (!PeekNamedPipe(
                    error_read, NULL, 0, NULL, &error_available, NULL
                ) && GetLastError() != ERROR_BROKEN_PIPE)) {
                status = MMAY_PROCESS_IO_FAILED;
                goto terminate;
            }
            if (output_available == 0 && error_available == 0) break;
        }
        if (!progress) Sleep(1);
    }
    {
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
            status = MMAY_PROCESS_WAIT_FAILED;
            goto cleanup;
        }
        result->exit_code = (uint32_t)exit_code;
    }
    if (!mmay_owned_bytes_copy(
            &result->standard_output, allocator,
            output.data, output.length
        ) || !mmay_owned_bytes_copy(
            &result->standard_error, allocator,
            error.data, error.length
        )) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
    }
    goto cleanup;
terminate:
    (void)TerminateProcess(process.hProcess, 70U);
    (void)WaitForSingleObject(process.hProcess, INFINITE);
cleanup:
    if (process_started) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    if (output_read != INVALID_HANDLE_VALUE) CloseHandle(output_read);
    if (output_write != INVALID_HANDLE_VALUE) CloseHandle(output_write);
    if (error_read != INVALID_HANDLE_VALUE) CloseHandle(error_read);
    if (error_write != INVALID_HANDLE_VALUE) CloseHandle(error_write);
    if (input_file != INVALID_HANDLE_VALUE) CloseHandle(input_file);
    if (command != NULL) mmay_deallocate(&allocator, command, command_size);
    mmay_vector_dispose(&output);
    mmay_vector_dispose(&error);
    if (status != MMAY_PROCESS_OK) {
        mmay_process_capture_result_dispose(result);
    }
    return status;
}

MarimayProcessStatus mmay_process_run_capture(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    size_t max_error,
    MarimayProcessCaptureResult *result
) {
    return mmay_process_run_capture_input(
        allocator, arguments, argument_count, NULL, 0,
        max_output, max_error, result
    );
}

void mmay_process_capture_result_dispose(MarimayProcessCaptureResult *result) {
    if (result == NULL) return;
    mmay_owned_bytes_dispose(&result->standard_output);
    mmay_owned_bytes_dispose(&result->standard_error);
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}
