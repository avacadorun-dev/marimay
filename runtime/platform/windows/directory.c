#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ruda/platform/directory.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include "ruda/base/utf8.h"

typedef struct RudaWindowsDirectoryState {
    RudaAllocator allocator;
    HANDLE search;
    WIN32_FIND_DATAW data;
    bool first;
} RudaWindowsDirectoryState;

static RudaFileStatus ruda_windows_directory_wide_path(
    RudaAllocator allocator,
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
        return RUDA_FILE_INVALID_ARGUMENT;
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
        || !ruda_size_add((size_t)count, 1, &count_with_zero)
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
            count
        ) != count) {
        ruda_deallocate(&allocator, wide, byte_count);
        return RUDA_FILE_INVALID_UTF8;
    }
    wide[count] = L'\0';
    *wide_path = wide;
    *allocation_size = byte_count;
    if (wide_length != NULL) {
        *wide_length = (size_t)count;
    }
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_windows_directory_utf8(
    RudaAllocator allocator,
    const wchar_t *wide,
    size_t wide_length,
    size_t max_bytes,
    RudaOwnedBytes *output
) {
    int byte_count = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    if (wide_length > (size_t)INT_MAX) {
        return RUDA_FILE_LIMIT_EXCEEDED;
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
        return RUDA_FILE_INVALID_UTF8;
    }
    if ((size_t)byte_count > max_bytes) {
        return RUDA_FILE_LIMIT_EXCEEDED;
    }
    if (!ruda_size_add((size_t)byte_count, 1, &allocation_size)) {
        return RUDA_FILE_LIMIT_EXCEEDED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return RUDA_FILE_ALLOCATION_FAILED;
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
        ruda_deallocate(&allocator, bytes, allocation_size);
        return RUDA_FILE_INVALID_UTF8;
    }
    bytes[byte_count] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = (size_t)byte_count;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_windows_directory_current(
    void *context,
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
) {
    DWORD required = 0;
    DWORD written = 0;
    size_t allocation_size = 0;
    wchar_t *wide = NULL;
    RudaFileStatus status;
    size_t index = 0;
    (void)context;
    required = GetCurrentDirectoryW(0, NULL);
    if (required == 0
        || !ruda_size_multiply((size_t)required, sizeof(wchar_t), &allocation_size)) {
        return RUDA_FILE_IO_FAILED;
    }
    wide = ruda_allocate(&allocator, allocation_size);
    if (wide == NULL) {
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    written = GetCurrentDirectoryW(required, wide);
    if (written == 0 || written >= required) {
        ruda_deallocate(&allocator, wide, allocation_size);
        return RUDA_FILE_IO_FAILED;
    }
    status = ruda_windows_directory_utf8(
        allocator,
        wide,
        (size_t)written,
        max_path_bytes,
        path
    );
    ruda_deallocate(&allocator, wide, allocation_size);
    if (status == RUDA_FILE_OK) {
        for (index = 0; index < path->length; index += 1) {
            if (path->data[index] == '\\') {
                path->data[index] = '/';
            }
        }
    }
    return status;
}

static RudaFileStatus ruda_windows_directory_kind(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
) {
    wchar_t *wide = NULL;
    size_t allocation_size = 0;
    DWORD attributes;
    RudaFileStatus status;
    (void)context;
    status = ruda_windows_directory_wide_path(
        allocator,
        path,
        path_length,
        &wide,
        &allocation_size,
        NULL
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    attributes = GetFileAttributesW(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            || error == ERROR_INVALID_NAME) {
            *kind = RUDA_DIRECTORY_ENTRY_MISSING;
            status = RUDA_FILE_OK;
        } else {
            status = RUDA_FILE_IO_FAILED;
        }
    } else if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        *kind = RUDA_DIRECTORY_ENTRY_SYMLINK;
    } else if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        *kind = RUDA_DIRECTORY_ENTRY_DIRECTORY;
    } else if ((attributes & FILE_ATTRIBUTE_DEVICE) != 0) {
        *kind = RUDA_DIRECTORY_ENTRY_OTHER;
    } else {
        *kind = RUDA_DIRECTORY_ENTRY_REGULAR;
    }
    ruda_deallocate(&allocator, wide, allocation_size);
    return status;
}

