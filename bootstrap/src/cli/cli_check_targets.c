#include "cli_check_targets.h"
#include "cli_standard_packages.h"

#include <stdlib.h>
#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

typedef struct MarimayCliCheckPath { MarimayOwnedBytes path; } MarimayCliCheckPath;
typedef struct MarimayCliCheckDirectory {
    MarimayOwnedBytes path;
    size_t depth;
} MarimayCliCheckDirectory;

static MarimayCliCheckTargetStatus mmay_check_file_status(MarimayFileStatus status) {
    if (status == MMAY_FILE_OK) {
        return MMAY_CLI_CHECK_TARGET_OK;
    }
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    if (status == MMAY_FILE_LIMIT_EXCEEDED) {
        return MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
    }
    return MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
}

static MarimayCliCheckTargetStatus mmay_check_compiler_status(
    MarimayCompilerStatus status
) {
    if (status == MMAY_COMPILER_OK) {
        return MMAY_CLI_CHECK_TARGET_OK;
    }
    if (status == MMAY_COMPILER_ALLOCATION_FAILED) {
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    if (status == MMAY_COMPILER_LIMIT_REACHED) {
        return MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
    }
    return MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR;
}

static bool mmay_check_path_equal(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
) {
    return left->length == right->length
        && (left->length == 0
            || memcmp(left->data, right->data, left->length) == 0);
}

static bool mmay_check_path_copy(
    MarimayOwnedBytes *output,
    MarimayAllocator allocator,
    const void *bytes,
    size_t length
) {
    size_t index = 0;
    const unsigned char *input = bytes;
    while (length > 1
        && (input[length - 1] == '/' || input[length - 1] == '\\')) {
        length -= 1;
    }
    if (length > MMAY_CLI_CHECK_MAX_PATH_BYTES
        || !mmay_owned_bytes_copy(output, allocator, bytes, length)) {
        return false;
    }
    for (index = 0; index < output->length; index += 1) {
        if (output->data[index] == '\\') {
            output->data[index] = '/';
        }
    }
    return true;
}

static MarimayCliCheckTargetStatus mmay_check_path_join(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *parent,
    const void *name,
    size_t name_length,
    MarimayOwnedBytes *output
) {
    bool separator = parent->length != 0
        && parent->data[parent->length - 1] != '/';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!mmay_size_add(parent->length, separator ? 1 : 0, &length)
        || !mmay_size_add(length, name_length, &length)
        || length > MMAY_CLI_CHECK_MAX_PATH_BYTES
        || !mmay_size_add(length, 1, &allocation_size)) {
        return MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    memcpy(bytes, parent->data, parent->length);
    if (separator) {
        bytes[parent->length] = '/';
    }
    memcpy(bytes + parent->length + (separator ? 1 : 0), name, name_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return MMAY_CLI_CHECK_TARGET_OK;
}

static bool mmay_check_path_absolute(const char *path) {
    size_t length = strlen(path);
    return (length != 0 && path[0] == '/')
        || (length >= 2 && ((path[0] >= 'A' && path[0] <= 'Z')
                || (path[0] >= 'a' && path[0] <= 'z'))
            && path[1] == ':')
        || (length >= 2 && path[0] == '\\' && path[1] == '\\');
}

static bool mmay_check_path_root(const MarimayOwnedBytes *path) {
    size_t index = 0;
    size_t separators = 0;
    if (path->length == 1 && path->data[0] == '/') {
        return true;
    }
    if (path->length == 3 && path->data[1] == ':'
        && path->data[2] == '/') {
        return true;
    }
    if (path->length < 5 || path->data[0] != '/' || path->data[1] != '/') {
        return false;
    }
    for (index = 2; index < path->length; index += 1) {
        if (path->data[index] == '/') {
            separators += 1;
        }
    }
    return separators == 1;
}

static MarimayCliCheckTargetStatus mmay_check_path_parent(MarimayOwnedBytes *path) {
    size_t length = path->length;
    size_t parent_length = 0;
    MarimayOwnedBytes parent = {0};
    if (mmay_check_path_root(path)) {
        return MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
    }
    while (length != 0 && path->data[length - 1] != '/') {
        length -= 1;
    }
    if (length == 0) {
        return MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
    }
    parent_length = length == 1 ? 1 : length - 1;
    if (parent_length == 2 && path->data[1] == ':') {
        parent_length = 3;
    }
    if (!mmay_owned_bytes_copy(
            &parent, path->allocator, path->data, parent_length
        )) {
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    mmay_owned_bytes_dispose(path);
    *path = parent;
    return MMAY_CLI_CHECK_TARGET_OK;
}

static MarimayCliCheckTargetStatus mmay_check_absolute_path(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *current,
    const char *path,
    MarimayOwnedBytes *output
) {
    if (mmay_check_path_absolute(path)) {
        return mmay_check_path_copy(
            output, allocator, path, strlen(path)
        ) ? MMAY_CLI_CHECK_TARGET_OK
            : (strlen(path) > MMAY_CLI_CHECK_MAX_PATH_BYTES
                ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                : MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED);
    }
    return mmay_check_path_join(
        allocator, current, path, strlen(path), output
    );
}

static MarimayCliCheckTargetStatus mmay_check_find_module_root(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *start,
    MarimayOwnedBytes *root
) {
    MarimayOwnedBytes cursor = {0};
    MarimayCliCheckTargetStatus status;
    memset(root, 0, sizeof(*root));
    if (!mmay_check_path_copy(
            &cursor, allocator, start->data, start->length
        )) {
        return start->length > MMAY_CLI_CHECK_MAX_PATH_BYTES
            ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
            : MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    for (;;) {
        MarimayOwnedBytes manifest = {0};
        MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
        status = mmay_check_path_join(
            allocator, &cursor, "mmay.mod", sizeof("mmay.mod") - 1,
            &manifest
        );
        if (status == MMAY_CLI_CHECK_TARGET_OK) {
            status = mmay_check_file_status(mmay_directory_kind(
                allocator, (const char *)manifest.data, manifest.length, &kind
            ));
        }
        mmay_owned_bytes_dispose(&manifest);
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            break;
        }
        if (kind == MMAY_DIRECTORY_ENTRY_REGULAR) {
            *root = cursor;
            memset(&cursor, 0, sizeof(cursor));
            break;
        }
        status = mmay_check_path_parent(&cursor);
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            break;
        }
    }
    mmay_owned_bytes_dispose(&cursor);
    return status;
}

static int mmay_check_path_compare(const void *left, const void *right) {
    const MarimayCliCheckPath *first = left;
    const MarimayCliCheckPath *second = right;
    size_t shared = first->path.length < second->path.length
        ? first->path.length : second->path.length;
    int comparison = shared == 0 ? 0
        : memcmp(first->path.data, second->path.data, shared);
    if (comparison != 0) {
        return comparison;
    }
    return first->path.length < second->path.length ? -1
        : first->path.length > second->path.length;
}

static void mmay_check_paths_dispose(MarimayVector *paths) {
    size_t index = 0;
    for (index = 0; index < paths->length; index += 1) {
        MarimayCliCheckPath *path = mmay_vector_at(paths, index);
        mmay_owned_bytes_dispose(&path->path);
    }
    mmay_vector_dispose(paths);
}

static bool mmay_check_test_file(const MarimayOwnedBytes *path) {
    static const char suffix[] = "_test.my";
    return path->length >= sizeof(suffix) - 1
        && memcmp(
            path->data + path->length - (sizeof(suffix) - 1),
            suffix,
            sizeof(suffix) - 1
        ) == 0;
}

static bool mmay_check_ignored_directory(const MarimayOwnedBytes *name) {
    return (name->length == 4 && memcmp(name->data, ".git", 4) == 0)
        || (name->length == 5 && memcmp(name->data, ".mmay", 5) == 0);
}

static MarimayCliCheckTargetStatus mmay_check_collect_sources(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *root,
    bool all_targets,
    MarimayVector *paths
) {
    MarimayVector directories;
    MarimayCliCheckDirectory initial;
    MarimayCliCheckTargetStatus status = MMAY_CLI_CHECK_TARGET_OK;
    size_t directory_count = 0;
    size_t entry_count = 0;
    if (!mmay_vector_init(
            &directories, allocator, sizeof(MarimayCliCheckDirectory)
        )) {
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    memset(&initial, 0, sizeof(initial));
    if (!mmay_check_path_copy(
            &initial.path, allocator, root->data, root->length
        ) || !mmay_vector_push(&directories, &initial)) {
        mmay_owned_bytes_dispose(&initial.path);
        mmay_vector_dispose(&directories);
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    while (directories.length != 0
        && status == MMAY_CLI_CHECK_TARGET_OK) {
        MarimayCliCheckDirectory directory = *(
            (MarimayCliCheckDirectory *)mmay_vector_at(
                &directories, directories.length - 1
            )
        );
        MarimayDirectoryIterator iterator;
        bool root_directory = mmay_check_path_equal(&directory.path, root);
        directories.length -= 1;
        directory_count += 1;
        if (directory_count > MMAY_CLI_CHECK_MAX_DIRECTORIES) {
            mmay_owned_bytes_dispose(&directory.path);
            status = MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
            break;
        }
        status = mmay_check_file_status(mmay_directory_open(
            allocator, (const char *)directory.path.data,
            directory.path.length, &iterator
        ));
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            mmay_owned_bytes_dispose(&directory.path);
            break;
        }
        for (;;) {
            MarimayOwnedBytes name = {0};
            MarimayOwnedBytes child = {0};
            MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
            bool end = false;
            status = mmay_check_file_status(mmay_directory_next(
                &iterator, allocator, MMAY_CLI_CHECK_MAX_NAME_BYTES,
                &name, &kind, &end
            ));
            if (status != MMAY_CLI_CHECK_TARGET_OK || end) {
                mmay_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (entry_count > MMAY_CLI_CHECK_MAX_ENTRIES) {
                status = MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
                mmay_owned_bytes_dispose(&name);
                break;
            }
            status = mmay_check_path_join(
                allocator, &directory.path, name.data, name.length, &child
            );
            if (status == MMAY_CLI_CHECK_TARGET_OK
                && kind == MMAY_DIRECTORY_ENTRY_REGULAR
                && mmay_cli_source_path((const char *)child.data)
                && (all_targets || !mmay_check_test_file(&child))) {
                MarimayCliCheckPath source;
                memset(&source, 0, sizeof(source));
                source.path = child;
                if (paths->length >= MMAY_CLI_CHECK_MAX_FILES) {
                    status = MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
                } else if (!mmay_vector_push(paths, &source)) {
                    status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
                } else {
                    memset(&child, 0, sizeof(child));
                }
            } else if (status == MMAY_CLI_CHECK_TARGET_OK
                && kind == MMAY_DIRECTORY_ENTRY_DIRECTORY
                && !mmay_check_ignored_directory(&name)
                && (all_targets || !root_directory
                    || name.length != sizeof("tests") - 1
                    || memcmp(name.data, "tests", sizeof("tests") - 1) != 0)) {
                MarimayOwnedBytes manifest = {0};
                MarimayDirectoryEntryKind manifest_kind =
                    MMAY_DIRECTORY_ENTRY_MISSING;
                status = mmay_check_path_join(
                    allocator, &child, "mmay.mod", sizeof("mmay.mod") - 1,
                    &manifest
                );
                if (status == MMAY_CLI_CHECK_TARGET_OK) {
                    status = mmay_check_file_status(mmay_directory_kind(
                        allocator, (const char *)manifest.data,
                        manifest.length, &manifest_kind
                    ));
                }
                mmay_owned_bytes_dispose(&manifest);
                if (status == MMAY_CLI_CHECK_TARGET_OK
                    && manifest_kind != MMAY_DIRECTORY_ENTRY_REGULAR) {
                    MarimayCliCheckDirectory nested;
                    memset(&nested, 0, sizeof(nested));
                    if (directory.depth >= MMAY_CLI_CHECK_MAX_DEPTH
                        || directories.length
                            >= MMAY_CLI_CHECK_MAX_DIRECTORIES) {
                        status = MMAY_CLI_CHECK_TARGET_LIMIT_REACHED;
                    } else {
                        nested.path = child;
                        nested.depth = directory.depth + 1;
                        if (!mmay_vector_push(&directories, &nested)) {
                            status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
                        } else {
                            memset(&child, 0, sizeof(child));
                        }
                    }
                }
            }
            mmay_owned_bytes_dispose(&child);
            mmay_owned_bytes_dispose(&name);
            if (status != MMAY_CLI_CHECK_TARGET_OK) {
                break;
            }
        }
        if (mmay_directory_close(&iterator) != MMAY_FILE_OK
            && status == MMAY_CLI_CHECK_TARGET_OK) {
            status = MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
        }
        mmay_owned_bytes_dispose(&directory.path);
    }
    while (directories.length != 0) {
        MarimayCliCheckDirectory *directory = mmay_vector_at(
            &directories, directories.length - 1
        );
        mmay_owned_bytes_dispose(&directory->path);
        directories.length -= 1;
    }
    mmay_vector_dispose(&directories);
    if (status == MMAY_CLI_CHECK_TARGET_OK && paths->length > 1) {
        qsort(paths->data, paths->length, paths->element_size,
            mmay_check_path_compare);
    }
    return status;
}

static MarimayCliCheckTargetStatus mmay_check_manifest_identity(
    MarimayCliCheckTarget *target,
    const MarimayOwnedBytes *module_root,
    MarimayOwnedBytes *module_path
) {
    MarimayOwnedBytes manifest_path = {0};
    MarimayOwnedBytes bytes = {0};
    MarimayOwnedBytes language = {0};
    const MarimaySourceSnapshot *snapshot = NULL;
    const MarimayManifestEntry *module = NULL;
    const MarimayManifestEntry *language_entry = NULL;
    MarimayCliCheckTargetStatus status;
    size_t index = 0;
    size_t module_count = 0;
    size_t language_count = 0;
    status = mmay_check_path_join(
        target->allocator, module_root, "mmay.mod", sizeof("mmay.mod") - 1,
        &manifest_path
    );
    if (status == MMAY_CLI_CHECK_TARGET_OK) {
        status = mmay_check_file_status(mmay_file_read_all(
            target->allocator, (const char *)manifest_path.data,
            manifest_path.length, MMAY_CLI_CHECK_MAX_SOURCE_BYTES, &bytes
        ));
    }
    if (status != MMAY_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    if (!mmay_source_manager_init(
            &target->manifest_sources, target->allocator
        )) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->manifest_sources_initialized = true;
    if (mmay_source_manager_add(
            &target->manifest_sources,
            (const char *)manifest_path.data,
            manifest_path.length,
            bytes.data,
            bytes.length,
            &snapshot
        ) != MMAY_SOURCE_OK) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->problem_source = snapshot;
    {
        MarimayManifestParseStatus parse_status = mmay_manifest_parse(
            snapshot, MMAY_MANIFEST_MODULE, target->allocator,
            mmay_manifest_parse_default_options(), &target->manifest
        );
        if (parse_status != MMAY_MANIFEST_PARSE_OK) {
            status = parse_status == MMAY_MANIFEST_PARSE_LIMIT_REACHED
                ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                : (parse_status == MMAY_MANIFEST_PARSE_ALLOCATION_FAILED
                    ? MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED
                    : MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR);
            goto cleanup;
        }
    }
    if (target->manifest.diagnostics.length != 0) {
        status = MMAY_CLI_CHECK_TARGET_SOURCE_INVALID;
        goto cleanup;
    }
    for (index = 0; index < target->manifest.entries.length; index += 1) {
        const MarimayManifestEntry *entry = mmay_manifest_entry(
            &target->manifest, index
        );
        if (entry->kind == MMAY_MANIFEST_DIRECTIVE_MODULE) {
            module_count += 1;
            module = entry;
        } else if (entry->kind == MMAY_MANIFEST_DIRECTIVE_LANGUAGE) {
            language_count += 1;
            language_entry = entry;
        }
    }
    if (module_count != 1 || language_count != 1) {
        target->problem_code = MMAY_DIAGNOSTIC_MODULE_IDENTITY;
        target->problem_message =
            "mmay.mod must contain exactly one module and one language directive";
        target->problem_start = 0;
        target->problem_end = 0;
        status = MMAY_CLI_CHECK_TARGET_SOURCE_INVALID;
        goto cleanup;
    }
    if (!mmay_manifest_decode_string(
            &target->manifest, module->values[0], target->allocator,
            module_path
        ) || !mmay_manifest_decode_string(
            &target->manifest, language_entry->values[0], target->allocator,
            &language
        )) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (module_path->length == 0 || language.length == 0
        || memchr(module_path->data, 0, module_path->length) != NULL
        || memchr(language.data, 0, language.length) != NULL) {
        bool module_invalid = module_path->length == 0
            || memchr(module_path->data, 0, module_path->length) != NULL;
        target->problem_code = MMAY_DIAGNOSTIC_MODULE_IDENTITY;
        target->problem_message =
            "module and language identities must be non-empty and NUL-free";
        target->problem_start = module_invalid
            ? module->values[0].start : language_entry->values[0].start;
        target->problem_end = module_invalid
            ? module->values[0].end : language_entry->values[0].end;
        status = MMAY_CLI_CHECK_TARGET_SOURCE_INVALID;
    }
cleanup:
    mmay_owned_bytes_dispose(&language);
    mmay_owned_bytes_dispose(&bytes);
    mmay_owned_bytes_dispose(&manifest_path);
    return status;
}

static MarimayCliCheckPackage *mmay_check_find_package(
    MarimayCliCheckTarget *target,
    const MarimayOwnedBytes *directory
) {
    size_t index = 0;
    for (index = 0; index < target->packages.length; index += 1) {
        MarimayCliCheckPackage *package = mmay_vector_at(
            &target->packages, index
        );
        if (mmay_check_path_equal(&package->directory, directory)) {
            return package;
        }
    }
    return NULL;
}

static MarimayCliCheckTargetStatus mmay_check_add_package(
    MarimayCliCheckTarget *target,
    const MarimayOwnedBytes *module_root,
    const MarimayOwnedBytes *module_path,
    const MarimayOwnedBytes *directory,
    MarimayCliCheckPackage **output
) {
    MarimayCliCheckPackage package;
    MarimayCliCheckTargetStatus status = MMAY_CLI_CHECK_TARGET_OK;
    memset(&package, 0, sizeof(package));
    if (!mmay_check_path_copy(
            &package.directory, target->allocator,
            directory->data, directory->length
        )) {
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    if (mmay_check_path_equal(module_root, directory)) {
        if (!mmay_owned_bytes_copy(
                &package.import_path, target->allocator,
                module_path->data, module_path->length
            )) {
            status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else {
        MarimayOwnedBytes relative;
        size_t relative_start = module_root->length;
        memset(&relative, 0, sizeof(relative));
        if (relative_start < directory->length
            && directory->data[relative_start] == '/') {
            relative_start += 1;
        }
        relative.data = directory->data + relative_start;
        relative.length = directory->length - relative_start;
        status = mmay_check_path_join(
            target->allocator, module_path, relative.data, relative.length,
            &package.import_path
        );
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            goto cleanup;
        }
    }
    if (!mmay_vector_init(
            &package.file_ids, target->allocator, sizeof(MarimayFileId)
        ) || !mmay_vector_push(&target->packages, &package)) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(&package, 0, sizeof(package));
    *output = mmay_vector_at(
        &target->packages, target->packages.length - 1
    );
    return MMAY_CLI_CHECK_TARGET_OK;
cleanup:
    mmay_vector_dispose(&package.file_ids);
    mmay_owned_bytes_dispose(&package.import_path);
    mmay_owned_bytes_dispose(&package.directory);
    return status;
}

static MarimayCliCheckTargetStatus mmay_check_load_sources(
    MarimayCliCheckTarget *target,
    const MarimayOwnedBytes *module_root,
    const MarimayOwnedBytes *module_path,
    MarimayVector *paths
) {
    size_t index = 0;
    for (index = 0; index < paths->length; index += 1) {
        MarimayCliCheckPath *source_path = mmay_vector_at(paths, index);
        MarimayOwnedBytes directory = {0};
        MarimayOwnedBytes source = {0};
        MarimayCliCheckPackage *package = NULL;
        MarimayFileId file_id = 0;
        MarimayCliCheckTargetStatus status;
        if (!mmay_check_path_copy(
                &directory, target->allocator,
                source_path->path.data, source_path->path.length
            )) {
            return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        status = mmay_check_path_parent(&directory);
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            mmay_owned_bytes_dispose(&directory);
            return status;
        }
        package = mmay_check_find_package(target, &directory);
        if (package == NULL) {
            status = mmay_check_add_package(
                target, module_root, module_path, &directory, &package
            );
            if (status != MMAY_CLI_CHECK_TARGET_OK) {
                mmay_owned_bytes_dispose(&directory);
                return status;
            }
        }
        status = mmay_check_file_status(mmay_file_read_all(
            target->allocator, (const char *)source_path->path.data,
            source_path->path.length, MMAY_CLI_CHECK_MAX_SOURCE_BYTES,
            &source
        ));
        if (status == MMAY_CLI_CHECK_TARGET_OK) {
            status = mmay_check_compiler_status(
                mmay_compiler_service_add_document(
                    &target->service,
                    (const char *)source_path->path.data,
                    source_path->path.length,
                    1,
                    source.data,
                    source.length,
                    &file_id
                )
            );
        }
        if (status == MMAY_CLI_CHECK_TARGET_OK
            && !mmay_vector_push(&package->file_ids, &file_id)) {
            status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        mmay_owned_bytes_dispose(&source);
        mmay_owned_bytes_dispose(&directory);
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            return status;
        }
    }
    return MMAY_CLI_CHECK_TARGET_OK;
}

static MarimayCliCheckTargetStatus mmay_check_build_package_views(
    MarimayCliCheckTarget *target,
    const MarimayOwnedBytes *selected_directory
) {
    size_t index = 0;
    bool selected = target->module_wide;
    for (index = 0; index < target->packages.length; index += 1) {
        MarimayCliCheckPackage *package = mmay_vector_at(&target->packages, index);
        MarimayCompilerPackage view;
        view.import_path = (const char *)package->import_path.data;
        view.import_path_length = package->import_path.length;
        view.file_ids = package->file_ids.data;
        view.file_count = package->file_ids.length;
        if (!mmay_vector_push(&target->compiler_packages, &view)) {
            return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        if (mmay_check_path_equal(&package->directory, selected_directory)) {
            mmay_owned_bytes_dispose(&target->selected_import_path);
            if (!mmay_owned_bytes_copy(
                    &target->selected_import_path,
                    target->allocator,
                    package->import_path.data,
                    package->import_path.length
                )) {
                return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            }
            selected = true;
        }
    }
    return selected ? MMAY_CLI_CHECK_TARGET_OK
        : MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
}

static MarimayCliCheckTargetStatus mmay_check_build_standalone(
    MarimayCliCheckTarget *target,
    const MarimayOwnedBytes *path
) {
    static const char package_name[] = "script";
    MarimayOwnedBytes source = {0};
    MarimayCliCheckPackage package;
    MarimayFileId file_id = 0;
    MarimayCliCheckTargetStatus status = mmay_check_file_status(mmay_file_read_all(
        target->allocator, (const char *)path->data, path->length,
        MMAY_CLI_CHECK_MAX_SOURCE_BYTES, &source
    ));
    memset(&package, 0, sizeof(package));
    if (status == MMAY_CLI_CHECK_TARGET_OK) {
        status = mmay_check_compiler_status(mmay_compiler_service_add_document(
            &target->service, (const char *)path->data, path->length, 1,
            source.data, source.length, &file_id
        ));
    }
    if (status != MMAY_CLI_CHECK_TARGET_OK
        || !mmay_owned_bytes_copy(
            &package.import_path, target->allocator,
            package_name, sizeof(package_name) - 1
        ) || !mmay_owned_bytes_copy(
            &package.directory, target->allocator, path->data, path->length
        ) || !mmay_vector_init(
            &package.file_ids, target->allocator, sizeof(MarimayFileId)
        ) || !mmay_vector_push(&package.file_ids, &file_id)
        || !mmay_vector_push(&target->packages, &package)) {
        if (status == MMAY_CLI_CHECK_TARGET_OK) {
            status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        mmay_vector_dispose(&package.file_ids);
        mmay_owned_bytes_dispose(&package.import_path);
        mmay_owned_bytes_dispose(&package.directory);
        mmay_owned_bytes_dispose(&source);
        return status;
    }
    memset(&package, 0, sizeof(package));
    mmay_owned_bytes_dispose(&source);
    return status;
}

MarimayCliCheckTargetStatus mmay_cli_check_target_build(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayCliCheckTarget *target
) {
    MarimayCompilerOptions compiler_options = mmay_compiler_default_options();
    MarimayOwnedBytes current = {0};
    MarimayOwnedBytes selected = {0};
    MarimayOwnedBytes selected_directory = {0};
    MarimayOwnedBytes module_root = {0};
    MarimayOwnedBytes module_path = {0};
    MarimayVector paths;
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_DIRECTORY;
    MarimayCliCheckTargetStatus status = MMAY_CLI_CHECK_TARGET_OK;
    bool paths_initialized = false;
    memset(target, 0, sizeof(*target));
    target->allocator = session->allocator;
    compiler_options.max_documents = MMAY_CLI_CHECK_MAX_FILES;
    compiler_options.max_path_bytes = MMAY_CLI_CHECK_MAX_PATH_BYTES;
    compiler_options.max_source_bytes = MMAY_CLI_CHECK_MAX_SOURCE_BYTES;
    status = mmay_check_compiler_status(mmay_compiler_service_init(
        &target->service, target->allocator, compiler_options
    ));
    if (status != MMAY_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    target->service_initialized = true;
    if (!mmay_vector_init(
            &target->packages, target->allocator,
            sizeof(MarimayCliCheckPackage)
        )) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->packages_initialized = true;
    if (!mmay_vector_init(
            &target->compiler_packages, target->allocator,
            sizeof(MarimayCompilerPackage)
        )) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->compiler_packages_initialized = true;
    status = mmay_check_file_status(mmay_directory_current(
        target->allocator, MMAY_CLI_CHECK_MAX_PATH_BYTES, &current
    ));
    if (status != MMAY_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    if (options->target == NULL) {
        if (!mmay_check_path_copy(
                &selected, target->allocator, current.data, current.length
            )) {
            status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else {
        status = mmay_check_absolute_path(
            target->allocator, &current, options->target, &selected
        );
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            goto cleanup;
        }
        status = mmay_check_file_status(mmay_directory_kind(
            target->allocator, (const char *)selected.data,
            selected.length, &kind
        ));
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            goto cleanup;
        }
        if (kind == MMAY_DIRECTORY_ENTRY_REGULAR) {
            if (!mmay_cli_source_path((const char *)selected.data)) {
                status = MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
            } else {
                if (!mmay_check_path_copy(
                        &target->root_directory, target->allocator,
                        selected.data, selected.length
                    )) {
                    status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
                    goto cleanup;
                }
                status = mmay_check_path_parent(&target->root_directory);
                if (status != MMAY_CLI_CHECK_TARGET_OK) goto cleanup;
                status = mmay_check_build_standalone(target, &selected);
                if (status == MMAY_CLI_CHECK_TARGET_OK) {
                    status = mmay_cli_standard_packages_add(target);
                }
                if (status == MMAY_CLI_CHECK_TARGET_OK) {
                    status = mmay_check_build_package_views(
                        target, &selected
                    );
                }
            }
            goto cleanup;
        }
        if (kind != MMAY_DIRECTORY_ENTRY_DIRECTORY) {
            status = MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
            goto cleanup;
        }
    }
    if (!mmay_check_path_copy(
            &selected_directory, target->allocator,
            selected.data, selected.length
        )) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    status = mmay_check_find_module_root(
        target->allocator, &selected_directory, &module_root
    );
    if (status != MMAY_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    target->module_wide = options->target == NULL
        || mmay_check_path_equal(&selected_directory, &module_root);
    if (target->module_wide
        && !mmay_check_path_equal(&selected_directory, &module_root)) {
        mmay_owned_bytes_dispose(&selected_directory);
        if (!mmay_check_path_copy(
                &selected_directory, target->allocator,
                module_root.data, module_root.length
            )) {
            status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    status = mmay_check_manifest_identity(
        target, &module_root, &module_path
    );
    if (status != MMAY_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    if (!mmay_check_path_copy(
            &target->root_directory, target->allocator,
            module_root.data, module_root.length
        )) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!mmay_vector_init(&paths, target->allocator, sizeof(MarimayCliCheckPath))) {
        status = MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    paths_initialized = true;
    status = mmay_check_collect_sources(
        target->allocator, &module_root, options->check_all_targets, &paths
    );
    if (status == MMAY_CLI_CHECK_TARGET_OK && paths.length == 0) {
        status = MMAY_CLI_CHECK_TARGET_INPUT_ERROR;
    }
    if (status == MMAY_CLI_CHECK_TARGET_OK) {
        status = mmay_check_load_sources(
            target, &module_root, &module_path, &paths
        );
    }
    if (status == MMAY_CLI_CHECK_TARGET_OK) {
        status = mmay_cli_standard_packages_add(target);
    }
    if (status == MMAY_CLI_CHECK_TARGET_OK) {
        status = mmay_check_build_package_views(
            target, &selected_directory
        );
    }
cleanup:
    if (paths_initialized) {
        mmay_check_paths_dispose(&paths);
    }
    mmay_owned_bytes_dispose(&module_path);
    mmay_owned_bytes_dispose(&module_root);
    mmay_owned_bytes_dispose(&selected_directory);
    mmay_owned_bytes_dispose(&selected);
    mmay_owned_bytes_dispose(&current);
    return status;
}

void mmay_cli_check_target_dispose(MarimayCliCheckTarget *target) {
    size_t index = 0;
    if (target == NULL) {
        return;
    }
    mmay_manifest_parse_result_dispose(&target->manifest);
    if (target->manifest_sources_initialized) {
        mmay_source_manager_dispose(&target->manifest_sources);
    }
    mmay_owned_bytes_dispose(&target->selected_import_path);
    mmay_owned_bytes_dispose(&target->root_directory);
    if (target->compiler_packages_initialized) {
        mmay_vector_dispose(&target->compiler_packages);
    }
    if (target->packages_initialized) {
        for (index = 0; index < target->packages.length; index += 1) {
            MarimayCliCheckPackage *package = mmay_vector_at(
                &target->packages, index
            );
            mmay_vector_dispose(&package->file_ids);
            mmay_owned_bytes_dispose(&package->directory);
            mmay_owned_bytes_dispose(&package->import_path);
        }
        mmay_vector_dispose(&target->packages);
    }
    if (target->service_initialized) {
        mmay_compiler_service_dispose(&target->service);
    }
    memset(target, 0, sizeof(*target));
}
