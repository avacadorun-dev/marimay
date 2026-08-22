#define _POSIX_C_SOURCE 200809L

#include "mmay/platform/directory.h"

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mmay/base/utf8.h"

typedef struct MarimayPosixDirectoryState {
    MarimayAllocator allocator;
    DIR *directory;
    MarimayOwnedBytes path;
} MarimayPosixDirectoryState;

static MarimayFileStatus mmay_posix_directory_owned_path(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayOwnedBytes *owned
) {
    return mmay_owned_bytes_copy(owned, allocator, path, path_length)
        ? MMAY_FILE_OK
        : MMAY_FILE_ALLOCATION_FAILED;
}

static MarimayFileStatus mmay_posix_directory_current(
    void *context,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
) {
    size_t maximum_capacity = 0;
    size_t capacity = 256;
    char *buffer = NULL;
    (void)context;
    if (!mmay_size_add(max_path_bytes, 1, &maximum_capacity)) {
        return MMAY_FILE_LIMIT_EXCEEDED;
    }
    if (capacity > maximum_capacity) {
        capacity = maximum_capacity;
    }
    for (;;) {
        size_t length = 0;
        MarimayFileStatus status;
        buffer = mmay_allocate(&allocator, capacity);
        if (buffer == NULL) {
            return MMAY_FILE_ALLOCATION_FAILED;
        }
        errno = 0;
        if (getcwd(buffer, capacity) != NULL) {
            length = strlen(buffer);
            status = length <= max_path_bytes
                && mmay_owned_bytes_copy(path, allocator, buffer, length)
                ? MMAY_FILE_OK
                : (length > max_path_bytes
                    ? MMAY_FILE_LIMIT_EXCEEDED
                    : MMAY_FILE_ALLOCATION_FAILED);
            mmay_deallocate(&allocator, buffer, capacity);
            return status;
        }
        {
            int failure = errno;
            status = failure == ERANGE
            ? MMAY_FILE_LIMIT_EXCEEDED
            : MMAY_FILE_IO_FAILED;
        }
        mmay_deallocate(&allocator, buffer, capacity);
        buffer = NULL;
        if (status != MMAY_FILE_LIMIT_EXCEEDED
            || capacity == maximum_capacity) {
            return status;
        }
        capacity = capacity > maximum_capacity / 2
            ? maximum_capacity
            : capacity * 2;
    }
}

static MarimayFileStatus mmay_posix_directory_kind(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
) {
    MarimayOwnedBytes owned = {0};
    struct stat information;
    MarimayFileStatus status;
    (void)context;
    status = mmay_posix_directory_owned_path(
        allocator,
        path,
        path_length,
        &owned
    );
    if (status != MMAY_FILE_OK) {
        return status;
    }
    if (lstat((const char *)owned.data, &information) != 0) {
        status = errno == ENOENT || errno == ENOTDIR
            ? MMAY_FILE_OK
            : MMAY_FILE_IO_FAILED;
        *kind = MMAY_DIRECTORY_ENTRY_MISSING;
    } else if (S_ISREG(information.st_mode)) {
        *kind = MMAY_DIRECTORY_ENTRY_REGULAR;
    } else if (S_ISDIR(information.st_mode)) {
        *kind = MMAY_DIRECTORY_ENTRY_DIRECTORY;
    } else if (S_ISLNK(information.st_mode)) {
        *kind = MMAY_DIRECTORY_ENTRY_SYMLINK;
    } else {
        *kind = MMAY_DIRECTORY_ENTRY_OTHER;
    }
    mmay_owned_bytes_dispose(&owned);
    return status;
}

