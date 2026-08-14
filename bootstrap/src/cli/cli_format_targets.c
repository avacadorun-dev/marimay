#include "cli_internal.h"

#include <stdlib.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

#include "ruda/platform/directory.h"

static RudaCliFormatTargetStatus ruda_cli_format_target_file_status(
    RudaFileStatus status
) {
    if (status == RUDA_FILE_OK) {
        return RUDA_CLI_FORMAT_TARGET_OK;
    }
    if (status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    if (status == RUDA_FILE_LIMIT_EXCEEDED) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    return RUDA_CLI_FORMAT_TARGET_INPUT_ERROR;
}

static bool ruda_cli_path_equal(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    return left->length == right->length
        && memcmp(left->data, right->data, left->length) == 0;
}

static bool ruda_cli_path_copy(
    RudaOwnedBytes *output,
    RudaAllocator allocator,
    const char *path,
    size_t length
) {
    if (length > RUDA_CLI_FORMAT_MAX_PATH_BYTES) {
        return false;
    }
    return ruda_owned_bytes_copy(output, allocator, path, length);
}

static RudaCliFormatTargetStatus ruda_cli_path_join(
    RudaAllocator allocator,
    const RudaOwnedBytes *parent,
    const char *name,
    size_t name_length,
    RudaOwnedBytes *output
) {
    bool separator = parent->length != 0
        && parent->data[parent->length - 1] != '/'
        && parent->data[parent->length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!ruda_size_add(parent->length, separator ? 1 : 0, &length)
        || !ruda_size_add(length, name_length, &length)
        || length > RUDA_CLI_FORMAT_MAX_PATH_BYTES
        || !ruda_size_add(length, 1, &allocation_size)) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memcpy(bytes, parent->data, parent->length);
    if (separator) {
        bytes[parent->length] = '/';
    }
    memcpy(
        bytes + parent->length + (separator ? 1 : 0),
        name,
        name_length
    );
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return RUDA_CLI_FORMAT_TARGET_OK;
}

static bool ruda_cli_path_is_root(const RudaOwnedBytes *path) {
    const unsigned char *bytes = path->data;
    size_t first_share_separator = SIZE_MAX;
    size_t index = 0;
    if (path->length == 1 && bytes[0] == '/') {
        return true;
    }
    if (path->length == 3
        && ((bytes[0] >= 'A' && bytes[0] <= 'Z')
            || (bytes[0] >= 'a' && bytes[0] <= 'z'))
        && bytes[1] == ':' && bytes[2] == '/') {
        return true;
    }
    if (path->length < 5 || bytes[0] != '/' || bytes[1] != '/') {
        return false;
    }
    for (index = 2; index < path->length; index += 1) {
        if (bytes[index] == '/') {
            first_share_separator = index;
            break;
        }
    }
    return first_share_separator != SIZE_MAX
        && memchr(
            bytes + first_share_separator + 1,
            '/',
            path->length - first_share_separator - 1
        ) == NULL;
}

static bool ruda_cli_path_parent(RudaOwnedBytes *path) {
    size_t length = path->length;
    size_t separator = SIZE_MAX;
    if (ruda_cli_path_is_root(path)) {
        return false;
    }
    while (length > 1 && path->data[length - 1] == '/') {
        length -= 1;
    }
    while (length != 0) {
        if (path->data[length - 1] == '/') {
            separator = length - 1;
            break;
        }
        length -= 1;
    }
    if (separator == SIZE_MAX) {
        return false;
    }
    if (separator == 0) {
        path->length = 1;
    } else if (separator == 2 && path->data[1] == ':') {
        path->length = 3;
    } else {
        path->length = separator;
    }
    path->data[path->length] = 0;
    return true;
}

static RudaCliFormatTargetStatus ruda_cli_format_target_add_owned(
    RudaVector *targets,
    RudaOwnedBytes *path
) {
    RudaCliFormatPath target;
    if (targets->length >= RUDA_CLI_FORMAT_MAX_FILES) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    memset(&target, 0, sizeof(target));
    target.path = *path;
    if (!ruda_vector_push(targets, &target)) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memset(path, 0, sizeof(*path));
    return RUDA_CLI_FORMAT_TARGET_OK;
}

static RudaCliFormatTargetStatus ruda_cli_format_target_add_copy(
    RudaVector *targets,
    RudaAllocator allocator,
    const char *path,
    size_t length
) {
    RudaOwnedBytes owned = {0};
    RudaCliFormatTargetStatus status;
    if (length > RUDA_CLI_FORMAT_MAX_PATH_BYTES) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    if (!ruda_owned_bytes_copy(&owned, allocator, path, length)) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    status = ruda_cli_format_target_add_owned(targets, &owned);
    ruda_owned_bytes_dispose(&owned);
    return status;
}

static void ruda_cli_format_targets_dispose(RudaVector *targets) {
    size_t index = 0;
    for (index = 0; index < targets->length; index += 1) {
        RudaCliFormatPath *target = ruda_vector_at(targets, index);
        ruda_owned_bytes_dispose(&target->path);
    }
    ruda_vector_dispose(targets);
}

static int ruda_cli_format_path_compare(const void *left, const void *right) {
    const RudaCliFormatPath *first = left;
    const RudaCliFormatPath *second = right;
    size_t common = first->path.length < second->path.length
        ? first->path.length
        : second->path.length;
    int comparison = common == 0
        ? 0
        : memcmp(first->path.data, second->path.data, common);
    if (comparison != 0) {
        return comparison;
    }
    return first->path.length < second->path.length
        ? -1
        : (first->path.length > second->path.length ? 1 : 0);
}

static void ruda_cli_format_targets_sort_unique(RudaVector *targets) {
    size_t read_index = 0;
    size_t write_index = 0;
    if (targets->length > 1) {
        qsort(
            targets->data,
            targets->length,
            sizeof(RudaCliFormatPath),
            ruda_cli_format_path_compare
        );
    }
    for (read_index = 0; read_index < targets->length; read_index += 1) {
        RudaCliFormatPath *current = ruda_vector_at(targets, read_index);
        RudaCliFormatPath *previous = write_index == 0
            ? NULL
            : ruda_vector_at(targets, write_index - 1);
        if (previous != NULL && ruda_cli_path_equal(
                &previous->path,
                &current->path
            )) {
            ruda_owned_bytes_dispose(&current->path);
            continue;
        }
        if (write_index != read_index) {
            RudaCliFormatPath *destination = ruda_vector_at(targets, write_index);
            *destination = *current;
            memset(current, 0, sizeof(*current));
        }
        write_index += 1;
    }
    targets->length = write_index;
}

static bool ruda_cli_format_ignored_directory(const RudaOwnedBytes *name) {
    return (name->length == 4 && memcmp(name->data, ".git", 4) == 0)
        || (name->length == 5 && memcmp(name->data, ".ruda", 5) == 0);
}

static RudaCliFormatTargetStatus ruda_cli_collect_format_tree(
    RudaAllocator allocator,
    const RudaOwnedBytes *root,
    RudaVector *targets
) {
    RudaVector directories;
    RudaCliFormatDirectory initial;
    RudaCliFormatTargetStatus status = RUDA_CLI_FORMAT_TARGET_OK;
    size_t directory_count = 0;
    size_t entry_count = 0;
    if (!ruda_vector_init(
            &directories,
            allocator,
            sizeof(RudaCliFormatDirectory)
        )) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memset(&initial, 0, sizeof(initial));
    if (!ruda_cli_path_copy(
            &initial.path,
            allocator,
            (const char *)root->data,
            root->length
        ) || !ruda_vector_push(&directories, &initial)) {
        ruda_owned_bytes_dispose(&initial.path);
        ruda_vector_dispose(&directories);
        return root->length > RUDA_CLI_FORMAT_MAX_PATH_BYTES
            ? RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
            : RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    while (directories.length != 0
        && status == RUDA_CLI_FORMAT_TARGET_OK) {
        RudaCliFormatDirectory directory = *(
            RudaCliFormatDirectory *
        )ruda_vector_at(&directories, directories.length - 1);
        RudaDirectoryIterator iterator;
        RudaFileStatus file_status;
        bool root_directory = ruda_cli_path_equal(&directory.path, root);
        directories.length -= 1;
        directory_count += 1;
        if (directory_count > RUDA_CLI_FORMAT_MAX_DIRECTORIES) {
            ruda_owned_bytes_dispose(&directory.path);
            status = RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
            break;
        }
        file_status = ruda_directory_open(
            allocator,
            (const char *)directory.path.data,
            directory.path.length,
            &iterator
        );
        status = ruda_cli_format_target_file_status(file_status);
        if (status != RUDA_CLI_FORMAT_TARGET_OK) {
            ruda_owned_bytes_dispose(&directory.path);
            break;
        }
        for (;;) {
            RudaOwnedBytes name = {0};
            RudaOwnedBytes child = {0};
            RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
            bool end = false;
            file_status = ruda_directory_next(
                &iterator,
                allocator,
                RUDA_CLI_FORMAT_MAX_NAME_BYTES,
                &name,
                &kind,
                &end
            );
            status = ruda_cli_format_target_file_status(file_status);
            if (status != RUDA_CLI_FORMAT_TARGET_OK || end) {
                ruda_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (entry_count > RUDA_CLI_FORMAT_MAX_ENTRIES) {
                ruda_owned_bytes_dispose(&name);
                status = RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
                break;
            }
            status = ruda_cli_path_join(
                allocator,
                &directory.path,
                (const char *)name.data,
                name.length,
                &child
            );
            if (status != RUDA_CLI_FORMAT_TARGET_OK) {
                ruda_owned_bytes_dispose(&name);
                break;
            }
            if (kind == RUDA_DIRECTORY_ENTRY_REGULAR
                && (ruda_cli_source_path((const char *)child.data)
                    || (root_directory
                        && ruda_cli_manifest_kind(
                            (const char *)child.data,
                            &(RudaManifestKind){0}
                        )))) {
                status = ruda_cli_format_target_add_owned(targets, &child);
            } else if (kind == RUDA_DIRECTORY_ENTRY_DIRECTORY
                && !ruda_cli_format_ignored_directory(&name)) {
                RudaOwnedBytes manifest = {0};
                RudaDirectoryEntryKind manifest_kind = RUDA_DIRECTORY_ENTRY_MISSING;
                status = ruda_cli_path_join(
                    allocator,
                    &child,
                    "ruda.mod",
                    strlen("ruda.mod"),
                    &manifest
                );
                if (status == RUDA_CLI_FORMAT_TARGET_OK) {
                    file_status = ruda_directory_kind(
                        allocator,
                        (const char *)manifest.data,
                        manifest.length,
                        &manifest_kind
                    );
                    status = ruda_cli_format_target_file_status(file_status);
                }
                ruda_owned_bytes_dispose(&manifest);
                if (status == RUDA_CLI_FORMAT_TARGET_OK
                    && manifest_kind != RUDA_DIRECTORY_ENTRY_REGULAR) {
                    RudaCliFormatDirectory nested;
                    memset(&nested, 0, sizeof(nested));
                    if (directory.depth >= RUDA_CLI_FORMAT_MAX_DEPTH
                        || directories.length
                            >= RUDA_CLI_FORMAT_MAX_DIRECTORIES) {
                        status = RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
                    } else {
                        nested.path = child;
                        nested.depth = directory.depth + 1;
                        if (!ruda_vector_push(&directories, &nested)) {
                            status = RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
                        } else {
                            memset(&child, 0, sizeof(child));
                        }
                    }
                }
            }
            ruda_owned_bytes_dispose(&child);
            ruda_owned_bytes_dispose(&name);
            if (status != RUDA_CLI_FORMAT_TARGET_OK) {
                break;
            }
        }
        file_status = ruda_directory_close(&iterator);
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            status = ruda_cli_format_target_file_status(file_status);
        }
        ruda_owned_bytes_dispose(&directory.path);
    }
    while (directories.length != 0) {
        RudaCliFormatDirectory *directory = ruda_vector_at(
            &directories,
            directories.length - 1
        );
        ruda_owned_bytes_dispose(&directory->path);
        directories.length -= 1;
    }
    ruda_vector_dispose(&directories);
    return status;
}

static RudaCliFormatTargetStatus ruda_cli_discover_format_roots(
    RudaAllocator allocator,
    RudaOwnedBytes *module_root,
    RudaOwnedBytes *workspace_manifest
) {
    RudaOwnedBytes cursor = {0};
    RudaCliFormatTargetStatus status;
    RudaFileStatus file_status = ruda_directory_current(
        allocator,
        RUDA_CLI_FORMAT_MAX_PATH_BYTES,
        &cursor
    );
    memset(module_root, 0, sizeof(*module_root));
    memset(workspace_manifest, 0, sizeof(*workspace_manifest));
    status = ruda_cli_format_target_file_status(file_status);
    while (status == RUDA_CLI_FORMAT_TARGET_OK) {
        RudaOwnedBytes candidate = {0};
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        status = ruda_cli_path_join(
            allocator,
            &cursor,
            "ruda.mod",
            strlen("ruda.mod"),
            &candidate
        );
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            file_status = ruda_directory_kind(
                allocator,
                (const char *)candidate.data,
                candidate.length,
                &kind
            );
            status = ruda_cli_format_target_file_status(file_status);
        }
        ruda_owned_bytes_dispose(&candidate);
        if (status != RUDA_CLI_FORMAT_TARGET_OK) {
            break;
        }
        if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
            *module_root = cursor;
            memset(&cursor, 0, sizeof(cursor));
            break;
        }
        if (!ruda_cli_path_parent(&cursor)) {
            status = RUDA_CLI_FORMAT_TARGET_INPUT_ERROR;
            break;
        }
    }
    ruda_owned_bytes_dispose(&cursor);
    if (status != RUDA_CLI_FORMAT_TARGET_OK) {
        return status;
    }
    if (!ruda_owned_bytes_copy(
            &cursor,
            allocator,
            module_root->data,
            module_root->length
        )) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    for (;;) {
        RudaOwnedBytes candidate = {0};
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        status = ruda_cli_path_join(
            allocator,
            &cursor,
            "ruda.work",
            strlen("ruda.work"),
            &candidate
        );
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            file_status = ruda_directory_kind(
                allocator,
                (const char *)candidate.data,
                candidate.length,
                &kind
            );
            status = ruda_cli_format_target_file_status(file_status);
        }
        if (status != RUDA_CLI_FORMAT_TARGET_OK) {
            ruda_owned_bytes_dispose(&candidate);
            break;
        }
        if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
            *workspace_manifest = candidate;
            memset(&candidate, 0, sizeof(candidate));
            ruda_owned_bytes_dispose(&candidate);
            break;
        }
        ruda_owned_bytes_dispose(&candidate);
        if (!ruda_cli_path_parent(&cursor)) {
            break;
        }
    }
    ruda_owned_bytes_dispose(&cursor);
    return status;
}

