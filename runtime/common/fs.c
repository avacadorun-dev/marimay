#include "mmay/runtime/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/utf8.h"

typedef struct MarimayFsOpenFile {
    MarimayFileHandle platform_handle;
    bool open;
} MarimayFsOpenFile;

static MarimayFileStatus mmay_fs_platform_metadata(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileMetadata *metadata
) {
    (void)context;
    return mmay_file_metadata(
        allocator, path, path_length, metadata
    );
}

MarimayFsOptions mmay_fs_default_options(void) {
    MarimayFsOptions options;
    memset(&options, 0, sizeof(options));
#if defined(_WIN32)
    options.flavor = MMAY_PATH_WINDOWS;
#else
    options.flavor = MMAY_PATH_POSIX;
#endif
    options.max_open_files = 1024;
    options.read_ops = mmay_platform_file_read_ops();
    options.write_ops = mmay_platform_file_ops();
    options.directory_ops = mmay_platform_directory_ops();
    options.metadata = mmay_fs_platform_metadata;
    return options;
}

static bool mmay_fs_path_equal_byte(
    MarimayPathFlavor flavor,
    unsigned char left,
    unsigned char right
) {
    if (flavor == MMAY_PATH_WINDOWS) {
        if (left >= 'a' && left <= 'z') left = (unsigned char)(left - 32U);
        if (right >= 'a' && right <= 'z') right = (unsigned char)(right - 32U);
    }
    return left == right;
}

static bool mmay_fs_path_contains(
    MarimayPathFlavor flavor,
    const MarimayOwnedBytes *root,
    const MarimayOwnedBytes *path
) {
    size_t index = 0;
    if (root == NULL || path == NULL || root->length > path->length) return false;
    for (index = 0; index < root->length; index += 1) {
        if (!mmay_fs_path_equal_byte(
                flavor, root->data[index], path->data[index]
            )) return false;
    }
    if (root->length == path->length) return true;
    if (root->length != 0 && root->data[root->length - 1] == '/') return true;
    return path->data[root->length] == '/';
}

static MarimayFsStatus mmay_fs_normalize_owned(
    MarimayAllocator allocator,
    MarimayPathFlavor flavor,
    const char *input,
    size_t input_length,
    MarimayOwnedBytes *output,
    bool *absolute
) {
    MarimayPath normalized;
    MarimayPathStatus status;
    memset(output, 0, sizeof(*output));
    if (input == NULL || input_length > MMAY_FS_MAX_PATH_BYTES) {
        return MMAY_FS_INVALID_ARGUMENT;
    }
    status = mmay_path_normalize(
        &normalized, allocator, flavor, input, input_length
    );
    if (status != MMAY_PATH_OK) {
        return status == MMAY_PATH_ALLOCATION_FAILED
            ? MMAY_FS_ALLOCATION_FAILED : MMAY_FS_INVALID_ARGUMENT;
    }
    if (normalized.text.length > MMAY_FS_MAX_PATH_BYTES) {
        mmay_path_dispose(&normalized);
        return MMAY_FS_INVALID_ARGUMENT;
    }
    *output = normalized.text;
    *absolute = normalized.absolute;
    memset(&normalized.text, 0, sizeof(normalized.text));
    mmay_path_dispose(&normalized);
    return MMAY_FS_OK;
}

