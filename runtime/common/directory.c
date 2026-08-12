#include "ruda/platform/directory.h"

#include <string.h>

#include "ruda/base/utf8.h"

static bool ruda_directory_path_valid(const char *path, size_t length) {
    return path != NULL && length != 0
        && memchr(path, 0, length) == NULL
        && ruda_utf8_validate(path, length, NULL);
}

static bool ruda_directory_ops_valid(RudaDirectoryOps ops) {
    return ops.current != NULL && ops.kind != NULL && ops.open != NULL
        && ops.next != NULL && ops.close != NULL;
}

RudaFileStatus ruda_directory_current_with_ops(
    RudaDirectoryOps ops,
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
) {
    RudaFileStatus status;
    if (path == NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    memset(path, 0, sizeof(*path));
    if (!ruda_directory_ops_valid(ops) || max_path_bytes == 0) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    status = ops.current(ops.context, allocator, max_path_bytes, path);
    if (status != RUDA_FILE_OK) {
        ruda_owned_bytes_dispose(path);
        return status;
    }
    if (path->data == NULL || path->length == 0
        || path->length > max_path_bytes
        || memchr(path->data, 0, path->length) != NULL
        || !ruda_utf8_validate(
            (const char *)path->data,
            path->length,
            NULL
        )) {
        ruda_owned_bytes_dispose(path);
        return RUDA_FILE_IO_FAILED;
    }
    return RUDA_FILE_OK;
}

RudaFileStatus ruda_directory_current(
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
) {
    return ruda_directory_current_with_ops(
        ruda_platform_directory_ops(),
        allocator,
        max_path_bytes,
        path
    );
}

RudaFileStatus ruda_directory_kind_with_ops(
    RudaDirectoryOps ops,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
) {
    if (!ruda_directory_ops_valid(ops) || kind == NULL
        || !ruda_directory_path_valid(path, path_length)) {
        return path != NULL && path_length != 0
            && !ruda_utf8_validate(path, path_length, NULL)
            ? RUDA_FILE_INVALID_UTF8
            : RUDA_FILE_INVALID_ARGUMENT;
    }
    *kind = RUDA_DIRECTORY_ENTRY_MISSING;
    return ops.kind(ops.context, allocator, path, path_length, kind);
}

RudaFileStatus ruda_directory_kind(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
) {
    return ruda_directory_kind_with_ops(
        ruda_platform_directory_ops(),
        allocator,
        path,
        path_length,
        kind
    );
}

RudaFileStatus ruda_directory_open_with_ops(
    RudaDirectoryOps ops,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryIterator *iterator
) {
    RudaFileStatus status;
    if (iterator == NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    memset(iterator, 0, sizeof(*iterator));
    if (!ruda_directory_ops_valid(ops)
        || !ruda_directory_path_valid(path, path_length)) {
        return path != NULL && path_length != 0
            && !ruda_utf8_validate(path, path_length, NULL)
            ? RUDA_FILE_INVALID_UTF8
            : RUDA_FILE_INVALID_ARGUMENT;
    }
    status = ops.open(
        ops.context,
        allocator,
        path,
        path_length,
        &iterator->handle
    );
    if (status == RUDA_FILE_OK) {
        iterator->ops = ops;
        iterator->open = true;
    }
    return status;
}

RudaFileStatus ruda_directory_open(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryIterator *iterator
) {
    return ruda_directory_open_with_ops(
        ruda_platform_directory_ops(),
        allocator,
        path,
        path_length,
        iterator
    );
}

RudaFileStatus ruda_directory_next(
    RudaDirectoryIterator *iterator,
    RudaAllocator allocator,
    size_t max_name_bytes,
    RudaOwnedBytes *name,
    RudaDirectoryEntryKind *kind,
    bool *end
) {
    RudaFileStatus status;
    if (name == NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    memset(name, 0, sizeof(*name));
    if (iterator == NULL || !iterator->open || kind == NULL || end == NULL
        || max_name_bytes == 0) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    *kind = RUDA_DIRECTORY_ENTRY_MISSING;
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
    if (status != RUDA_FILE_OK) {
        ruda_owned_bytes_dispose(name);
        return status;
    }
    if (*end) {
        if (name->data != NULL || name->length != 0) {
            ruda_owned_bytes_dispose(name);
            return RUDA_FILE_IO_FAILED;
        }
        return RUDA_FILE_OK;
    }
    if (name->data == NULL || name->length == 0
        || name->length > max_name_bytes
        || memchr(name->data, 0, name->length) != NULL
        || memchr(name->data, '/', name->length) != NULL
        || memchr(name->data, '\\', name->length) != NULL
        || !ruda_utf8_validate(
            (const char *)name->data,
            name->length,
            NULL
        )
        || (name->length == 1 && name->data[0] == '.')
        || (name->length == 2 && name->data[0] == '.'
            && name->data[1] == '.')) {
        ruda_owned_bytes_dispose(name);
        return RUDA_FILE_IO_FAILED;
    }
    return RUDA_FILE_OK;
}

RudaFileStatus ruda_directory_close(RudaDirectoryIterator *iterator) {
    RudaFileStatus status;
    if (iterator == NULL || !iterator->open) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    status = iterator->ops.close(iterator->ops.context, iterator->handle);
    memset(iterator, 0, sizeof(*iterator));
    return status;
}