static MarimayFileStatus mmay_posix_directory_open(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryHandle *handle
) {
    MarimayOwnedBytes owned = {0};
    MarimayPosixDirectoryState *state = NULL;
    MarimayFileStatus status;
    (void)context;
    status = mmay_posix_directory_owned_path(
        allocator,
        path,
        path_length,
        &owned
    );
    if (status != MMAY_FILE_OK) {
        return status;
    }
    state = mmay_allocate(&allocator, sizeof(*state));
    if (state == NULL) {
        mmay_owned_bytes_dispose(&owned);
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    memset(state, 0, sizeof(*state));
    state->directory = opendir((const char *)owned.data);
    if (state->directory == NULL) {
        mmay_deallocate(&allocator, state, sizeof(*state));
        mmay_owned_bytes_dispose(&owned);
        return MMAY_FILE_IO_FAILED;
    }
    state->allocator = allocator;
    state->path = owned;
    *handle = (MarimayDirectoryHandle)(uintptr_t)state;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_posix_directory_entry_kind(
    MarimayPosixDirectoryState *state,
    const char *name,
    size_t name_length,
    MarimayDirectoryEntryKind *kind
) {
    bool separator = state->path.length != 0
        && state->path.data[state->path.length - 1] != '/';
    size_t joined_length = 0;
    size_t allocation_size = 0;
    char *joined = NULL;
    MarimayFileStatus status;
    if (!mmay_size_add(state->path.length, separator ? 1 : 0, &joined_length)
        || !mmay_size_add(joined_length, name_length, &joined_length)
        || !mmay_size_add(joined_length, 1, &allocation_size)) {
        return MMAY_FILE_LIMIT_EXCEEDED;
    }
    joined = mmay_allocate(&state->allocator, allocation_size);
    if (joined == NULL) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    memcpy(joined, state->path.data, state->path.length);
    if (separator) {
        joined[state->path.length] = '/';
    }
    memcpy(
        joined + state->path.length + (separator ? 1 : 0),
        name,
        name_length
    );
    joined[joined_length] = '\0';
    status = mmay_posix_directory_kind(
        NULL,
        state->allocator,
        joined,
        joined_length,
        kind
    );
    mmay_deallocate(&state->allocator, joined, allocation_size);
    return status;
}

static MarimayFileStatus mmay_posix_directory_next(
    void *context,
    MarimayDirectoryHandle handle,
    MarimayAllocator allocator,
    size_t max_name_bytes,
    MarimayOwnedBytes *name,
    MarimayDirectoryEntryKind *kind,
    bool *end
) {
    MarimayPosixDirectoryState *state = (MarimayPosixDirectoryState *)(uintptr_t)handle;
    (void)context;
    for (;;) {
        struct dirent *entry = NULL;
        size_t length = 0;
        MarimayFileStatus status;
        errno = 0;
        entry = readdir(state->directory);
        if (entry == NULL) {
            *end = errno == 0;
            return errno == 0 ? MMAY_FILE_OK : MMAY_FILE_IO_FAILED;
        }
        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        length = strlen(entry->d_name);
        if (length > max_name_bytes) {
            return MMAY_FILE_LIMIT_EXCEEDED;
        }
        if (!mmay_utf8_validate(entry->d_name, length, NULL)) {
            return MMAY_FILE_INVALID_UTF8;
        }
        status = mmay_posix_directory_entry_kind(
            state,
            entry->d_name,
            length,
            kind
        );
        if (status != MMAY_FILE_OK) {
            return status;
        }
        if (*kind == MMAY_DIRECTORY_ENTRY_MISSING) {
            continue;
        }
        if (!mmay_owned_bytes_copy(name, allocator, entry->d_name, length)) {
            return MMAY_FILE_ALLOCATION_FAILED;
        }
        *end = false;
        return MMAY_FILE_OK;
    }
}

static MarimayFileStatus mmay_posix_directory_close(
    void *context,
    MarimayDirectoryHandle handle
) {
    MarimayPosixDirectoryState *state = (MarimayPosixDirectoryState *)(uintptr_t)handle;
    MarimayAllocator allocator = state->allocator;
    MarimayFileStatus status;
    (void)context;
    status = closedir(state->directory) == 0
        ? MMAY_FILE_OK
        : MMAY_FILE_IO_FAILED;
    mmay_owned_bytes_dispose(&state->path);
    mmay_deallocate(&allocator, state, sizeof(*state));
    return status;
}

MarimayDirectoryOps mmay_platform_directory_ops(void) {
    MarimayDirectoryOps ops;
    ops.context = NULL;
    ops.current = mmay_posix_directory_current;
    ops.kind = mmay_posix_directory_kind;
    ops.open = mmay_posix_directory_open;
    ops.next = mmay_posix_directory_next;
    ops.close = mmay_posix_directory_close;
    return ops;
}

MarimayFileStatus mmay_directory_create(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
) {
    MarimayOwnedBytes owned = {0};
    struct stat information;
    MarimayFileStatus status;
    if (path == NULL || path_length == 0
        || memchr(path, 0, path_length) != NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    if (!mmay_utf8_validate(path, path_length, NULL)) {
        return MMAY_FILE_INVALID_UTF8;
    }
    status = mmay_posix_directory_owned_path(
        allocator, path, path_length, &owned
    );
    if (status != MMAY_FILE_OK) return status;
    if (mkdir((const char *)owned.data, 0700) != 0) {
        status = errno == EEXIST
            && stat((const char *)owned.data, &information) == 0
            && S_ISDIR(information.st_mode)
            ? MMAY_FILE_OK : MMAY_FILE_IO_FAILED;
    }
    mmay_owned_bytes_dispose(&owned);
    return status;
}