static MarimayFsStatus mmay_fs_resolve(
    MarimayFsContext *context,
    const char *input,
    size_t input_length,
    MarimayOwnedBytes *output
) {
    MarimayOwnedBytes normalized = {0};
    MarimayFsStatus status;
    bool absolute = false;
    status = mmay_fs_normalize_owned(
        context->allocator, context->flavor,
        input, input_length, &normalized, &absolute
    );
    if (status != MMAY_FS_OK || absolute) {
        *output = normalized;
        return status;
    }
    {
        size_t joined_length = 0;
        size_t allocation_size = 0;
        char *joined = NULL;
        bool separator = context->working_directory.length != 0
            && context->working_directory.data[
                context->working_directory.length - 1
            ] != '/';
        if (!mmay_size_add(
                context->working_directory.length, separator ? 1U : 0U,
                &joined_length
            ) || !mmay_size_add(joined_length, normalized.length, &joined_length)
            || joined_length > MMAY_FS_MAX_PATH_BYTES
            || !mmay_size_add(joined_length, 1U, &allocation_size)) {
            mmay_owned_bytes_dispose(&normalized);
            return MMAY_FS_INVALID_ARGUMENT;
        }
        joined = mmay_allocate(&context->allocator, allocation_size);
        if (joined == NULL) {
            mmay_owned_bytes_dispose(&normalized);
            return MMAY_FS_ALLOCATION_FAILED;
        }
        memcpy(
            joined, context->working_directory.data,
            context->working_directory.length
        );
        if (separator) joined[context->working_directory.length] = '/';
        memcpy(
            joined + context->working_directory.length + (separator ? 1U : 0U),
            normalized.data, normalized.length
        );
        joined[joined_length] = '\0';
        mmay_owned_bytes_dispose(&normalized);
        status = mmay_fs_normalize_owned(
            context->allocator, context->flavor,
            joined, joined_length, output, &absolute
        );
        mmay_deallocate(&context->allocator, joined, allocation_size);
    }
    return status == MMAY_FS_OK && absolute
        ? MMAY_FS_OK : (status == MMAY_FS_OK
            ? MMAY_FS_INVALID_ARGUMENT : status);
}

static bool mmay_fs_capability_matches(
    const MarimayFsContext *context,
    const MarimayVector *roots,
    const MarimayOwnedBytes *path
) {
    size_t index = 0;
    for (index = 0; index < roots->length; index += 1) {
        const MarimayOwnedBytes *root = mmay_vector_at_const(roots, index);
        if (mmay_fs_path_contains(context->flavor, root, path)) return true;
    }
    return false;
}

static MarimayFsStatus mmay_fs_authorize(
    MarimayFsContext *context,
    bool write,
    const char *path,
    size_t path_length,
    MarimayOwnedBytes *absolute
) {
    MarimayFsStatus status;
    if (context == NULL || !context->initialized || absolute == NULL) {
        return MMAY_FS_INVALID_ARGUMENT;
    }
    status = mmay_fs_resolve(context, path, path_length, absolute);
    if (status != MMAY_FS_OK) return status;
    if (!mmay_fs_capability_matches(
            context, write ? &context->write_roots : &context->read_roots,
            absolute
        )) {
        mmay_owned_bytes_dispose(absolute);
        return MMAY_FS_CAPABILITY_DENIED;
    }
    return MMAY_FS_OK;
}

static size_t mmay_fs_absolute_prefix_length(
    MarimayPathFlavor flavor,
    const MarimayOwnedBytes *path
) {
    size_t slash_count = 0;
    size_t index = 0;
    if (flavor != MMAY_PATH_WINDOWS) return 1;
    if (path->length >= 3 && path->data[1] == ':' && path->data[2] == '/') {
        return 3;
    }
    if (path->length >= 2 && path->data[0] == '/' && path->data[1] == '/') {
        for (index = 2; index < path->length; index += 1) {
            if (path->data[index] == '/' && ++slash_count == 2) return index;
        }
        return path->length;
    }
    return 1;
}

static MarimayFsStatus mmay_fs_check_components(
    MarimayFsContext *context,
    const MarimayOwnedBytes *absolute,
    bool allow_missing_final,
    MarimayDirectoryEntryKind *final_kind
) {
    size_t prefix = mmay_fs_absolute_prefix_length(context->flavor, absolute);
    if (final_kind != NULL) *final_kind = MMAY_DIRECTORY_ENTRY_MISSING;
    while (prefix <= absolute->length) {
        size_t end = prefix;
        MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
        MarimayFileStatus file_status;
        if (end < absolute->length) {
            while (end < absolute->length && absolute->data[end] != '/') end += 1;
        }
        if (end == 0) end = 1;
        file_status = mmay_directory_kind_with_ops(
            context->directory_ops, context->allocator,
            (const char *)absolute->data, end, &kind
        );
        if (file_status == MMAY_FILE_ALLOCATION_FAILED) {
            return MMAY_FS_ALLOCATION_FAILED;
        }
        if (file_status != MMAY_FILE_OK) return MMAY_FS_DIRECTORY_FAILED;
        if (kind == MMAY_DIRECTORY_ENTRY_SYMLINK) return MMAY_FS_SYMLINK;
        if (kind == MMAY_DIRECTORY_ENTRY_MISSING) {
            if (allow_missing_final && end == absolute->length) {
                if (final_kind != NULL) *final_kind = kind;
                return MMAY_FS_OK;
            }
            return MMAY_FS_NOT_FOUND;
        }
        if (end < absolute->length
            && kind != MMAY_DIRECTORY_ENTRY_DIRECTORY) {
            return MMAY_FS_NOT_DIRECTORY;
        }
        if (end == absolute->length) {
            if (final_kind != NULL) *final_kind = kind;
            return MMAY_FS_OK;
        }
        prefix = end + 1U;
    }
    return MMAY_FS_INTERNAL_FAILED;
}

