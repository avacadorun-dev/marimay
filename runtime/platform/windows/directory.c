#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mmay/platform/directory.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include "mmay/base/utf8.h"

typedef struct MarimayWindowsDirectoryState {
    MarimayAllocator allocator;
    HANDLE search;
    WIN32_FIND_DATAW data;
    bool first;
} MarimayWindowsDirectoryState;

static MarimayFileStatus mmay_windows_directory_wide_path(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    wchar_t **wide_path,
    size_t *allocation_size,
    size_t *wide_length
) {
    int count = 0;
    size_t count_with_zero = 0;
    size_t byte_count = 0;
    wchar_t *wide = NULL;
    if (path_length > (size_t)INT_MAX) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        path,
        (int)path_length,
        NULL,
        0
    );
    if (count <= 0
        || !mmay_size_add((size_t)count, 1, &count_with_zero)
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
            count
        ) != count) {
        mmay_deallocate(&allocator, wide, byte_count);
        return MMAY_FILE_INVALID_UTF8;
    }
    wide[count] = L'\0';
    *wide_path = wide;
    *allocation_size = byte_count;
    if (wide_length != NULL) {
        *wide_length = (size_t)count;
    }
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_windows_directory_utf8(
    MarimayAllocator allocator,
    const wchar_t *wide,
    size_t wide_length,
    size_t max_bytes,
    MarimayOwnedBytes *output
) {
    int byte_count = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    if (wide_length > (size_t)INT_MAX) {
        return MMAY_FILE_LIMIT_EXCEEDED;
    }
    byte_count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide,
        (int)wide_length,
        NULL,
        0,
        NULL,
        NULL
    );
    if (byte_count <= 0) {
        return MMAY_FILE_INVALID_UTF8;
    }
    if ((size_t)byte_count > max_bytes) {
        return MMAY_FILE_LIMIT_EXCEEDED;
    }
    if (!mmay_size_add((size_t)byte_count, 1, &allocation_size)) {
        return MMAY_FILE_LIMIT_EXCEEDED;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wide,
            (int)wide_length,
            (char *)bytes,
            byte_count,
            NULL,
            NULL
        ) != byte_count) {
        mmay_deallocate(&allocator, bytes, allocation_size);
        return MMAY_FILE_INVALID_UTF8;
    }
    bytes[byte_count] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = (size_t)byte_count;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_windows_directory_current(
    void *context,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
) {
    DWORD required = 0;
    DWORD written = 0;
    size_t allocation_size = 0;
    wchar_t *wide = NULL;
    MarimayFileStatus status;
    size_t index = 0;
    (void)context;
    required = GetCurrentDirectoryW(0, NULL);
    if (required == 0
        || !mmay_size_multiply((size_t)required, sizeof(wchar_t), &allocation_size)) {
        return MMAY_FILE_IO_FAILED;
    }
    wide = mmay_allocate(&allocator, allocation_size);
    if (wide == NULL) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    written = GetCurrentDirectoryW(required, wide);
    if (written == 0 || written >= required) {
        mmay_deallocate(&allocator, wide, allocation_size);
        return MMAY_FILE_IO_FAILED;
    }
    status = mmay_windows_directory_utf8(
        allocator,
        wide,
        (size_t)written,
        max_path_bytes,
        path
    );
    mmay_deallocate(&allocator, wide, allocation_size);
    if (status == MMAY_FILE_OK) {
        for (index = 0; index < path->length; index += 1) {
            if (path->data[index] == '\\') {
                path->data[index] = '/';
            }
        }
    }
    return status;
}

static MarimayFileStatus mmay_windows_directory_kind(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
) {
    wchar_t *wide = NULL;
    size_t allocation_size = 0;
    DWORD attributes;
    MarimayFileStatus status;
    (void)context;
    status = mmay_windows_directory_wide_path(
        allocator,
        path,
        path_length,
        &wide,
        &allocation_size,
        NULL
    );
    if (status != MMAY_FILE_OK) {
        return status;
    }
    attributes = GetFileAttributesW(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            || error == ERROR_INVALID_NAME) {
            *kind = MMAY_DIRECTORY_ENTRY_MISSING;
            status = MMAY_FILE_OK;
        } else {
            status = MMAY_FILE_IO_FAILED;
        }
    } else if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        *kind = MMAY_DIRECTORY_ENTRY_SYMLINK;
    } else if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        *kind = MMAY_DIRECTORY_ENTRY_DIRECTORY;
    } else if ((attributes & FILE_ATTRIBUTE_DEVICE) != 0) {
        *kind = MMAY_DIRECTORY_ENTRY_OTHER;
    } else {
        *kind = MMAY_DIRECTORY_ENTRY_REGULAR;
    }
    mmay_deallocate(&allocator, wide, allocation_size);
    return status;
}

