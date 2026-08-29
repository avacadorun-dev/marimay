#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mmay/platform/file.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/utf8.h"

static MarimayFileStatus mmay_windows_owned_path(
    MarimayAllocator allocator,
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
        return MMAY_FILE_INVALID_ARGUMENT;
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
        || !mmay_size_add((size_t)wide_count, 1, &count_with_zero)
        || !mmay_size_multiply(count_with_zero, sizeof(wchar_t), &byte_count)) {
        return GetLastError() == ERROR_NO_UNICODE_TRANSLATION
            ? MMAY_FILE_INVALID_UTF8
            : MMAY_FILE_INVALID_ARGUMENT;
    }
    wide = mmay_allocate(&allocator, byte_count);
    if (wide == NULL) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            (int)path_length,
            wide,
            wide_count
        ) != wide_count) {
        mmay_deallocate(&allocator, wide, byte_count);
        return MMAY_FILE_INVALID_UTF8;
    }
    wide[wide_count] = L'\0';
    *wide_path = wide;
    *allocation_size = byte_count;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_windows_create_exclusive(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    MarimayFileStatus status = MMAY_FILE_OK;
    (void)context;
    status = mmay_windows_owned_path(
        allocator,
        path,
        path_length,
        &wide_path,
        &allocation_size
    );
    if (status != MMAY_FILE_OK) {
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
            ? MMAY_FILE_ALREADY_EXISTS
            : MMAY_FILE_IO_FAILED;
    } else {
        *handle = (MarimayFileHandle)(uintptr_t)file;
    }
    mmay_deallocate(&allocator, wide_path, allocation_size);
    return status;
}

static MarimayFileStatus mmay_windows_open_read(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    MarimayFileStatus status = MMAY_FILE_OK;
    (void)context;
    status = mmay_windows_owned_path(
        allocator,
        path,
        path_length,
        &wide_path,
        &allocation_size
    );
    if (status != MMAY_FILE_OK) {
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
        status = MMAY_FILE_IO_FAILED;
    } else {
        *handle = (MarimayFileHandle)(uintptr_t)file;
    }
    mmay_deallocate(&allocator, wide_path, allocation_size);
    return status;
}

static MarimayFileStatus mmay_windows_read(
    void *context,
    MarimayFileHandle handle,
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
        return MMAY_FILE_IO_FAILED;
    }
    *read_count = (size_t)count;
    *end = count == 0;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_windows_write(
    void *context,
    MarimayFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
) {
    DWORD request = length > (size_t)UINT32_MAX ? UINT32_MAX : (DWORD)length;
    DWORD count = 0;
    (void)context;
    if (!WriteFile((HANDLE)(uintptr_t)handle, bytes, request, &count, NULL)) {
        return MMAY_FILE_IO_FAILED;
    }
    *written = (size_t)count;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_windows_sync(void *context, MarimayFileHandle handle) {
    (void)context;
    return FlushFileBuffers((HANDLE)(uintptr_t)handle)
        ? MMAY_FILE_OK
        : MMAY_FILE_IO_FAILED;
}

static MarimayFileStatus mmay_windows_close(void *context, MarimayFileHandle handle) {
    (void)context;
    return CloseHandle((HANDLE)(uintptr_t)handle)
        ? MMAY_FILE_OK
        : MMAY_FILE_IO_FAILED;
}

static MarimayFileStatus mmay_windows_replace(
    void *context,
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    wchar_t *temporary = NULL;
    wchar_t *destination = NULL;
    size_t temporary_size = 0;
    size_t destination_size = 0;
    MarimayFileStatus status = MMAY_FILE_OK;
    (void)context;
    status = mmay_windows_owned_path(
        allocator,
        temporary_path,
        temporary_path_length,
        &temporary,
        &temporary_size
    );
    if (status != MMAY_FILE_OK) {
        return status;
    }
    status = mmay_windows_owned_path(
        allocator,
        destination_path,
        destination_path_length,
        &destination,
        &destination_size
    );
    if (status == MMAY_FILE_OK
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
            status = MMAY_FILE_IO_FAILED;
        }
    }
    if (destination != NULL) {
        mmay_deallocate(&allocator, destination, destination_size);
    }
    mmay_deallocate(&allocator, temporary, temporary_size);
    return status;
}