static void mmay_fs_owned_vector_dispose(MarimayVector *roots) {
    size_t index = 0;
    for (index = 0; index < roots->length; index += 1) {
        MarimayOwnedBytes *root = mmay_vector_at(roots, index);
        mmay_owned_bytes_dispose(root);
    }
    mmay_vector_dispose(roots);
}

static MarimayFsStatus mmay_fs_copy_roots(
    MarimayFsContext *context,
    const MarimayFsPathView *views,
    size_t count,
    MarimayVector *roots
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        MarimayOwnedBytes root = {0};
        MarimayFsStatus status = mmay_fs_resolve(
            context, views[index].data, views[index].length, &root
        );
        if (status != MMAY_FS_OK) {
            mmay_owned_bytes_dispose(&root);
            return status;
        }
        if (!mmay_vector_push(roots, &root)) {
            mmay_owned_bytes_dispose(&root);
            return MMAY_FS_ALLOCATION_FAILED;
        }
    }
    return MMAY_FS_OK;
}

MarimayFsStatus mmay_fs_init_status(
    MarimayFsContext *context,
    MarimayAllocator allocator,
    const MarimayFsOptions *supplied
) {
    MarimayFsOptions defaults = mmay_fs_default_options();
    const MarimayFsOptions *options = supplied != NULL ? supplied : &defaults;
    MarimayOwnedBytes current = {0};
    MarimayFsStatus status;
    bool absolute = false;
    if (context == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || options->read_root_count > MMAY_FS_MAX_CAPABILITY_ROOTS
        || options->write_root_count > MMAY_FS_MAX_CAPABILITY_ROOTS
        || (options->read_root_count != 0 && options->read_roots == NULL)
        || (options->write_root_count != 0 && options->write_roots == NULL)
        || options->max_open_files == 0
        || options->read_ops.open == NULL || options->read_ops.read == NULL
        || options->read_ops.close == NULL
        || options->write_ops.create_exclusive == NULL
        || options->write_ops.write == NULL || options->write_ops.sync == NULL
        || options->write_ops.close == NULL || options->write_ops.replace == NULL
        || options->write_ops.remove == NULL || options->directory_ops.kind == NULL
        || options->directory_ops.current == NULL || options->directory_ops.open == NULL
        || options->directory_ops.next == NULL || options->directory_ops.close == NULL
        || options->metadata == NULL) return MMAY_FS_INVALID_ARGUMENT;
    memset(context, 0, sizeof(*context));
    context->allocator = allocator;
    context->flavor = options->flavor;
    context->read_ops = options->read_ops;
    context->write_ops = options->write_ops;
    context->directory_ops = options->directory_ops;
    context->metadata_context = options->metadata_context;
    context->metadata = options->metadata;
    context->max_open_files = options->max_open_files;
    if (!mmay_vector_init(&context->read_roots, allocator, sizeof(MarimayOwnedBytes))
        || !mmay_vector_init(&context->write_roots, allocator, sizeof(MarimayOwnedBytes))
        || !mmay_vector_init(&context->files, allocator, sizeof(MarimayFsOpenFile))) {
        mmay_fs_dispose(context);
        return MMAY_FS_ALLOCATION_FAILED;
    }
    if (options->working_directory.length == 0) {
        MarimayFileStatus current_status = mmay_directory_current_with_ops(
            options->directory_ops, allocator,
            MMAY_FS_MAX_PATH_BYTES, &current
        );
        if (current_status != MMAY_FILE_OK) {
            mmay_fs_dispose(context);
            return current_status == MMAY_FILE_ALLOCATION_FAILED
                ? MMAY_FS_ALLOCATION_FAILED : MMAY_FS_DIRECTORY_FAILED;
        }
    } else if (!mmay_owned_bytes_copy(
            &current, allocator, options->working_directory.data,
            options->working_directory.length
    )) {
        mmay_fs_dispose(context);
        return MMAY_FS_ALLOCATION_FAILED;
    }
    status = mmay_fs_normalize_owned(
        allocator, options->flavor, (const char *)current.data,
        current.length, &context->working_directory, &absolute
    );
    mmay_owned_bytes_dispose(&current);
    if (status != MMAY_FS_OK || !absolute) {
        mmay_fs_dispose(context);
        return status == MMAY_FS_OK ? MMAY_FS_INVALID_ARGUMENT : status;
    }
    context->initialized = true;
    status = mmay_fs_copy_roots(
            context, options->read_roots, options->read_root_count,
            &context->read_roots
        );
    if (status == MMAY_FS_OK) status = mmay_fs_copy_roots(
            context, options->write_roots, options->write_root_count,
            &context->write_roots
        );
    if (status != MMAY_FS_OK) {
        mmay_fs_dispose(context);
        return status;
    }
    return MMAY_FS_OK;
}

