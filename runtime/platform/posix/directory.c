#define _POSIX_C_SOURCE 200809L

#include "ruda/platform/directory.h"

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ruda/base/utf8.h"

typedef struct RudaPosixDirectoryState {
    RudaAllocator allocator;
    DIR *directory;
    RudaOwnedBytes path;
} RudaPosixDirectoryState;

static RudaFileStatus ruda_posix_directory_owned_path(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaOwnedBytes *owned
) {
    return ruda_owned_bytes_copy(owned, allocator, path, path_length)
        ? RUDA_FILE_OK
        : RUDA_FILE_ALLOCATION_FAILED;
}

static RudaFileStatus ruda_posix_directory_current(
    void *context,
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
) {
    size_t maximum_capacity = 0;
    size_t capacity = 256;
    char *buffer = NULL;
    (void)context;
    if (!ruda_size_add(max_path_bytes, 1, &maximum_capacity)) {
        return RUDA_FILE_LIMIT_EXCEEDED;
    }
    if (capacity > maximum_capacity) {
        capacity = maximum_capacity;
    }
    for (;;) {
        size_t length = 0;
        RudaFileStatus status;
        buffer = ruda_allocate(&allocator, capacity);
        if (buffer == NULL) {
            return RUDA_FILE_ALLOCATION_FAILED;
        }
        errno = 0;
        if (getcwd(buffer, capacity) != NULL) {
            length = strlen(buffer);
            status = length <= max_path_bytes
                && ruda_owned_bytes_copy(path, allocator, buffer, length)
                ? RUDA_FILE_OK
                : (length > max_path_bytes
                    ? RUDA_FILE_LIMIT_EXCEEDED
                    : RUDA_FILE_ALLOCATION_FAILED);
            ruda_deallocate(&allocator, buffer, capacity);
            return status;
        }
        {
            int failure = errno;
            status = failure == ERANGE
            ? RUDA_FILE_LIMIT_EXCEEDED
            : RUDA_FILE_IO_FAILED;
        }
        ruda_deallocate(&allocator, buffer, capacity);
        buffer = NULL;
        if (status != RUDA_FILE_LIMIT_EXCEEDED
            || capacity == maximum_capacity) {
            return status;
        }
        capacity = capacity > maximum_capacity / 2
            ? maximum_capacity
            : capacity * 2;
    }
}