static void mmay_windows_remove(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    (void)context;
    if (mmay_windows_owned_path(
            allocator,
            path,
            path_length,
            &wide_path,
            &allocation_size
        ) == MMAY_FILE_OK) {
        (void)DeleteFileW(wide_path);
        mmay_deallocate(&allocator, wide_path, allocation_size);
    }
}

MarimayFileOps mmay_platform_file_ops(void) {
    MarimayFileOps ops;
    ops.context = NULL;
    ops.create_exclusive = mmay_windows_create_exclusive;
    ops.write = mmay_windows_write;
    ops.sync = mmay_windows_sync;
    ops.close = mmay_windows_close;
    ops.replace = mmay_windows_replace;
    ops.remove = mmay_windows_remove;
    return ops;
}

MarimayFileReadOps mmay_platform_file_read_ops(void) {
    MarimayFileReadOps ops;
    ops.context = NULL;
    ops.open = mmay_windows_open_read;
    ops.read = mmay_windows_read;
    ops.close = mmay_windows_close;
    return ops;
}

MarimayFileStatus mmay_file_metadata(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileMetadata *metadata
) {
    wchar_t *wide_path = NULL;
    size_t allocation_size = 0;
    WIN32_FILE_ATTRIBUTE_DATA information;
    MarimayFileStatus status;
    if (metadata == NULL || path == NULL || path_length == 0
        || memchr(path, 0, path_length) != NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    memset(metadata, 0, sizeof(*metadata));
    if (!mmay_utf8_validate(path, path_length, NULL)) {
        return MMAY_FILE_INVALID_UTF8;
    }
    status = mmay_windows_owned_path(
        allocator, path, path_length, &wide_path, &allocation_size
    );
    if (status != MMAY_FILE_OK) return status;
    if (!GetFileAttributesExW(
            wide_path, GetFileExInfoStandard, &information
        )) {
        DWORD error = GetLastError();
        status = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            ? MMAY_FILE_LIMIT_EXCEEDED : MMAY_FILE_IO_FAILED;
    } else {
        metadata->symlink =
            (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        metadata->directory =
            (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        metadata->regular = !metadata->symlink && !metadata->directory
            && (information.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) == 0;
        if (metadata->regular) {
            metadata->size = ((uint64_t)information.nFileSizeHigh << 32)
                | (uint64_t)information.nFileSizeLow;
        }
    }
    mmay_deallocate(&allocator, wide_path, allocation_size);
    return status;
}

MarimayFileStatus mmay_file_replace_executable(
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    wchar_t *temporary = NULL;
    wchar_t *destination = NULL;
    size_t temporary_bytes = 0;
    size_t destination_bytes = 0;
    MarimayFileStatus status;
    if (temporary_path == NULL || temporary_path_length == 0
        || destination_path == NULL || destination_path_length == 0
        || memchr(temporary_path, 0, temporary_path_length) != NULL
        || memchr(destination_path, 0, destination_path_length) != NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    if (!mmay_utf8_validate(temporary_path, temporary_path_length, NULL)
        || !mmay_utf8_validate(
            destination_path, destination_path_length, NULL
        )) return MMAY_FILE_INVALID_UTF8;
    status = mmay_windows_owned_path(
        allocator, temporary_path, temporary_path_length,
        &temporary, &temporary_bytes
    );
    if (status == MMAY_FILE_OK) status = mmay_windows_owned_path(
        allocator, destination_path, destination_path_length,
        &destination, &destination_bytes
    );
    if (status == MMAY_FILE_OK && !MoveFileExW(
            temporary, destination,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) status = MMAY_FILE_IO_FAILED;
    if (destination != NULL) {
        mmay_deallocate(&allocator, destination, destination_bytes);
    }
    if (temporary != NULL) {
        mmay_deallocate(&allocator, temporary, temporary_bytes);
    }
    return status;
}