bool mmay_fs_init(
    MarimayFsContext *context,
    MarimayAllocator allocator,
    const MarimayFsOptions *options
) {
    return mmay_fs_init_status(context, allocator, options) == MMAY_FS_OK;
}

void mmay_fs_dispose(MarimayFsContext *context) {
    size_t index = 0;
    if (context == NULL) return;
    for (index = 0; index < context->files.length; index += 1) {
        MarimayFsOpenFile *file = mmay_vector_at(&context->files, index);
        if (file != NULL && file->open && context->read_ops.close != NULL) {
            (void)context->read_ops.close(
                context->read_ops.context, file->platform_handle
            );
            file->open = false;
        }
    }
    mmay_vector_dispose(&context->files);
    mmay_fs_owned_vector_dispose(&context->write_roots);
    mmay_fs_owned_vector_dispose(&context->read_roots);
    mmay_owned_bytes_dispose(&context->working_directory);
    memset(context, 0, sizeof(*context));
}

MarimayFsStatus mmay_fs_open_read(
    MarimayFsContext *context,
    const char *path,
    size_t path_length,
    int64_t *handle
) {
    MarimayOwnedBytes absolute = {0};
    MarimayDirectoryEntryKind kind;
    MarimayFileHandle platform_handle = 0;
    MarimayFsOpenFile file;
    MarimayFsStatus status;
    MarimayFileStatus file_status;
    size_t open_count = 0;
    size_t reusable = SIZE_MAX;
    size_t index = 0;
    if (handle == NULL) return MMAY_FS_INVALID_ARGUMENT;
    status = mmay_fs_authorize(context, false, path, path_length, &absolute);
    if (status != MMAY_FS_OK) return status;
    status = mmay_fs_check_components(context, &absolute, false, &kind);
    if (status == MMAY_FS_OK && kind != MMAY_DIRECTORY_ENTRY_REGULAR) {
        status = kind == MMAY_DIRECTORY_ENTRY_DIRECTORY
            ? MMAY_FS_NOT_FILE : MMAY_FS_OPEN_FAILED;
    }
    if (status == MMAY_FS_OK) {
        for (index = 0; index < context->files.length; index += 1U) {
            const MarimayFsOpenFile *existing = mmay_vector_at_const(
                &context->files, index
            );
            if (existing != NULL && existing->open) open_count += 1U;
            else if (reusable == SIZE_MAX) reusable = index;
        }
        if (open_count >= context->max_open_files) {
            status = MMAY_FS_LIMIT_EXCEEDED;
        }
    }
    if (status == MMAY_FS_OK) {
        file_status = context->read_ops.open(
            context->read_ops.context, context->allocator,
            (const char *)absolute.data, absolute.length, &platform_handle
        );
        status = file_status == MMAY_FILE_OK ? MMAY_FS_OK
            : (file_status == MMAY_FILE_ALLOCATION_FAILED
                ? MMAY_FS_ALLOCATION_FAILED : MMAY_FS_OPEN_FAILED);
    }
    if (status == MMAY_FS_OK) {
        file.platform_handle = platform_handle;
        file.open = true;
        if (reusable != SIZE_MAX) {
            MarimayFsOpenFile *slot = mmay_vector_at(
                &context->files, reusable
            );
            if (slot == NULL) {
                (void)context->read_ops.close(
                    context->read_ops.context, platform_handle
                );
                status = MMAY_FS_INTERNAL_FAILED;
            } else {
                *slot = file;
                *handle = (int64_t)reusable + 1;
            }
        } else if (!mmay_vector_push(&context->files, &file)) {
            (void)context->read_ops.close(
                context->read_ops.context, platform_handle
            );
            status = MMAY_FS_ALLOCATION_FAILED;
        } else {
            *handle = (int64_t)context->files.length;
        }
    }
    mmay_owned_bytes_dispose(&absolute);
    return status;
}