static RudaFileStatus ruda_windows_directory_open(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryHandle *handle
) {
    size_t pattern_length = 0;
    size_t pattern_size = 0;
    char *pattern = NULL;
    wchar_t *wide = NULL;
    size_t wide_size = 0;
    RudaWindowsDirectoryState *state = NULL;
    RudaFileStatus status;
    bool separator = path_length != 0
        && path[path_length - 1] != '/'
        && path[path_length - 1] != '\\';
    (void)context;
    if (!ruda_size_add(path_length, separator ? 2 : 1, &pattern_length)
        || !ruda_size_add(pattern_length, 1, &pattern_size)) {
        return RUDA_FILE_LIMIT_EXCEEDED;
    }
    pattern = ruda_allocate(&allocator, pattern_size);
    if (pattern == NULL) {
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    memcpy(pattern, path, path_length);
    if (separator) {
        pattern[path_length] = '/';
    }
    pattern[path_length + (separator ? 1 : 0)] = '*';
    pattern[pattern_length] = '\0';
    status = ruda_windows_directory_wide_path(
        allocator,
        pattern,
        pattern_length,
        &wide,
        &wide_size,
        NULL
    );
    ruda_deallocate(&allocator, pattern, pattern_size);
    if (status != RUDA_FILE_OK) {
        return status;
    }
    state = ruda_allocate(&allocator, sizeof(*state));
    if (state == NULL) {
        ruda_deallocate(&allocator, wide, wide_size);
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    memset(state, 0, sizeof(*state));
    state->search = FindFirstFileW(wide, &state->data);
    ruda_deallocate(&allocator, wide, wide_size);
    if (state->search == INVALID_HANDLE_VALUE) {
        ruda_deallocate(&allocator, state, sizeof(*state));
        return RUDA_FILE_IO_FAILED;
    }
    state->allocator = allocator;
    state->first = true;
    *handle = (RudaDirectoryHandle)(uintptr_t)state;
    return RUDA_FILE_OK;
}

static RudaDirectoryEntryKind ruda_windows_directory_attribute_kind(
    DWORD attributes
) {
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return RUDA_DIRECTORY_ENTRY_SYMLINK;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return RUDA_DIRECTORY_ENTRY_DIRECTORY;
    }
    return (attributes & FILE_ATTRIBUTE_DEVICE) != 0
        ? RUDA_DIRECTORY_ENTRY_OTHER
        : RUDA_DIRECTORY_ENTRY_REGULAR;
}

static RudaFileStatus ruda_windows_directory_next(
    void *context,
    RudaDirectoryHandle handle,
    RudaAllocator allocator,
    size_t max_name_bytes,
    RudaOwnedBytes *name,
    RudaDirectoryEntryKind *kind,
    bool *end
) {
    RudaWindowsDirectoryState *state = (RudaWindowsDirectoryState *)(uintptr_t)handle;
    (void)context;
    for (;;) {
        const wchar_t *wide_name;
        size_t wide_length;
        RudaFileStatus status;
        if (state->first) {
            state->first = false;
        } else if (!FindNextFileW(state->search, &state->data)) {
            DWORD error = GetLastError();
            *end = error == ERROR_NO_MORE_FILES;
            return error == ERROR_NO_MORE_FILES
                ? RUDA_FILE_OK
                : RUDA_FILE_IO_FAILED;
        }
        wide_name = state->data.cFileName;
        if (wcscmp(wide_name, L".") == 0 || wcscmp(wide_name, L"..") == 0) {
            continue;
        }
        wide_length = wcslen(wide_name);
        status = ruda_windows_directory_utf8(
            allocator,
            wide_name,
            wide_length,
            max_name_bytes,
            name
        );
        if (status != RUDA_FILE_OK) {
            return status;
        }
        *kind = ruda_windows_directory_attribute_kind(state->data.dwFileAttributes);
        *end = false;
        return RUDA_FILE_OK;
    }
}

static RudaFileStatus ruda_windows_directory_close(
    void *context,
    RudaDirectoryHandle handle
) {
    RudaWindowsDirectoryState *state = (RudaWindowsDirectoryState *)(uintptr_t)handle;
    RudaAllocator allocator = state->allocator;
    RudaFileStatus status = FindClose(state->search)
        ? RUDA_FILE_OK
        : RUDA_FILE_IO_FAILED;
    (void)context;
    ruda_deallocate(&allocator, state, sizeof(*state));
    return status;
}

RudaDirectoryOps ruda_platform_directory_ops(void) {
    RudaDirectoryOps ops;
    ops.context = NULL;
    ops.current = ruda_windows_directory_current;
    ops.kind = ruda_windows_directory_kind;
    ops.open = ruda_windows_directory_open;
    ops.next = ruda_windows_directory_next;
    ops.close = ruda_windows_directory_close;
    return ops;
}

RudaFileStatus ruda_directory_create(
    RudaAllocator allocator,
    const char *path,
    size_t path_length
) {
    wchar_t *wide = NULL;
    size_t allocation_size = 0;
    DWORD attributes = 0;
    RudaFileStatus status;
    if (path == NULL || path_length == 0
        || memchr(path, 0, path_length) != NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    if (!ruda_utf8_validate(path, path_length, NULL)) {
        return RUDA_FILE_INVALID_UTF8;
    }
    status = ruda_windows_directory_wide_path(
        allocator, path, path_length, &wide, &allocation_size, NULL
    );
    if (status != RUDA_FILE_OK) return status;
    if (!CreateDirectoryW(wide, NULL)) {
        DWORD create_error = GetLastError();
        attributes = GetFileAttributesW(wide);
        status = create_error == ERROR_ALREADY_EXISTS
            && attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
            ? RUDA_FILE_OK : RUDA_FILE_IO_FAILED;
    }
    ruda_deallocate(&allocator, wide, allocation_size);
    return status;
}
