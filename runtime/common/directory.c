#include "mmay/platform/directory.h"

#include <string.h>

#include "mmay/base/utf8.h"

static bool mmay_directory_path_valid(const char *path, size_t length) {
    return path != NULL && length != 0
        && memchr(path, 0, length) == NULL
        && mmay_utf8_validate(path, length, NULL);
}

static bool mmay_directory_ops_valid(MarimayDirectoryOps ops) {
    return ops.current != NULL && ops.kind != NULL && ops.open != NULL
        && ops.next != NULL && ops.close != NULL;
}

MarimayFileStatus mmay_directory_current_with_ops(
    MarimayDirectoryOps ops,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
) {
    MarimayFileStatus status;
    if (path == NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    memset(path, 0, sizeof(*path));
    if (!mmay_directory_ops_valid(ops) || max_path_bytes == 0) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    status = ops.current(ops.context, allocator, max_path_bytes, path);
    if (status != MMAY_FILE_OK) {
        mmay_owned_bytes_dispose(path);
        return status;
    }
    if (path->data == NULL || path->length == 0
        || path->length > max_path_bytes
        || memchr(path->data, 0, path->length) != NULL
        || !mmay_utf8_validate(
            (const char *)path->data,
            path->length,
            NULL
        )) {
        mmay_owned_bytes_dispose(path);
        return MMAY_FILE_IO_FAILED;
    }
    return MMAY_FILE_OK;
}

MarimayFileStatus mmay_directory_current(
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
) {
    return mmay_directory_current_with_ops(
        mmay_platform_directory_ops(),
        allocator,
        max_path_bytes,
        path
    );
}

MarimayFileStatus mmay_directory_kind_with_ops(
    MarimayDirectoryOps ops,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
) {
    if (!mmay_directory_ops_valid(ops) || kind == NULL
        || !mmay_directory_path_valid(path, path_length)) {
        return path != NULL && path_length != 0
            && !mmay_utf8_validate(path, path_length, NULL)
            ? MMAY_FILE_INVALID_UTF8
            : MMAY_FILE_INVALID_ARGUMENT;
    }
    *kind = MMAY_DIRECTORY_ENTRY_MISSING;
    return ops.kind(ops.context, allocator, path, path_length, kind);
}

MarimayFileStatus mmay_directory_kind(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
) {
    return mmay_directory_kind_with_ops(
        mmay_platform_directory_ops(),
        allocator,
        path,
        path_length,
        kind
    );
}

MarimayFileStatus mmay_directory_open_with_ops(
    MarimayDirectoryOps ops,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryIterator *iterator
) {
    MarimayFileStatus status;
    if (iterator == NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    memset(iterator, 0, sizeof(*iterator));
    if (!mmay_directory_ops_valid(ops)
        || !mmay_directory_path_valid(path, path_length)) {
        return path != NULL && path_length != 0
            && !mmay_utf8_validate(path, path_length, NULL)
            ? MMAY_FILE_INVALID_UTF8
            : MMAY_FILE_INVALID_ARGUMENT;
    }
    status = ops.open(
        ops.context,
        allocator,
        path,
        path_length,
        &iterator->handle
    );
    if (status == MMAY_FILE_OK) {
        iterator->ops = ops;
        iterator->open = true;
    }
    return status;
}

MarimayFileStatus mmay_directory_open(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryIterator *iterator
) {
    return mmay_directory_open_with_ops(
        mmay_platform_directory_ops(),
        allocator,
        path,
        path_length,
        iterator
    );
}

MarimayFileStatus mmay_directory_next(
    MarimayDirectoryIterator *iterator,
    MarimayAllocator allocator,
    size_t max_name_bytes,
    MarimayOwnedBytes *name,
    MarimayDirectoryEntryKind *kind,
    bool *end
) {
    MarimayFileStatus status;
    if (name == NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    memset(name, 0, sizeof(*name));
    if (iterator == NULL || !iterator->open || kind == NULL || end == NULL
        || max_name_bytes == 0) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    *kind = MMAY_DIRECTORY_ENTRY_MISSING;
    *end = false;
    status = iterator->ops.next(
        iterator->ops.context,
        iterator->handle,
        allocator,
        max_name_bytes,
        name,
        kind,
        end
    );
    if (status != MMAY_FILE_OK) {
        mmay_owned_bytes_dispose(name);
        return status;
    }
    if (*end) {
        if (name->data != NULL || name->length != 0) {
            mmay_owned_bytes_dispose(name);
            return MMAY_FILE_IO_FAILED;
        }
        return MMAY_FILE_OK;
    }
    if (name->data == NULL || name->length == 0
        || name->length > max_name_bytes
        || memchr(name->data, 0, name->length) != NULL
        || memchr(name->data, '/', name->length) != NULL
        || memchr(name->data, '\\', name->length) != NULL
        || !mmay_utf8_validate(
            (const char *)name->data,
            name->length,
            NULL
        )
        || (name->length == 1 && name->data[0] == '.')
        || (name->length == 2 && name->data[0] == '.'
            && name->data[1] == '.')) {
        mmay_owned_bytes_dispose(name);
        return MMAY_FILE_IO_FAILED;
    }
    return MMAY_FILE_OK;
}

MarimayFileStatus mmay_directory_close(MarimayDirectoryIterator *iterator) {
    MarimayFileStatus status;
    if (iterator == NULL || !iterator->open) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    status = iterator->ops.close(iterator->ops.context, iterator->handle);
    memset(iterator, 0, sizeof(*iterator));
    return status;
}