int ruda_cli_format_target_status_error(
    const RudaCliOptions *options,
    RudaCliFormatTargetStatus status,
    RudaWriter standard_error
) {
    if (status == RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED) {
        return RUDA_EXIT_INTERNAL;
    }
    return ruda_cli_plain_error(
        standard_error,
        options->diagnostic_style,
        status == RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
            ? RUDA_DIAGNOSTIC_FORMAT_LIMIT
            : RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
        status == RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
            ? "formatter target discovery exhausted a defensive path or tree limit"
            : "cannot discover or traverse the selected formatter target",
        RUDA_EXIT_SOURCE_FAILURE
    );
}

static int ruda_cli_format_selected_files(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaVector *targets,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    size_t index = 0;
    int aggregate = RUDA_EXIT_SUCCESS;
    for (index = 0; index < targets->length; index += 1) {
        const RudaCliFormatPath *target = ruda_vector_at_const(targets, index);
        RudaCliOptions file_options = *options;
        int result;
        file_options.target = (const char *)target->path.data;
        result = ruda_cli_format_file(
            session,
            &file_options,
            standard_output,
            standard_error
        );
        if (result == RUDA_EXIT_INTERNAL) {
            return result;
        }
        if (result != RUDA_EXIT_SUCCESS) {
            aggregate = result;
        }
    }
    return aggregate;
}