static MarimayFsOpenFile *mmay_fs_open_file(
    MarimayFsContext *context,
    int64_t handle
) {
    MarimayFsOpenFile *file = context != NULL && context->initialized
            && handle > 0 && (uint64_t)handle <= context->files.length
        ? mmay_vector_at(&context->files, (size_t)handle - 1U) : NULL;
    return file != NULL && file->open ? file : NULL;
}

MarimayFsStatus mmay_fs_read(
    MarimayFsContext *context,
    int64_t handle,
    int64_t limit,
    MarimayOwnedBytes *output
) {
    MarimayFsOpenFile *file = NULL;
    MarimayVector bytes;
    unsigned char chunk[4096];
    MarimayFsStatus status = MMAY_FS_OK;
    if (output == NULL || context == NULL || !context->initialized) {
        return MMAY_FS_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (limit < 0 || (uint64_t)limit > MMAY_FS_MAX_FILE_BYTES) {
        return MMAY_FS_INVALID_LIMIT;
    }
    file = mmay_fs_open_file(context, handle);
    if (file == NULL) return MMAY_FS_CLOSED;
    if (!mmay_vector_init(&bytes, context->allocator, sizeof(unsigned char))) {
        return MMAY_FS_ALLOCATION_FAILED;
    }
    for (;;) {
        size_t read_count = 0;
        bool end = false;
        MarimayFileStatus file_status = context->read_ops.read(
            context->read_ops.context, file->platform_handle,
            chunk, sizeof(chunk), &read_count, &end
        );
        if (file_status != MMAY_FILE_OK || read_count > sizeof(chunk)
            || (read_count == 0 && !end)) {
            status = MMAY_FS_READ_FAILED;
            break;
        }
        if (read_count > (size_t)limit - bytes.length) {
            status = MMAY_FS_LIMIT_EXCEEDED;
            break;
        }
        if (!mmay_vector_append(&bytes, chunk, read_count)) {
            status = MMAY_FS_ALLOCATION_FAILED;
            break;
        }
        if (end) break;
    }
    if (status == MMAY_FS_OK && !mmay_owned_bytes_copy(
            output, context->allocator, bytes.data, bytes.length
        )) status = MMAY_FS_ALLOCATION_FAILED;
    mmay_vector_dispose(&bytes);
    if (status != MMAY_FS_OK) mmay_owned_bytes_dispose(output);
    return status;
}

MarimayFsStatus mmay_fs_close(MarimayFsContext *context, int64_t handle) {
    MarimayFsOpenFile *file;
    MarimayFileStatus status;
    if (context == NULL || !context->initialized || handle <= 0
        || (uint64_t)handle > context->files.length) return MMAY_FS_CLOSED;
    file = mmay_vector_at(&context->files, (size_t)handle - 1U);
    if (file == NULL || !file->open) return MMAY_FS_OK;
    file->open = false;
    status = context->read_ops.close(
        context->read_ops.context, file->platform_handle
    );
    return status == MMAY_FILE_OK ? MMAY_FS_OK : MMAY_FS_CLOSE_FAILED;
}

void mmay_fs_close_best_effort(MarimayFsContext *context, int64_t handle) {
    (void)mmay_fs_close(context, handle);
}

MarimayFsStatus mmay_fs_read_all(
    MarimayFsContext *context,
    const char *path,
    size_t path_length,
    int64_t limit,
    MarimayOwnedBytes *output
) {
    int64_t handle = 0;
    MarimayFsStatus status;
    if (output == NULL) return MMAY_FS_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (limit < 0 || (uint64_t)limit > MMAY_FS_MAX_FILE_BYTES) {
        return MMAY_FS_INVALID_LIMIT;
    }
    status = mmay_fs_open_read(context, path, path_length, &handle);
    if (status != MMAY_FS_OK) return status;
    status = mmay_fs_read(context, handle, limit, output);
    if (status != MMAY_FS_OK) {
        mmay_fs_close_best_effort(context, handle);
        return status;
    }
    status = mmay_fs_close(context, handle);
    if (status != MMAY_FS_OK) mmay_owned_bytes_dispose(output);
    return status;
}

static MarimayFsStatus mmay_fs_file_phase_status(
    MarimayFileStatus status,
    MarimayFsStatus failure
) {
    if (status == MMAY_FILE_OK) return MMAY_FS_OK;
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_FS_ALLOCATION_FAILED;
    }
    return failure;
}

