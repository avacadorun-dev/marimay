#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ruda/platform/process.h"

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/utf8.h"
#include "ruda/base/vector.h"

static void ruda_process_result_init(RudaProcessResult *result, RudaAllocator allocator) {
    result->output.allocator = allocator;
    result->output.data = NULL;
    result->output.length = 0;
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}

static bool ruda_windows_append_repeat(RudaVector *command, char byte, size_t count) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        if (!ruda_vector_push(command, &byte)) {
            return false;
        }
    }
    return true;
}

static bool ruda_windows_append_argument(RudaVector *command, const char *argument) {
    size_t length = strlen(argument);
    size_t index = 0;
    char quote = '"';
    if (!ruda_vector_push(command, &quote)) {
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
            if (!ruda_size_multiply(backslashes, 2, &escaped)) {
                return false;
            }
            return ruda_windows_append_repeat(command, '\\', escaped)
                && ruda_vector_push(command, &quote);
        }
        if (argument[index] == '"') {
            size_t escaped = 0;
            if (!ruda_size_multiply(backslashes, 2, &escaped)
                || !ruda_size_add(escaped, 1, &escaped)
                || !ruda_windows_append_repeat(command, '\\', escaped)
                || !ruda_vector_push(command, &quote)) {
                return false;
            }
        } else {
            if (!ruda_windows_append_repeat(command, '\\', backslashes)
                || !ruda_vector_push(command, &argument[index])) {
                return false;
            }
        }
        index += 1;
    }
    return ruda_vector_push(command, &quote);
}

static bool ruda_windows_command_line(
    RudaAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    wchar_t **wide_command,
    size_t *wide_allocation_size
) {
    RudaVector command;
    size_t index = 0;
    char zero = '\0';
    int wide_count = 0;
    size_t allocation_size = 0;
    wchar_t *wide = NULL;
    if (!ruda_vector_init(&command, allocator, sizeof(char))) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (index != 0) {
            char space = ' ';
            if (!ruda_vector_push(&command, &space)) {
                ruda_vector_dispose(&command);
                return false;
            }
        }
        if (!ruda_windows_append_argument(&command, arguments[index])) {
            ruda_vector_dispose(&command);
            return false;
        }
    }
    if (!ruda_vector_push(&command, &zero) || command.length > (size_t)INT_MAX) {
        ruda_vector_dispose(&command);
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
        || !ruda_size_multiply((size_t)wide_count, sizeof(wchar_t), &allocation_size)) {
        ruda_vector_dispose(&command);
        return false;
    }
    wide = ruda_allocate(&allocator, allocation_size);
    if (wide == NULL) {
        ruda_vector_dispose(&command);
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
        ruda_deallocate(&allocator, wide, allocation_size);
        ruda_vector_dispose(&command);
        return false;
    }
    ruda_vector_dispose(&command);
    *wide_command = wide;
    *wide_allocation_size = allocation_size;
    return true;
}

RudaProcessStatus ruda_process_run(
    RudaAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    RudaProcessResult *result
) {
    SECURITY_ATTRIBUTES security;
    HANDLE read_handle = INVALID_HANDLE_VALUE;
    HANDLE write_handle = INVALID_HANDLE_VALUE;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    wchar_t *command = NULL;
    size_t command_allocation_size = 0;
    RudaVector output;
    unsigned char chunk[4096];
    RudaProcessStatus status = RUDA_PROCESS_OK;
    bool process_started = false;
    size_t index = 0;

    if (result == NULL) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    ruda_process_result_init(result, allocator);
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL
            || !ruda_utf8_validate(arguments[index], strlen(arguments[index]), NULL)) {
            return RUDA_PROCESS_INVALID_ARGUMENT;
        }
    }
    if (!ruda_vector_init(&output, allocator, sizeof(unsigned char))) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    if (!ruda_windows_command_line(
            allocator,
            arguments,
            argument_count,
            &command,
            &command_allocation_size
        )) {
        status = RUDA_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }
    ZeroMemory(&security, sizeof(security));
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    if (!CreatePipe(&read_handle, &write_handle, &security, 0)
        || !SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0)) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = (DWORD)sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = write_handle;
    startup.hStdError = write_handle;
    ZeroMemory(&process, sizeof(process));
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
        status = RUDA_PROCESS_SPAWN_FAILED;
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
            status = RUDA_PROCESS_IO_FAILED;
            goto terminate;
        }
        if (count == 0) {
            break;
        }
        if (output.length > max_output || (size_t)count > max_output - output.length) {
            status = RUDA_PROCESS_OUTPUT_LIMIT;
            goto terminate;
        }
        if (!ruda_vector_append(&output, chunk, (size_t)count)) {
            status = RUDA_PROCESS_ALLOCATION_FAILED;
            goto terminate;
        }
    }
    if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0) {
        status = RUDA_PROCESS_WAIT_FAILED;
        goto terminate;
    }
    {
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
            status = RUDA_PROCESS_WAIT_FAILED;
            goto cleanup;
        }
        result->exit_code = (uint32_t)exit_code;
    }
    if (!ruda_owned_bytes_copy(&result->output, allocator, output.data, output.length)) {
        status = RUDA_PROCESS_ALLOCATION_FAILED;
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
        ruda_deallocate(&allocator, command, command_allocation_size);
    }
    ruda_vector_dispose(&output);
    if (status != RUDA_PROCESS_OK) {
        ruda_owned_bytes_dispose(&result->output);
    }
    return status;
}

void ruda_process_result_dispose(RudaProcessResult *result) {
    if (result != NULL) {
        ruda_owned_bytes_dispose(&result->output);
        result->exit_code = 0;
        result->signaled = false;
        result->signal_code = 0;
    }
}
