#include "ruda/platform/file.h"

#include <stdbool.h>
#include <string.h>

#include "ruda/base/utf8.h"
#include "ruda/base/vector.h"

static bool ruda_file_path_valid(const char *path, size_t length) {
    return path != NULL
        && length != 0
        && memchr(path, 0, length) == NULL
        && ruda_utf8_validate(path, length, NULL);
}

RudaFileStatus ruda_file_read_all_with_ops(
    RudaFileReadOps ops,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    size_t max_bytes,
    RudaOwnedBytes *output
) {
    RudaVector bytes;
    RudaFileHandle handle = 0;
    RudaFileStatus status = RUDA_FILE_OK;
    unsigned char chunk[4096];
    if (output == NULL) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (ops.open == NULL || ops.read == NULL || ops.close == NULL
        || max_bytes == 0 || !ruda_file_path_valid(path, path_length)) {
        return path != NULL && path_length != 0
            && !ruda_utf8_validate(path, path_length, NULL)
            ? RUDA_FILE_INVALID_UTF8
            : RUDA_FILE_INVALID_ARGUMENT;
    }
    if (!ruda_vector_init(&bytes, allocator, sizeof(unsigned char))) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    status = ops.open(
        ops.context,
        allocator,
        path,
        path_length,
        &handle
    );
    if (status != RUDA_FILE_OK) {
        goto cleanup;
    }
    for (;;) {
        size_t read_count = 0;
        bool end = false;
        status = ops.read(
            ops.context,
            handle,
            chunk,
            sizeof(chunk),
            &read_count,
            &end
        );
        if (status != RUDA_FILE_OK || read_count > sizeof(chunk)
            || (read_count == 0 && !end)) {
            if (status == RUDA_FILE_OK) {
                status = RUDA_FILE_IO_FAILED;
            }
            break;
        }
        if (read_count > max_bytes - bytes.length) {
            status = RUDA_FILE_LIMIT_EXCEEDED;
            break;
        }
        if (!ruda_vector_append(&bytes, chunk, read_count)) {
            status = RUDA_FILE_ALLOCATION_FAILED;
            break;
        }
        if (end) {
            break;
        }
    }
    {
        RudaFileStatus close_status = ops.close(ops.context, handle);
        if (status == RUDA_FILE_OK && close_status != RUDA_FILE_OK) {
            status = close_status;
        }
    }
    if (status == RUDA_FILE_OK
        && !ruda_owned_bytes_copy(output, allocator, bytes.data, bytes.length)) {
        status = RUDA_FILE_ALLOCATION_FAILED;
    }

cleanup:
    ruda_vector_dispose(&bytes);
    if (status != RUDA_FILE_OK) {
        ruda_owned_bytes_dispose(output);
    }
    return status;
}

RudaFileStatus ruda_file_read_all(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    size_t max_bytes,
    RudaOwnedBytes *output
) {
    return ruda_file_read_all_with_ops(
        ruda_platform_file_read_ops(),
        allocator,
        path,
        path_length,
        max_bytes,
        output
    );
}

static size_t ruda_file_parent_length(const char *path, size_t length) {
    size_t index = length;
    while (index != 0) {
        unsigned char byte = (unsigned char)path[index - 1];
        if (byte == '/' || byte == '\\') {
            return index;
        }
        index -= 1;
    }
    return 0;
}

static bool ruda_file_paths_are_distinct_siblings(
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    size_t temporary_parent_length = ruda_file_parent_length(
        temporary_path,
        temporary_path_length
    );
    size_t destination_parent_length = ruda_file_parent_length(
        destination_path,
        destination_path_length
    );
    bool same_path = temporary_path_length == destination_path_length
        && memcmp(temporary_path, destination_path, temporary_path_length) == 0;
    return !same_path
        && temporary_parent_length == destination_parent_length
        && memcmp(
            temporary_path,
            destination_path,
            temporary_parent_length
        ) == 0;
}

RudaFileStatus ruda_file_replace_atomic_with_ops(
    RudaFileOps ops,
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length,
    const unsigned char *bytes,
    size_t length
) {
    RudaFileHandle handle = 0;
    RudaFileStatus status = RUDA_FILE_OK;
    bool created = false;
    bool open = false;
    size_t offset = 0;

    if (ops.create_exclusive == NULL || ops.write == NULL || ops.sync == NULL
        || ops.close == NULL || ops.replace == NULL || ops.remove == NULL
        || (bytes == NULL && length != 0)) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    if (!ruda_file_path_valid(temporary_path, temporary_path_length)
        || !ruda_file_path_valid(destination_path, destination_path_length)) {
        if ((temporary_path != NULL && temporary_path_length != 0
                && !ruda_utf8_validate(temporary_path, temporary_path_length, NULL))
            || (destination_path != NULL && destination_path_length != 0
                && !ruda_utf8_validate(destination_path, destination_path_length, NULL))) {
            return RUDA_FILE_INVALID_UTF8;
        }
        return RUDA_FILE_INVALID_ARGUMENT;
    }
    if (!ruda_file_paths_are_distinct_siblings(
            temporary_path,
            temporary_path_length,
            destination_path,
            destination_path_length
        )) {
        return RUDA_FILE_INVALID_ARGUMENT;
    }

    status = ops.create_exclusive(
        ops.context,
        allocator,
        temporary_path,
        temporary_path_length,
        &handle
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    created = true;
    open = true;

    while (offset < length) {
        size_t written = 0;
        status = ops.write(
            ops.context,
            handle,
            bytes + offset,
            length - offset,
            &written
        );
        if (status != RUDA_FILE_OK || written == 0 || written > length - offset) {
            if (status == RUDA_FILE_OK) {
                status = RUDA_FILE_IO_FAILED;
            }
            goto cleanup;
        }
        offset += written;
    }

    status = ops.sync(ops.context, handle);
    if (status != RUDA_FILE_OK) {
        goto cleanup;
    }
    status = ops.close(ops.context, handle);
    open = false;
    if (status != RUDA_FILE_OK) {
        goto cleanup;
    }
    status = ops.replace(
        ops.context,
        allocator,
        temporary_path,
        temporary_path_length,
        destination_path,
        destination_path_length
    );
    if (status == RUDA_FILE_OK) {
        created = false;
    }

cleanup:
    if (open) {
        (void)ops.close(ops.context, handle);
    }
    if (created) {
        ops.remove(
            ops.context,
            allocator,
            temporary_path,
            temporary_path_length
        );
    }
    return status;
}

RudaFileStatus ruda_file_replace_atomic(
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length,
    const unsigned char *bytes,
    size_t length
) {
    return ruda_file_replace_atomic_with_ops(
        ruda_platform_file_ops(),
        allocator,
        temporary_path,
        temporary_path_length,
        destination_path,
        destination_path_length,
        bytes,
        length
    );
}