MarimayFsStatus mmay_fs_write_atomic(
    MarimayFsContext *context,
    const char *path,
    size_t path_length,
    const unsigned char *bytes,
    size_t length
) {
    MarimayOwnedBytes absolute = {0};
    MarimayDirectoryEntryKind kind;
    MarimayFsStatus status;
    size_t temporary_capacity = 0;
    char *temporary = NULL;
    size_t temporary_length = 0;
    MarimayFileHandle file = 0;
    bool created = false;
    bool open = false;
    size_t attempt = 0;
    size_t offset = 0;
    if (context == NULL || !context->initialized
        || (bytes == NULL && length != 0) || length > MMAY_FS_MAX_FILE_BYTES) {
        return length > MMAY_FS_MAX_FILE_BYTES
            ? MMAY_FS_LIMIT_EXCEEDED : MMAY_FS_INVALID_ARGUMENT;
    }
    status = mmay_fs_authorize(context, true, path, path_length, &absolute);
    if (status != MMAY_FS_OK) return status;
    status = mmay_fs_check_components(context, &absolute, true, &kind);
    if (status == MMAY_FS_OK && kind == MMAY_DIRECTORY_ENTRY_DIRECTORY) {
        status = MMAY_FS_NOT_FILE;
    } else if (status == MMAY_FS_OK
        && kind != MMAY_DIRECTORY_ENTRY_MISSING
        && kind != MMAY_DIRECTORY_ENTRY_REGULAR) {
        status = MMAY_FS_WRITE_FAILED;
    }
    if (status != MMAY_FS_OK) goto cleanup;
    if (!mmay_size_add(absolute.length, 40U, &temporary_capacity)) {
        status = MMAY_FS_INTERNAL_FAILED;
        goto cleanup;
    }
    temporary = mmay_allocate(&context->allocator, temporary_capacity);
    if (temporary == NULL) {
        status = MMAY_FS_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (attempt = 0; attempt < 64U; attempt += 1U) {
        int written = snprintf(
            temporary, temporary_capacity, "%.*s.mmay-tmp-%016llx",
            (int)absolute.length, (const char *)absolute.data,
            (unsigned long long)context->temporary_counter++
        );
        if (written < 0 || (size_t)written >= temporary_capacity) {
            status = MMAY_FS_INTERNAL_FAILED;
            goto cleanup;
        }
        temporary_length = (size_t)written;
        {
            MarimayFileStatus file_status = context->write_ops.create_exclusive(
                context->write_ops.context, context->allocator,
                temporary, temporary_length, &file
            );
            if (file_status == MMAY_FILE_ALREADY_EXISTS) continue;
            status = mmay_fs_file_phase_status(
                file_status, MMAY_FS_WRITE_FAILED
            );
        }
        break;
    }
    if (status != MMAY_FS_OK || attempt == 64U) {
        if (attempt == 64U) status = MMAY_FS_WRITE_FAILED;
        goto cleanup;
    }
    created = true;
    open = true;
    while (offset < length) {
        size_t written = 0;
        MarimayFileStatus file_status = context->write_ops.write(
            context->write_ops.context, file, bytes + offset,
            length - offset, &written
        );
        if (file_status != MMAY_FILE_OK || written == 0
            || written > length - offset) {
            status = mmay_fs_file_phase_status(
                file_status, MMAY_FS_WRITE_FAILED
            );
            if (status == MMAY_FS_OK) status = MMAY_FS_WRITE_FAILED;
            goto cleanup;
        }
        offset += written;
    }
    status = mmay_fs_file_phase_status(
        context->write_ops.sync(context->write_ops.context, file),
        MMAY_FS_SYNC_FAILED
    );
    if (status != MMAY_FS_OK) goto cleanup;
    status = mmay_fs_file_phase_status(
        context->write_ops.close(context->write_ops.context, file),
        MMAY_FS_CLOSE_FAILED
    );
    open = false;
    if (status != MMAY_FS_OK) goto cleanup;
    status = mmay_fs_file_phase_status(
        context->write_ops.replace(
            context->write_ops.context, context->allocator,
            temporary, temporary_length,
            (const char *)absolute.data, absolute.length
        ),
        MMAY_FS_REPLACE_FAILED
    );
    if (status == MMAY_FS_OK) created = false;

cleanup:
    if (open) (void)context->write_ops.close(context->write_ops.context, file);
    if (created) context->write_ops.remove(
        context->write_ops.context, context->allocator,
        temporary, temporary_length
    );
    mmay_deallocate(&context->allocator, temporary, temporary_capacity);
    mmay_owned_bytes_dispose(&absolute);
    return status;
}

MarimayFsStatus mmay_fs_metadata(
    MarimayFsContext *context,
    const char *path,
    size_t path_length,
    MarimayFsMetadata *metadata
) {
    MarimayOwnedBytes absolute = {0};
    MarimayDirectoryEntryKind kind;
    MarimayFileMetadata platform;
    MarimayFsStatus status;
    MarimayFileStatus file_status;
    if (metadata == NULL) return MMAY_FS_INVALID_ARGUMENT;
    memset(metadata, 0, sizeof(*metadata));
    status = mmay_fs_authorize(context, false, path, path_length, &absolute);
    if (status != MMAY_FS_OK) return status;
    status = mmay_fs_check_components(context, &absolute, false, &kind);
    if (status != MMAY_FS_OK) goto cleanup;
    if (kind != MMAY_DIRECTORY_ENTRY_REGULAR
        && kind != MMAY_DIRECTORY_ENTRY_DIRECTORY) {
        status = MMAY_FS_NOT_FILE;
        goto cleanup;
    }
    file_status = context->metadata(
        context->metadata_context, context->allocator,
        (const char *)absolute.data, absolute.length, &platform
    );
    status = mmay_fs_file_phase_status(
        file_status, MMAY_FS_DIRECTORY_FAILED
    );
    if (status == MMAY_FS_OK) {
        if (platform.symlink) status = MMAY_FS_SYMLINK;
        else if (!platform.regular && !platform.directory) status = MMAY_FS_NOT_FILE;
        else {
            metadata->directory = platform.directory;
            metadata->size = platform.directory ? 0 : platform.size;
        }
    }
cleanup:
    mmay_owned_bytes_dispose(&absolute);
    return status;
}

static int mmay_fs_entry_compare(const void *left, const void *right) {
    const MarimayFsEntry *a = left;
    const MarimayFsEntry *b = right;
    size_t common = a->name.length < b->name.length
        ? a->name.length : b->name.length;
    int order = common == 0 ? 0 : memcmp(a->name.data, b->name.data, common);
    if (order != 0) return order;
    return a->name.length < b->name.length ? -1
        : (a->name.length > b->name.length ? 1 : 0);
}

void mmay_fs_entries_dispose(MarimayVector *entries) {
    size_t index = 0;
    if (entries == NULL) return;
    for (index = 0; index < entries->length; index += 1) {
        MarimayFsEntry *entry = mmay_vector_at(entries, index);
        if (entry != NULL) mmay_owned_bytes_dispose(&entry->name);
    }
    mmay_vector_dispose(entries);
}

MarimayFsStatus mmay_fs_read_directory(
    MarimayFsContext *context,
    const char *path,
    size_t path_length,
    int64_t limit,
    MarimayVector *entries
) {
    MarimayOwnedBytes absolute = {0};
    MarimayDirectoryEntryKind kind;
    MarimayDirectoryIterator iterator;
    MarimayFsStatus status;
    MarimayFileStatus file_status;
    bool iterator_open = false;
    if (entries == NULL || context == NULL || !context->initialized) {
        return MMAY_FS_INVALID_ARGUMENT;
    }
    memset(entries, 0, sizeof(*entries));
    if (limit < 0 || (uint64_t)limit > MMAY_FS_MAX_DIRECTORY_ENTRIES) {
        return MMAY_FS_INVALID_LIMIT;
    }
    if (!mmay_vector_init(entries, context->allocator, sizeof(MarimayFsEntry))) {
        return MMAY_FS_ALLOCATION_FAILED;
    }
    status = mmay_fs_authorize(context, false, path, path_length, &absolute);
    if (status != MMAY_FS_OK) goto cleanup;
    status = mmay_fs_check_components(context, &absolute, false, &kind);
    if (status != MMAY_FS_OK) goto cleanup;
    if (kind != MMAY_DIRECTORY_ENTRY_DIRECTORY) {
        status = MMAY_FS_NOT_DIRECTORY;
        goto cleanup;
    }
    file_status = mmay_directory_open_with_ops(
        context->directory_ops, context->allocator,
        (const char *)absolute.data, absolute.length, &iterator
    );
    status = mmay_fs_file_phase_status(
        file_status, MMAY_FS_DIRECTORY_FAILED
    );
    if (status != MMAY_FS_OK) goto cleanup;
    iterator_open = true;
    for (;;) {
        MarimayFsEntry entry;
        bool end = false;
        memset(&entry, 0, sizeof(entry));
        file_status = mmay_directory_next(
            &iterator, context->allocator, MMAY_FS_MAX_PATH_BYTES,
            &entry.name, &entry.kind, &end
        );
        if (file_status != MMAY_FILE_OK) {
            mmay_owned_bytes_dispose(&entry.name);
            status = file_status == MMAY_FILE_ALLOCATION_FAILED
                ? MMAY_FS_ALLOCATION_FAILED
                : (file_status == MMAY_FILE_INVALID_UTF8
                    ? MMAY_FS_INVALID_UTF8 : MMAY_FS_DIRECTORY_FAILED);
            break;
        }
        if (end) break;
        if (entries->length >= (size_t)limit) {
            mmay_owned_bytes_dispose(&entry.name);
            status = MMAY_FS_TOO_MANY_ENTRIES;
            break;
        }
        if (!mmay_vector_push(entries, &entry)) {
            mmay_owned_bytes_dispose(&entry.name);
            status = MMAY_FS_ALLOCATION_FAILED;
            break;
        }
    }
    file_status = mmay_directory_close(&iterator);
    iterator_open = false;
    if (status == MMAY_FS_OK && file_status != MMAY_FILE_OK) {
        status = MMAY_FS_CLOSE_FAILED;
    }
    if (status == MMAY_FS_OK && entries->length > 1) {
        qsort(
            entries->data, entries->length,
            sizeof(MarimayFsEntry), mmay_fs_entry_compare
        );
    }
cleanup:
    if (iterator_open) (void)mmay_directory_close(&iterator);
    mmay_owned_bytes_dispose(&absolute);
    if (status != MMAY_FS_OK) mmay_fs_entries_dispose(entries);
    return status;
}