static RudaFileStatus ruda_posix_directory_kind(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
) {
    RudaOwnedBytes owned = {0};
    struct stat information;
    RudaFileStatus status;
    (void)context;
    status = ruda_posix_directory_owned_path(
        allocator,
        path,
        path_length,
        &owned
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    if (lstat((const char *)owned.data, &information) != 0) {
        status = errno == ENOENT || errno == ENOTDIR
            ? RUDA_FILE_OK
            : RUDA_FILE_IO_FAILED;
        *kind = RUDA_DIRECTORY_ENTRY_MISSING;
    } else if (S_ISREG(information.st_mode)) {
        *kind = RUDA_DIRECTORY_ENTRY_REGULAR;
    } else if (S_ISDIR(information.st_mode)) {
        *kind = RUDA_DIRECTORY_ENTRY_DIRECTORY;
    } else if (S_ISLNK(information.st_mode)) {
        *kind = RUDA_DIRECTORY_ENTRY_SYMLINK;
    } else {
        *kind = RUDA_DIRECTORY_ENTRY_OTHER;
    }
    ruda_owned_bytes_dispose(&owned);
    return status;
}

static RudaFileStatus ruda_posix_directory_open(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryHandle *handle
) {
    RudaOwnedBytes owned = {0};
    RudaPosixDirectoryState *state = NULL;
    RudaFileStatus status;
    (void)context;
    status = ruda_posix_directory_owned_path(
        allocator,
        path,
        path_length,
        &owned
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    state = ruda_allocate(&allocator, sizeof(*state));
    if (state == NULL) {
        ruda_owned_bytes_dispose(&owned);
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    memset(state, 0, sizeof(*state));
    state->directory = opendir((const char *)owned.data);
    if (state->directory == NULL) {
        ruda_deallocate(&allocator, state, sizeof(*state));
        ruda_owned_bytes_dispose(&owned);
        return RUDA_FILE_IO_FAILED;
    }
    state->allocator = allocator;
    state->path = owned;
    *handle = (RudaDirectoryHandle)(uintptr_t)state;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_posix_directory_entry_kind(
    RudaPosixDirectoryState *state,
    const char *name,
    size_t name_length,
    RudaDirectoryEntryKind *kind
) {
    bool separator = state->path.length != 0
        && state->path.data[state->path.length - 1] != '/';
    size_t joined_length = 0;
    size_t allocation_size = 0;
    char *joined = NULL;
    RudaFileStatus status;
    if (!ruda_size_add(state->path.length, separator ? 1 : 0, &joined_length)
        || !ruda_size_add(joined_length, name_length, &joined_length)
        || !ruda_size_add(joined_length, 1, &allocation_size)) {
        return RUDA_FILE_LIMIT_EXCEEDED;
    }
    joined = ruda_allocate(&state->allocator, allocation_size);
    if (joined == NULL) {
        return RUDA_FILE_ALLOCATION_FAILED;
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
    status = ruda_posix_directory_kind(
        NULL,
        state->allocator,
        joined,
        joined_length,
        kind
    );
    ruda_deallocate(&state->allocator, joined, allocation_size);
    return status;
}

static RudaFileStatus ruda_posix_directory_next(
    void *context,
    RudaDirectoryHandle handle,
    RudaAllocator allocator,
    size_t max_name_bytes,
    RudaOwnedBytes *name,
    RudaDirectoryEntryKind *kind,
    bool *end
) {
    RudaPosixDirectoryState *state = (RudaPosixDirectoryState *)(uintptr_t)handle;
    (void)context;
    for (;;) {
        struct dirent *entry = NULL;
        size_t length = 0;
        RudaFileStatus status;
        errno = 0;
        entry = readdir(state->directory);
        if (entry == NULL) {
            *end = errno == 0;
            return errno == 0 ? RUDA_FILE_OK : RUDA_FILE_IO_FAILED;
        }
        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        length = strlen(entry->d_name);
        if (length > max_name_bytes) {
            return RUDA_FILE_LIMIT_EXCEEDED;
        }
        if (!ruda_utf8_validate(entry->d_name, length, NULL)) {
            return RUDA_FILE_INVALID_UTF8;
        }
        status = ruda_posix_directory_entry_kind(
            state,
            entry->d_name,
            length,
            kind
        );
        if (status != RUDA_FILE_OK) {
            return status;
        }
        if (*kind == RUDA_DIRECTORY_ENTRY_MISSING) {
            continue;
        }
        if (!ruda_owned_bytes_copy(name, allocator, entry->d_name, length)) {
            return RUDA_FILE_ALLOCATION_FAILED;
        }
        *end = false;
        return RUDA_FILE_OK;
    }
}

static RudaFileStatus ruda_posix_directory_close(
    void *context,
    RudaDirectoryHandle handle
) {
    RudaPosixDirectoryState *state = (RudaPosixDirectoryState *)(uintptr_t)handle;
    RudaAllocator allocator = state->allocator;
    RudaFileStatus status;
    (void)context;
    status = closedir(state->directory) == 0
        ? RUDA_FILE_OK
        : RUDA_FILE_IO_FAILED;
    ruda_owned_bytes_dispose(&state->path);
    ruda_deallocate(&allocator, state, sizeof(*state));
    return status;
}

RudaDirectoryOps ruda_platform_directory_ops(void) {
    RudaDirectoryOps ops;
    ops.context = NULL;
    ops.current = ruda_posix_directory_current;
    ops.kind = ruda_posix_directory_kind;
    ops.open = ruda_posix_directory_open;
    ops.next = ruda_posix_directory_next;
    ops.close = ruda_posix_directory_close;
    return ops;
}

RudaFileStatus ruda_directory_create(
    RudaAllocator allocator,
    const char *path,
    size_t path_length
) {
    RudaOwnedBytes owned = {0};
    struct stat information;
    RudaFileStatus status;
    if (path == NULL || path_length == 0
        || memchr(path, 0, path_length) != NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    if (!ruda_utf8_validate(path, path_length, NULL)) {
        return RUDA_FILE_INVALID_UTF8;
    }
    status = ruda_posix_directory_owned_path(
        allocator, path, path_length, &owned
    );
    if (status != RUDA_FILE_OK) return status;
    if (mkdir((const char *)owned.data, 0700) != 0) {
        status = errno == EEXIST
            && stat((const char *)owned.data, &information) == 0
            && S_ISDIR(information.st_mode)
            ? RUDA_FILE_OK : RUDA_FILE_IO_FAILED;
    }
    ruda_owned_bytes_dispose(&owned);
    return status;
}
