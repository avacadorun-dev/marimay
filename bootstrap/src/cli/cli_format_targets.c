#include "cli_internal.h"

#include <stdlib.h>
#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

#include "mmay/platform/directory.h"

static MarimayCliFormatTargetStatus mmay_cli_format_target_file_status(
    MarimayFileStatus status
) {
    if (status == MMAY_FILE_OK) {
        return MMAY_CLI_FORMAT_TARGET_OK;
    }
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    if (status == MMAY_FILE_LIMIT_EXCEEDED) {
        return MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    return MMAY_CLI_FORMAT_TARGET_INPUT_ERROR;
}

static bool mmay_cli_path_equal(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
) {
    return left->length == right->length
        && memcmp(left->data, right->data, left->length) == 0;
}

static bool mmay_cli_path_copy(
    MarimayOwnedBytes *output,
    MarimayAllocator allocator,
    const char *path,
    size_t length
) {
    if (length > MMAY_CLI_FORMAT_MAX_PATH_BYTES) {
        return false;
    }
    return mmay_owned_bytes_copy(output, allocator, path, length);
}

static MarimayCliFormatTargetStatus mmay_cli_path_join(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *parent,
    const char *name,
    size_t name_length,
    MarimayOwnedBytes *output
) {
    bool separator = parent->length != 0
        && parent->data[parent->length - 1] != '/'
        && parent->data[parent->length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!mmay_size_add(parent->length, separator ? 1 : 0, &length)
        || !mmay_size_add(length, name_length, &length)
        || length > MMAY_CLI_FORMAT_MAX_PATH_BYTES
        || !mmay_size_add(length, 1, &allocation_size)) {
        return MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
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
    return MMAY_CLI_FORMAT_TARGET_OK;
}

static bool mmay_cli_path_is_root(const MarimayOwnedBytes *path) {
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

static bool mmay_cli_path_parent(MarimayOwnedBytes *path) {
    size_t length = path->length;
    size_t separator = SIZE_MAX;
    if (mmay_cli_path_is_root(path)) {
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

static MarimayCliFormatTargetStatus mmay_cli_format_target_add_owned(
    MarimayVector *targets,
    MarimayOwnedBytes *path
) {
    MarimayCliFormatPath target;
    if (targets->length >= MMAY_CLI_FORMAT_MAX_FILES) {
        return MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    memset(&target, 0, sizeof(target));
    target.path = *path;
    if (!mmay_vector_push(targets, &target)) {
        return MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memset(path, 0, sizeof(*path));
    return MMAY_CLI_FORMAT_TARGET_OK;
}

static MarimayCliFormatTargetStatus mmay_cli_format_target_add_copy(
    MarimayVector *targets,
    MarimayAllocator allocator,
    const char *path,
    size_t length
) {
    MarimayOwnedBytes owned = {0};
    MarimayCliFormatTargetStatus status;
    if (length > MMAY_CLI_FORMAT_MAX_PATH_BYTES) {
        return MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    if (!mmay_owned_bytes_copy(&owned, allocator, path, length)) {
        return MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    status = mmay_cli_format_target_add_owned(targets, &owned);
    mmay_owned_bytes_dispose(&owned);
    return status;
}

static void mmay_cli_format_targets_dispose(MarimayVector *targets) {
    size_t index = 0;
    for (index = 0; index < targets->length; index += 1) {
        MarimayCliFormatPath *target = mmay_vector_at(targets, index);
        mmay_owned_bytes_dispose(&target->path);
    }
    mmay_vector_dispose(targets);
}

static int mmay_cli_format_path_compare(const void *left, const void *right) {
    const MarimayCliFormatPath *first = left;
    const MarimayCliFormatPath *second = right;
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

static void mmay_cli_format_targets_sort_unique(MarimayVector *targets) {
    size_t read_index = 0;
    size_t write_index = 0;
    if (targets->length > 1) {
        qsort(
            targets->data,
            targets->length,
            sizeof(MarimayCliFormatPath),
            mmay_cli_format_path_compare
        );
    }
    for (read_index = 0; read_index < targets->length; read_index += 1) {
        MarimayCliFormatPath *current = mmay_vector_at(targets, read_index);
        MarimayCliFormatPath *previous = write_index == 0
            ? NULL
            : mmay_vector_at(targets, write_index - 1);
        if (previous != NULL && mmay_cli_path_equal(
                &previous->path,
                &current->path
            )) {
            mmay_owned_bytes_dispose(&current->path);
            continue;
        }
        if (write_index != read_index) {
            MarimayCliFormatPath *destination = mmay_vector_at(targets, write_index);
            *destination = *current;
            memset(current, 0, sizeof(*current));
        }
        write_index += 1;
    }
    targets->length = write_index;
}

static bool mmay_cli_format_ignored_directory(const MarimayOwnedBytes *name) {
    return (name->length == 4 && memcmp(name->data, ".git", 4) == 0)
        || (name->length == 5 && memcmp(name->data, ".mmay", 5) == 0);
}

static MarimayCliFormatTargetStatus mmay_cli_collect_format_tree(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *root,
    MarimayVector *targets
) {
    MarimayVector directories;
    MarimayCliFormatDirectory initial;
    MarimayCliFormatTargetStatus status = MMAY_CLI_FORMAT_TARGET_OK;
    size_t directory_count = 0;
    size_t entry_count = 0;
    if (!mmay_vector_init(
            &directories,
            allocator,
            sizeof(MarimayCliFormatDirectory)
        )) {
        return MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memset(&initial, 0, sizeof(initial));
    if (!mmay_cli_path_copy(
            &initial.path,
            allocator,
            (const char *)root->data,
            root->length
        ) || !mmay_vector_push(&directories, &initial)) {
        mmay_owned_bytes_dispose(&initial.path);
        mmay_vector_dispose(&directories);
        return root->length > MMAY_CLI_FORMAT_MAX_PATH_BYTES
            ? MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED
            : MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    while (directories.length != 0
        && status == MMAY_CLI_FORMAT_TARGET_OK) {
        MarimayCliFormatDirectory directory = *(
            MarimayCliFormatDirectory *
        )mmay_vector_at(&directories, directories.length - 1);
        MarimayDirectoryIterator iterator;
        MarimayFileStatus file_status;
        bool root_directory = mmay_cli_path_equal(&directory.path, root);
        directories.length -= 1;
        directory_count += 1;
        if (directory_count > MMAY_CLI_FORMAT_MAX_DIRECTORIES) {
            mmay_owned_bytes_dispose(&directory.path);
            status = MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
            break;
        }
        file_status = mmay_directory_open(
            allocator,
            (const char *)directory.path.data,
            directory.path.length,
            &iterator
        );
        status = mmay_cli_format_target_file_status(file_status);
        if (status != MMAY_CLI_FORMAT_TARGET_OK) {
            mmay_owned_bytes_dispose(&directory.path);
            break;
        }
        for (;;) {
            MarimayOwnedBytes name = {0};
            MarimayOwnedBytes child = {0};
            MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
            bool end = false;
            file_status = mmay_directory_next(
                &iterator,
                allocator,
                MMAY_CLI_FORMAT_MAX_NAME_BYTES,
                &name,
                &kind,
                &end
            );
            status = mmay_cli_format_target_file_status(file_status);
            if (status != MMAY_CLI_FORMAT_TARGET_OK || end) {
                mmay_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (entry_count > MMAY_CLI_FORMAT_MAX_ENTRIES) {
                mmay_owned_bytes_dispose(&name);
                status = MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
                break;
            }
            status = mmay_cli_path_join(
                allocator,
                &directory.path,
                (const char *)name.data,
                name.length,
                &child
            );
            if (status != MMAY_CLI_FORMAT_TARGET_OK) {
                mmay_owned_bytes_dispose(&name);
                break;
            }
            if (kind == MMAY_DIRECTORY_ENTRY_REGULAR
                && (mmay_cli_source_path((const char *)child.data)
                    || (root_directory
                        && mmay_cli_manifest_kind(
                            (const char *)child.data,
                            &(MarimayManifestKind){0}
                        )))) {
                status = mmay_cli_format_target_add_owned(targets, &child);
            } else if (kind == MMAY_DIRECTORY_ENTRY_DIRECTORY
                && !mmay_cli_format_ignored_directory(&name)) {
                MarimayOwnedBytes manifest = {0};
                MarimayDirectoryEntryKind manifest_kind = MMAY_DIRECTORY_ENTRY_MISSING;
                status = mmay_cli_path_join(
                    allocator,
                    &child,
                    "mmay.mod",
                    strlen("mmay.mod"),
                    &manifest
                );
                if (status == MMAY_CLI_FORMAT_TARGET_OK) {
                    file_status = mmay_directory_kind(
                        allocator,
                        (const char *)manifest.data,
                        manifest.length,
                        &manifest_kind
                    );
                    status = mmay_cli_format_target_file_status(file_status);
                }
                mmay_owned_bytes_dispose(&manifest);
                if (status == MMAY_CLI_FORMAT_TARGET_OK
                    && manifest_kind != MMAY_DIRECTORY_ENTRY_REGULAR) {
                    MarimayCliFormatDirectory nested;
                    memset(&nested, 0, sizeof(nested));
                    if (directory.depth >= MMAY_CLI_FORMAT_MAX_DEPTH
                        || directories.length
                            >= MMAY_CLI_FORMAT_MAX_DIRECTORIES) {
                        status = MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED;
                    } else {
                        nested.path = child;
                        nested.depth = directory.depth + 1;
                        if (!mmay_vector_push(&directories, &nested)) {
                            status = MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
                        } else {
                            memset(&child, 0, sizeof(child));
                        }
                    }
                }
            }
            mmay_owned_bytes_dispose(&child);
            mmay_owned_bytes_dispose(&name);
            if (status != MMAY_CLI_FORMAT_TARGET_OK) {
                break;
            }
        }
        file_status = mmay_directory_close(&iterator);
        if (status == MMAY_CLI_FORMAT_TARGET_OK) {
            status = mmay_cli_format_target_file_status(file_status);
        }
        mmay_owned_bytes_dispose(&directory.path);
    }
    while (directories.length != 0) {
        MarimayCliFormatDirectory *directory = mmay_vector_at(
            &directories,
            directories.length - 1
        );
        mmay_owned_bytes_dispose(&directory->path);
        directories.length -= 1;
    }
    mmay_vector_dispose(&directories);
    return status;
}

static MarimayCliFormatTargetStatus mmay_cli_discover_format_roots(
    MarimayAllocator allocator,
    MarimayOwnedBytes *module_root,
    MarimayOwnedBytes *workspace_manifest
) {
    MarimayOwnedBytes cursor = {0};
    MarimayCliFormatTargetStatus status;
    MarimayFileStatus file_status = mmay_directory_current(
        allocator,
        MMAY_CLI_FORMAT_MAX_PATH_BYTES,
        &cursor
    );
    memset(module_root, 0, sizeof(*module_root));
    memset(workspace_manifest, 0, sizeof(*workspace_manifest));
    status = mmay_cli_format_target_file_status(file_status);
    while (status == MMAY_CLI_FORMAT_TARGET_OK) {
        MarimayOwnedBytes candidate = {0};
        MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
        status = mmay_cli_path_join(
            allocator,
            &cursor,
            "mmay.mod",
            strlen("mmay.mod"),
            &candidate
        );
        if (status == MMAY_CLI_FORMAT_TARGET_OK) {
            file_status = mmay_directory_kind(
                allocator,
                (const char *)candidate.data,
                candidate.length,
                &kind
            );
            status = mmay_cli_format_target_file_status(file_status);
        }
        mmay_owned_bytes_dispose(&candidate);
        if (status != MMAY_CLI_FORMAT_TARGET_OK) {
            break;
        }
        if (kind == MMAY_DIRECTORY_ENTRY_REGULAR) {
            *module_root = cursor;
            memset(&cursor, 0, sizeof(cursor));
            break;
        }
        if (!mmay_cli_path_parent(&cursor)) {
            status = MMAY_CLI_FORMAT_TARGET_INPUT_ERROR;
            break;
        }
    }
    mmay_owned_bytes_dispose(&cursor);
    if (status != MMAY_CLI_FORMAT_TARGET_OK) {
        return status;
    }
    if (!mmay_owned_bytes_copy(
            &cursor,
            allocator,
            module_root->data,
            module_root->length
        )) {
        return MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    for (;;) {
        MarimayOwnedBytes candidate = {0};
        MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
        status = mmay_cli_path_join(
            allocator,
            &cursor,
            "mmay.work",
            strlen("mmay.work"),
            &candidate
        );
        if (status == MMAY_CLI_FORMAT_TARGET_OK) {
            file_status = mmay_directory_kind(
                allocator,
                (const char *)candidate.data,
                candidate.length,
                &kind
            );
            status = mmay_cli_format_target_file_status(file_status);
        }
        if (status != MMAY_CLI_FORMAT_TARGET_OK) {
            mmay_owned_bytes_dispose(&candidate);
            break;
        }
        if (kind == MMAY_DIRECTORY_ENTRY_REGULAR) {
            *workspace_manifest = candidate;
            memset(&candidate, 0, sizeof(candidate));
            mmay_owned_bytes_dispose(&candidate);
            break;
        }
        mmay_owned_bytes_dispose(&candidate);
        if (!mmay_cli_path_parent(&cursor)) {
            break;
        }
    }
    mmay_owned_bytes_dispose(&cursor);
    return status;
}

int mmay_cli_format_target_status_error(
    const MarimayCliOptions *options,
    MarimayCliFormatTargetStatus status,
    MarimayWriter standard_error
) {
    if (status == MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED) {
        return MMAY_EXIT_INTERNAL;
    }
    return mmay_cli_plain_error(
        standard_error,
        options->diagnostic_style,
        status == MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED
            ? MMAY_DIAGNOSTIC_FORMAT_LIMIT
            : MMAY_DIAGNOSTIC_FORMAT_INPUT_IO,
        status == MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED
            ? "formatter target discovery exhausted a defensive path or tree limit"
            : "cannot discover or traverse the selected formatter target",
        MMAY_EXIT_SOURCE_FAILURE
    );
}

static int mmay_cli_format_selected_files(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayVector *targets,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    size_t index = 0;
    int aggregate = MMAY_EXIT_SUCCESS;
    for (index = 0; index < targets->length; index += 1) {
        const MarimayCliFormatPath *target = mmay_vector_at_const(targets, index);
        MarimayCliOptions file_options = *options;
        int result;
        file_options.target = (const char *)target->path.data;
        result = mmay_cli_format_file(
            session,
            &file_options,
            standard_output,
            standard_error
        );
        if (result == MMAY_EXIT_INTERNAL) {
            return result;
        }
        if (result != MMAY_EXIT_SUCCESS) {
            aggregate = result;
        }
    }
    return aggregate;
}

int mmay_cli_format_target(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayVector targets;
    MarimayCliFormatTargetStatus status = MMAY_CLI_FORMAT_TARGET_OK;
    MarimayOwnedBytes module_root = {0};
    MarimayOwnedBytes workspace_manifest = {0};
    if (!mmay_vector_init(
            &targets,
            session->allocator,
            sizeof(MarimayCliFormatPath)
        )) {
        return MMAY_EXIT_INTERNAL;
    }
    if (options->target == NULL) {
        status = mmay_cli_discover_format_roots(
            session->allocator,
            &module_root,
            &workspace_manifest
        );
        if (status == MMAY_CLI_FORMAT_TARGET_OK) {
            status = mmay_cli_collect_format_tree(
                session->allocator,
                &module_root,
                &targets
            );
        }
        if (status == MMAY_CLI_FORMAT_TARGET_OK
            && workspace_manifest.data != NULL) {
            status = mmay_cli_format_target_add_owned(
                &targets,
                &workspace_manifest
            );
        }
    } else {
        MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
        MarimayFileStatus file_status = mmay_directory_kind(
            session->allocator,
            options->target,
            strlen(options->target),
            &kind
        );
        status = mmay_cli_format_target_file_status(file_status);
        if (status == MMAY_CLI_FORMAT_TARGET_OK
            && kind == MMAY_DIRECTORY_ENTRY_REGULAR
            && mmay_cli_format_file_path(options->target)) {
            status = mmay_cli_format_target_add_copy(
                &targets,
                session->allocator,
                options->target,
                strlen(options->target)
            );
        } else if (status == MMAY_CLI_FORMAT_TARGET_OK
            && kind == MMAY_DIRECTORY_ENTRY_DIRECTORY) {
            if (!mmay_cli_path_copy(
                    &module_root,
                    session->allocator,
                    options->target,
                    strlen(options->target)
                )) {
                status = strlen(options->target)
                        > MMAY_CLI_FORMAT_MAX_PATH_BYTES
                    ? MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED
                    : MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
            } else {
                status = mmay_cli_collect_format_tree(
                    session->allocator,
                    &module_root,
                    &targets
                );
            }
        } else if (status == MMAY_CLI_FORMAT_TARGET_OK) {
            status = MMAY_CLI_FORMAT_TARGET_INPUT_ERROR;
        }
    }
    mmay_owned_bytes_dispose(&workspace_manifest);
    mmay_owned_bytes_dispose(&module_root);
    if (status != MMAY_CLI_FORMAT_TARGET_OK) {
        int result = mmay_cli_format_target_status_error(
            options,
            status,
            standard_error
        );
        mmay_cli_format_targets_dispose(&targets);
        return result;
    }
    mmay_cli_format_targets_sort_unique(&targets);
    {
        int result = mmay_cli_format_selected_files(
            session,
            options,
            &targets,
            standard_output,
            standard_error
        );
        mmay_cli_format_targets_dispose(&targets);
        return result;
    }
}
