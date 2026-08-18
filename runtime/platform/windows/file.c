#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ruda/platform/file.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/utf8.h"

static RudaFileStatus ruda_windows_owned_path(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    wchar_t **wide_path,
    size_t *allocation_size
) {
    int wide_count = 0;
    size_t count_with_zero = 0;
    size_t byte_count = 0;
    wchar_t *wide = NULL;
    if (path_length > (size_t)INT_MAX) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    wide_count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        path,
        (int)path_length,
        NULL,
        0
    );
    if (wide_count <= 0
        || !ruda_size_add((size_t)wide_count, 1, &count_with_zero)
        || !ruda_size_multiply(count_with_zero, sizeof(wchar_t), &byte_count)) {
        return GetLastError() == ERROR_NO_UNICODE_TRANSLATION
            ? RUDA_FILE_INVALID_UTF8
            : RUDA_FILE_INVALID_ARGUMENT;
    }
    wide = ruda_allocate(&allocator, byte_count);
    if (wide == NULL) {
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            (int)path_length,
            wide,
            wide_count
        ) != wide_count) {
        ruda_deallocate(&allocator, wide, byte_count);
        return RUDA_FILE_INVALID_UTF8;
    }
    wide[wide_count] = L'\0';
    *wide_path = wide;
    *allocation_size = byte_count;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_windows_create_exclusive(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    RudaFileStatus status = RUDA_FILE_OK;
    (void)context;
    status = ruda_windows_owned_path(
        allocator,
        path,
        path_length,
        &wide_path,
        &allocation_size
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    file = CreateFileW(
        wide_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        status = error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS
            ? RUDA_FILE_ALREADY_EXISTS
            : RUDA_FILE_IO_FAILED;
    } else {
        *handle = (RudaFileHandle)(uintptr_t)file;
    }
    ruda_deallocate(&allocator, wide_path, allocation_size);
    return status;
}

static RudaFileStatus ruda_windows_open_read(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    RudaFileStatus status = RUDA_FILE_OK;
    (void)context;
    status = ruda_windows_owned_path(
        allocator,
        path,
        path_length,
        &wide_path,
        &allocation_size
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    file = CreateFileW(
        wide_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        status = RUDA_FILE_IO_FAILED;
    } else {
        *handle = (RudaFileHandle)(uintptr_t)file;
    }
    ruda_deallocate(&allocator, wide_path, allocation_size);
    return status;
}

static RudaFileStatus ruda_windows_read(
    void *context,
    RudaFileHandle handle,
    unsigned char *bytes,
    size_t capacity,
    size_t *read_count,
    bool *end
) {
    DWORD request = capacity > (size_t)UINT32_MAX
        ? UINT32_MAX
        : (DWORD)capacity;
    DWORD count = 0;
    (void)context;
    if (!ReadFile((HANDLE)(uintptr_t)handle, bytes, request, &count, NULL)) {
        return RUDA_FILE_IO_FAILED;
    }
    *read_count = (size_t)count;
    *end = count == 0;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_windows_write(
    void *context,
    RudaFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
) {
    DWORD request = length > (size_t)UINT32_MAX ? UINT32_MAX : (DWORD)length;
    DWORD count = 0;
    (void)context;
    if (!WriteFile((HANDLE)(uintptr_t)handle, bytes, request, &count, NULL)) {
        return RUDA_FILE_IO_FAILED;
    }
    *written = (size_t)count;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_windows_sync(void *context, RudaFileHandle handle) {
    (void)context;
    return FlushFileBuffers((HANDLE)(uintptr_t)handle)
        ? RUDA_FILE_OK
        : RUDA_FILE_IO_FAILED;
}

static RudaFileStatus ruda_windows_close(void *context, RudaFileHandle handle) {
    (void)context;
    return CloseHandle((HANDLE)(uintptr_t)handle)
        ? RUDA_FILE_OK
        : RUDA_FILE_IO_FAILED;
}

static RudaFileStatus ruda_windows_replace(
    void *context,
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    wchar_t *temporary = NULL;
    wchar_t *destination = NULL;
    size_t temporary_size = 0;
    size_t destination_size = 0;
    RudaFileStatus status = RUDA_FILE_OK;
    (void)context;
    status = ruda_windows_owned_path(
        allocator,
        temporary_path,
        temporary_path_length,
        &temporary,
        &temporary_size
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    status = ruda_windows_owned_path(
        allocator,
        destination_path,
        destination_path_length,
        &destination,
        &destination_size
    );
    if (status == RUDA_FILE_OK
        && !ReplaceFileW(
            destination,
            temporary,
            NULL,
            REPLACEFILE_WRITE_THROUGH,
            NULL,
            NULL
        )) {
        DWORD error = GetLastError();
        if ((error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
            || !MoveFileExW(
                temporary,
                destination,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
            )) {
            status = RUDA_FILE_IO_FAILED;
        }
    }
    if (destination != NULL) {
        ruda_deallocate(&allocator, destination, destination_size);
    }
    ruda_deallocate(&allocator, temporary, temporary_size);
    return status;
}

static void ruda_windows_remove(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    (void)context;
    if (ruda_windows_owned_path(
            allocator,
            path,
            path_length,
            &wide_path,
            &allocation_size
        ) == RUDA_FILE_OK) {
        (void)DeleteFileW(wide_path);
        ruda_deallocate(&allocator, wide_path, allocation_size);
    }
}

RudaFileOps ruda_platform_file_ops(void) {
    RudaFileOps ops;
    ops.context = NULL;
    ops.create_exclusive = ruda_windows_create_exclusive;
    ops.write = ruda_windows_write;
    ops.sync = ruda_windows_sync;
    ops.close = ruda_windows_close;
    ops.replace = ruda_windows_replace;
    ops.remove = ruda_windows_remove;
    return ops;
}

RudaFileReadOps ruda_platform_file_read_ops(void) {
    RudaFileReadOps ops;
    ops.context = NULL;
    ops.open = ruda_windows_open_read;
    ops.read = ruda_windows_read;
    ops.close = ruda_windows_close;
    return ops;
}

RudaFileStatus ruda_file_replace_executable(
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    wchar_t *temporary = NULL;
    wchar_t *destination = NULL;
    size_t temporary_bytes = 0;
    size_t destination_bytes = 0;
    RudaFileStatus status;
    if (temporary_path == NULL || temporary_path_length == 0
        || destination_path == NULL || destination_path_length == 0
        || memchr(temporary_path, 0, temporary_path_length) != NULL
        || memchr(destination_path, 0, destination_path_length) != NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    if (!ruda_utf8_validate(temporary_path, temporary_path_length, NULL)
        || !ruda_utf8_validate(
            destination_path, destination_path_length, NULL
        )) return RUDA_FILE_INVALID_UTF8;
    status = ruda_windows_owned_path(
        allocator, temporary_path, temporary_path_length,
        &temporary, &temporary_bytes
    );
    if (status == RUDA_FILE_OK) status = ruda_windows_owned_path(
        allocator, destination_path, destination_path_length,
        &destination, &destination_bytes
    );
    if (status == RUDA_FILE_OK && !MoveFileExW(
            temporary, destination,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) status = RUDA_FILE_IO_FAILED;
    if (destination != NULL) {
        ruda_deallocate(&allocator, destination, destination_bytes);
    }
    if (temporary != NULL) {
        ruda_deallocate(&allocator, temporary, temporary_bytes);
    }
    return status;
}