static MarimayFileStatus mmay_windows_directory_open(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryHandle *handle
) {
    size_t pattern_length = 0;
    size_t pattern_size = 0;
    char *pattern = NULL;
    wchar_t *wide = NULL;
    size_t wide_size = 0;
    MarimayWindowsDirectoryState *state = NULL;
    MarimayFileStatus status;
    bool separator = path_length != 0
        && path[path_length - 1] != '/'
        && path[path_length - 1] != '\\';
    (void)context;
    if (!mmay_size_add(path_length, separator ? 2 : 1, &pattern_length)
        || !mmay_size_add(pattern_length, 1, &pattern_size)) {
        return MMAY_FILE_LIMIT_EXCEEDED;
    }
    pattern = mmay_allocate(&allocator, pattern_size);
    if (pattern == NULL) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    memcpy(pattern, path, path_length);
    if (separator) {
        pattern[path_length] = '/';
    }
    pattern[path_length + (separator ? 1 : 0)] = '*';
    pattern[pattern_length] = '\0';
    status = mmay_windows_directory_wide_path(
        allocator,
        pattern,
        pattern_length,
        &wide,
        &wide_size,
        NULL
    );
    mmay_deallocate(&allocator, pattern, pattern_size);
    if (status != MMAY_FILE_OK) {
        return status;
    }
    state = mmay_allocate(&allocator, sizeof(*state));
    if (state == NULL) {
        mmay_deallocate(&allocator, wide, wide_size);
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    memset(state, 0, sizeof(*state));
    state->search = FindFirstFileW(wide, &state->data);
    mmay_deallocate(&allocator, wide, wide_size);
    if (state->search == INVALID_HANDLE_VALUE) {
        mmay_deallocate(&allocator, state, sizeof(*state));
        return MMAY_FILE_IO_FAILED;
    }
    state->allocator = allocator;
    state->first = true;
    *handle = (MarimayDirectoryHandle)(uintptr_t)state;
    return MMAY_FILE_OK;
}

static MarimayDirectoryEntryKind mmay_windows_directory_attribute_kind(
    DWORD attributes
) {
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return MMAY_DIRECTORY_ENTRY_SYMLINK;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return MMAY_DIRECTORY_ENTRY_DIRECTORY;
    }
    return (attributes & FILE_ATTRIBUTE_DEVICE) != 0
        ? MMAY_DIRECTORY_ENTRY_OTHER
        : MMAY_DIRECTORY_ENTRY_REGULAR;
}

static MarimayFileStatus mmay_windows_directory_next(
    void *context,
    MarimayDirectoryHandle handle,
    MarimayAllocator allocator,
    size_t max_name_bytes,
    MarimayOwnedBytes *name,
    MarimayDirectoryEntryKind *kind,
    bool *end
) {
    MarimayWindowsDirectoryState *state = (MarimayWindowsDirectoryState *)(uintptr_t)handle;
    (void)context;
    for (;;) {
        const wchar_t *wide_name;
        size_t wide_length;
        MarimayFileStatus status;
        if (state->first) {
            state->first = false;
        } else if (!FindNextFileW(state->search, &state->data)) {
            DWORD error = GetLastError();
            *end = error == ERROR_NO_MORE_FILES;
            return error == ERROR_NO_MORE_FILES
                ? MMAY_FILE_OK
                : MMAY_FILE_IO_FAILED;
        }
        wide_name = state->data.cFileName;
        if (wcscmp(wide_name, L".") == 0 || wcscmp(wide_name, L"..") == 0) {
            continue;
        }
        wide_length = wcslen(wide_name);
        status = mmay_windows_directory_utf8(
            allocator,
            wide_name,
            wide_length,
            max_name_bytes,
            name
        );
        if (status != MMAY_FILE_OK) {
            return status;
        }
        *kind = mmay_windows_directory_attribute_kind(state->data.dwFileAttributes);
        *end = false;
        return MMAY_FILE_OK;
    }
}

static MarimayFileStatus mmay_windows_directory_close(
    void *context,
    MarimayDirectoryHandle handle
) {
    MarimayWindowsDirectoryState *state = (MarimayWindowsDirectoryState *)(uintptr_t)handle;
    MarimayAllocator allocator = state->allocator;
    MarimayFileStatus status = FindClose(state->search)
        ? MMAY_FILE_OK
        : MMAY_FILE_IO_FAILED;
    (void)context;
    mmay_deallocate(&allocator, state, sizeof(*state));
    return status;
}

MarimayDirectoryOps mmay_platform_directory_ops(void) {
    MarimayDirectoryOps ops;
    ops.context = NULL;
    ops.current = mmay_windows_directory_current;
    ops.kind = mmay_windows_directory_kind;
    ops.open = mmay_windows_directory_open;
    ops.next = mmay_windows_directory_next;
    ops.close = mmay_windows_directory_close;
    return ops;
}

MarimayFileStatus mmay_directory_create(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
) {
    wchar_t *wide = NULL;
    size_t allocation_size = 0;
    DWORD attributes = 0;
    MarimayFileStatus status;
    if (path == NULL || path_length == 0
        || memchr(path, 0, path_length) != NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    if (!mmay_utf8_validate(path, path_length, NULL)) {
        return MMAY_FILE_INVALID_UTF8;
    }
    status = mmay_windows_directory_wide_path(
        allocator, path, path_length, &wide, &allocation_size, NULL
    );
    if (status != MMAY_FILE_OK) return status;
    if (!CreateDirectoryW(wide, NULL)) {
        DWORD create_error = GetLastError();
        attributes = GetFileAttributesW(wide);
        status = create_error == ERROR_ALREADY_EXISTS
            && attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
            ? MMAY_FILE_OK : MMAY_FILE_IO_FAILED;
    }
    mmay_deallocate(&allocator, wide, allocation_size);
    return status;
}