int ruda_cli_format_target(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaVector targets;
    RudaCliFormatTargetStatus status = RUDA_CLI_FORMAT_TARGET_OK;
    RudaOwnedBytes module_root = {0};
    RudaOwnedBytes workspace_manifest = {0};
    if (!ruda_vector_init(
            &targets,
            session->allocator,
            sizeof(RudaCliFormatPath)
        )) {
        return RUDA_EXIT_INTERNAL;
    }
    if (options->target == NULL) {
        status = ruda_cli_discover_format_roots(
            session->allocator,
            &module_root,
            &workspace_manifest
        );
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            status = ruda_cli_collect_format_tree(
                session->allocator,
                &module_root,
                &targets
            );
        }
        if (status == RUDA_CLI_FORMAT_TARGET_OK
            && workspace_manifest.data != NULL) {
            status = ruda_cli_format_target_add_owned(
                &targets,
                &workspace_manifest
            );
        }
    } else {
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        RudaFileStatus file_status = ruda_directory_kind(
            session->allocator,
            options->target,
            strlen(options->target),
            &kind
        );
        status = ruda_cli_format_target_file_status(file_status);
        if (status == RUDA_CLI_FORMAT_TARGET_OK
            && kind == RUDA_DIRECTORY_ENTRY_REGULAR
            && ruda_cli_format_file_path(options->target)) {
            status = ruda_cli_format_target_add_copy(
                &targets,
                session->allocator,
                options->target,
                strlen(options->target)
            );
        } else if (status == RUDA_CLI_FORMAT_TARGET_OK
            && kind == RUDA_DIRECTORY_ENTRY_DIRECTORY) {
            if (!ruda_cli_path_copy(
                    &module_root,
                    session->allocator,
                    options->target,
                    strlen(options->target)
                )) {
                status = strlen(options->target)
                        > RUDA_CLI_FORMAT_MAX_PATH_BYTES
                    ? RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
                    : RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
            } else {
                status = ruda_cli_collect_format_tree(
                    session->allocator,
                    &module_root,
                    &targets
                );
            }
        } else if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            status = RUDA_CLI_FORMAT_TARGET_INPUT_ERROR;
        }
    }
    ruda_owned_bytes_dispose(&workspace_manifest);
    ruda_owned_bytes_dispose(&module_root);
    if (status != RUDA_CLI_FORMAT_TARGET_OK) {
        int result = ruda_cli_format_target_status_error(
            options,
            status,
            standard_error
        );
        ruda_cli_format_targets_dispose(&targets);
        return result;
    }
    ruda_cli_format_targets_sort_unique(&targets);
    {
        int result = ruda_cli_format_selected_files(
            session,
            options,
            &targets,
            standard_output,
            standard_error
        );
        ruda_cli_format_targets_dispose(&targets);
        return result;
    }
}
