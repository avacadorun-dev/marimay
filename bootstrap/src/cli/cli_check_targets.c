#include "cli_check_targets.h"

#include <stdlib.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

typedef struct RudaCliCheckPath { RudaOwnedBytes path; } RudaCliCheckPath;
typedef struct RudaCliCheckDirectory {
    RudaOwnedBytes path;
    size_t depth;
} RudaCliCheckDirectory;

static RudaCliCheckTargetStatus ruda_check_file_status(RudaFileStatus status) {
    if (status == RUDA_FILE_OK) {
        return RUDA_CLI_CHECK_TARGET_OK;
    }
    if (status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    if (status == RUDA_FILE_LIMIT_EXCEEDED) {
        return RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
    }
    return RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
}

static RudaCliCheckTargetStatus ruda_check_compiler_status(
    RudaCompilerStatus status
) {
    if (status == RUDA_COMPILER_OK) {
        return RUDA_CLI_CHECK_TARGET_OK;
    }
    if (status == RUDA_COMPILER_ALLOCATION_FAILED) {
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    if (status == RUDA_COMPILER_LIMIT_REACHED) {
        return RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
    }
    return RUDA_CLI_CHECK_TARGET_INTERNAL_ERROR;
}

static bool ruda_check_path_equal(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    return left->length == right->length
        && (left->length == 0
            || memcmp(left->data, right->data, left->length) == 0);
}

static bool ruda_check_path_copy(
    RudaOwnedBytes *output,
    RudaAllocator allocator,
    const void *bytes,
    size_t length
) {
    size_t index = 0;
    const unsigned char *input = bytes;
    while (length > 1
        && (input[length - 1] == '/' || input[length - 1] == '\\')) {
        length -= 1;
    }
    if (length > RUDA_CLI_CHECK_MAX_PATH_BYTES
        || !ruda_owned_bytes_copy(output, allocator, bytes, length)) {
        return false;
    }
    for (index = 0; index < output->length; index += 1) {
        if (output->data[index] == '\\') {
            output->data[index] = '/';
        }
    }
    return true;
}

static RudaCliCheckTargetStatus ruda_check_path_join(
    RudaAllocator allocator,
    const RudaOwnedBytes *parent,
    const void *name,
    size_t name_length,
    RudaOwnedBytes *output
) {
    bool separator = parent->length != 0
        && parent->data[parent->length - 1] != '/';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!ruda_size_add(parent->length, separator ? 1 : 0, &length)
        || !ruda_size_add(length, name_length, &length)
        || length > RUDA_CLI_CHECK_MAX_PATH_BYTES
        || !ruda_size_add(length, 1, &allocation_size)) {
        return RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
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
    return RUDA_CLI_CHECK_TARGET_OK;
}

static bool ruda_check_path_absolute(const char *path) {
    size_t length = strlen(path);
    return (length != 0 && path[0] == '/')
        || (length >= 2 && ((path[0] >= 'A' && path[0] <= 'Z')
                || (path[0] >= 'a' && path[0] <= 'z'))
            && path[1] == ':')
        || (length >= 2 && path[0] == '\\' && path[1] == '\\');
}

static bool ruda_check_path_root(const RudaOwnedBytes *path) {
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

static RudaCliCheckTargetStatus ruda_check_path_parent(RudaOwnedBytes *path) {
    size_t length = path->length;
    size_t parent_length = 0;
    RudaOwnedBytes parent = {0};
    if (ruda_check_path_root(path)) {
        return RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
    }
    while (length != 0 && path->data[length - 1] != '/') {
        length -= 1;
    }
    if (length == 0) {
        return RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
    }
    parent_length = length == 1 ? 1 : length - 1;
    if (parent_length == 2 && path->data[1] == ':') {
        parent_length = 3;
    }
    if (!ruda_owned_bytes_copy(
            &parent, path->allocator, path->data, parent_length
        )) {
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    ruda_owned_bytes_dispose(path);
    *path = parent;
    return RUDA_CLI_CHECK_TARGET_OK;
}

static RudaCliCheckTargetStatus ruda_check_absolute_path(
    RudaAllocator allocator,
    const RudaOwnedBytes *current,
    const char *path,
    RudaOwnedBytes *output
) {
    if (ruda_check_path_absolute(path)) {
        return ruda_check_path_copy(
            output, allocator, path, strlen(path)
        ) ? RUDA_CLI_CHECK_TARGET_OK
            : (strlen(path) > RUDA_CLI_CHECK_MAX_PATH_BYTES
                ? RUDA_CLI_CHECK_TARGET_LIMIT_REACHED
                : RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED);
    }
    return ruda_check_path_join(
        allocator, current, path, strlen(path), output
    );
}

static RudaCliCheckTargetStatus ruda_check_find_module_root(
    RudaAllocator allocator,
    const RudaOwnedBytes *start,
    RudaOwnedBytes *root
) {
    RudaOwnedBytes cursor = {0};
    RudaCliCheckTargetStatus status;
    memset(root, 0, sizeof(*root));
    if (!ruda_check_path_copy(
            &cursor, allocator, start->data, start->length
        )) {
        return start->length > RUDA_CLI_CHECK_MAX_PATH_BYTES
            ? RUDA_CLI_CHECK_TARGET_LIMIT_REACHED
            : RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    for (;;) {
        RudaOwnedBytes manifest = {0};
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        status = ruda_check_path_join(
            allocator, &cursor, "ruda.mod", sizeof("ruda.mod") - 1,
            &manifest
        );
        if (status == RUDA_CLI_CHECK_TARGET_OK) {
            status = ruda_check_file_status(ruda_directory_kind(
                allocator, (const char *)manifest.data, manifest.length, &kind
            ));
        }
        ruda_owned_bytes_dispose(&manifest);
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            break;
        }
        if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
            *root = cursor;
            memset(&cursor, 0, sizeof(cursor));
            break;
        }
        status = ruda_check_path_parent(&cursor);
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            break;
        }
    }
    ruda_owned_bytes_dispose(&cursor);
    return status;
}

static int ruda_check_path_compare(const void *left, const void *right) {
    const RudaCliCheckPath *first = left;
    const RudaCliCheckPath *second = right;
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

static void ruda_check_paths_dispose(RudaVector *paths) {
    size_t index = 0;
    for (index = 0; index < paths->length; index += 1) {
        RudaCliCheckPath *path = ruda_vector_at(paths, index);
        ruda_owned_bytes_dispose(&path->path);
    }
    ruda_vector_dispose(paths);
}

static bool ruda_check_test_file(const RudaOwnedBytes *path) {
    static const char suffix[] = "_test.ruda";
    return path->length >= sizeof(suffix) - 1
        && memcmp(
            path->data + path->length - (sizeof(suffix) - 1),
            suffix,
            sizeof(suffix) - 1
        ) == 0;
}

static bool ruda_check_ignored_directory(const RudaOwnedBytes *name) {
    return (name->length == 4 && memcmp(name->data, ".git", 4) == 0)
        || (name->length == 5 && memcmp(name->data, ".ruda", 5) == 0);
}

static RudaCliCheckTargetStatus ruda_check_collect_sources(
    RudaAllocator allocator,
    const RudaOwnedBytes *root,
    bool all_targets,
    RudaVector *paths
) {
    RudaVector directories;
    RudaCliCheckDirectory initial;
    RudaCliCheckTargetStatus status = RUDA_CLI_CHECK_TARGET_OK;
    size_t directory_count = 0;
    size_t entry_count = 0;
    if (!ruda_vector_init(
            &directories, allocator, sizeof(RudaCliCheckDirectory)
        )) {
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    memset(&initial, 0, sizeof(initial));
    if (!ruda_check_path_copy(
            &initial.path, allocator, root->data, root->length
        ) || !ruda_vector_push(&directories, &initial)) {
        ruda_owned_bytes_dispose(&initial.path);
        ruda_vector_dispose(&directories);
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    while (directories.length != 0
        && status == RUDA_CLI_CHECK_TARGET_OK) {
        RudaCliCheckDirectory directory = *(
            (RudaCliCheckDirectory *)ruda_vector_at(
                &directories, directories.length - 1
            )
        );
        RudaDirectoryIterator iterator;
        bool root_directory = ruda_check_path_equal(&directory.path, root);
        directories.length -= 1;
        directory_count += 1;
        if (directory_count > RUDA_CLI_CHECK_MAX_DIRECTORIES) {
            ruda_owned_bytes_dispose(&directory.path);
            status = RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
            break;
        }
        status = ruda_check_file_status(ruda_directory_open(
            allocator, (const char *)directory.path.data,
            directory.path.length, &iterator
        ));
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            ruda_owned_bytes_dispose(&directory.path);
            break;
        }
        for (;;) {
            RudaOwnedBytes name = {0};
            RudaOwnedBytes child = {0};
            RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
            bool end = false;
            status = ruda_check_file_status(ruda_directory_next(
                &iterator, allocator, RUDA_CLI_CHECK_MAX_NAME_BYTES,
                &name, &kind, &end
            ));
            if (status != RUDA_CLI_CHECK_TARGET_OK || end) {
                ruda_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (entry_count > RUDA_CLI_CHECK_MAX_ENTRIES) {
                status = RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
                ruda_owned_bytes_dispose(&name);
                break;
            }
            status = ruda_check_path_join(
                allocator, &directory.path, name.data, name.length, &child
            );
            if (status == RUDA_CLI_CHECK_TARGET_OK
                && kind == RUDA_DIRECTORY_ENTRY_REGULAR
                && ruda_cli_source_path((const char *)child.data)
                && (all_targets || !ruda_check_test_file(&child))) {
                RudaCliCheckPath source;
                memset(&source, 0, sizeof(source));
                source.path = child;
                if (paths->length >= RUDA_CLI_CHECK_MAX_FILES) {
                    status = RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
                } else if (!ruda_vector_push(paths, &source)) {
                    status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
                } else {
                    memset(&child, 0, sizeof(child));
                }
            } else if (status == RUDA_CLI_CHECK_TARGET_OK
                && kind == RUDA_DIRECTORY_ENTRY_DIRECTORY
                && !ruda_check_ignored_directory(&name)
                && (all_targets || !root_directory
                    || name.length != sizeof("tests") - 1
                    || memcmp(name.data, "tests", sizeof("tests") - 1) != 0)) {
                RudaOwnedBytes manifest = {0};
                RudaDirectoryEntryKind manifest_kind =
                    RUDA_DIRECTORY_ENTRY_MISSING;
                status = ruda_check_path_join(
                    allocator, &child, "ruda.mod", sizeof("ruda.mod") - 1,
                    &manifest
                );
                if (status == RUDA_CLI_CHECK_TARGET_OK) {
                    status = ruda_check_file_status(ruda_directory_kind(
                        allocator, (const char *)manifest.data,
                        manifest.length, &manifest_kind
                    ));
                }
                ruda_owned_bytes_dispose(&manifest);
                if (status == RUDA_CLI_CHECK_TARGET_OK
                    && manifest_kind != RUDA_DIRECTORY_ENTRY_REGULAR) {
                    RudaCliCheckDirectory nested;
                    memset(&nested, 0, sizeof(nested));
                    if (directory.depth >= RUDA_CLI_CHECK_MAX_DEPTH
                        || directories.length
                            >= RUDA_CLI_CHECK_MAX_DIRECTORIES) {
                        status = RUDA_CLI_CHECK_TARGET_LIMIT_REACHED;
                    } else {
                        nested.path = child;
                        nested.depth = directory.depth + 1;
                        if (!ruda_vector_push(&directories, &nested)) {
                            status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
                        } else {
                            memset(&child, 0, sizeof(child));
                        }
                    }
                }
            }
            ruda_owned_bytes_dispose(&child);
            ruda_owned_bytes_dispose(&name);
            if (status != RUDA_CLI_CHECK_TARGET_OK) {
                break;
            }
        }
        if (ruda_directory_close(&iterator) != RUDA_FILE_OK
            && status == RUDA_CLI_CHECK_TARGET_OK) {
            status = RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
        }
        ruda_owned_bytes_dispose(&directory.path);
    }
    while (directories.length != 0) {
        RudaCliCheckDirectory *directory = ruda_vector_at(
            &directories, directories.length - 1
        );
        ruda_owned_bytes_dispose(&directory->path);
        directories.length -= 1;
    }
    ruda_vector_dispose(&directories);
    if (status == RUDA_CLI_CHECK_TARGET_OK && paths->length > 1) {
        qsort(paths->data, paths->length, paths->element_size,
            ruda_check_path_compare);
    }
    return status;
}

static RudaCliCheckTargetStatus ruda_check_manifest_identity(
    RudaCliCheckTarget *target,
    const RudaOwnedBytes *module_root,
    RudaOwnedBytes *module_path
) {
    RudaOwnedBytes manifest_path = {0};
    RudaOwnedBytes bytes = {0};
    RudaOwnedBytes language = {0};
    const RudaSourceSnapshot *snapshot = NULL;
    const RudaManifestEntry *module = NULL;
    const RudaManifestEntry *language_entry = NULL;
    RudaCliCheckTargetStatus status;
    size_t index = 0;
    size_t module_count = 0;
    size_t language_count = 0;
    status = ruda_check_path_join(
        target->allocator, module_root, "ruda.mod", sizeof("ruda.mod") - 1,
        &manifest_path
    );
    if (status == RUDA_CLI_CHECK_TARGET_OK) {
        status = ruda_check_file_status(ruda_file_read_all(
            target->allocator, (const char *)manifest_path.data,
            manifest_path.length, RUDA_CLI_CHECK_MAX_SOURCE_BYTES, &bytes
        ));
    }
    if (status != RUDA_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    if (!ruda_source_manager_init(
            &target->manifest_sources, target->allocator
        )) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->manifest_sources_initialized = true;
    if (ruda_source_manager_add(
            &target->manifest_sources,
            (const char *)manifest_path.data,
            manifest_path.length,
            bytes.data,
            bytes.length,
            &snapshot
        ) != RUDA_SOURCE_OK) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->problem_source = snapshot;
    {
        RudaManifestParseStatus parse_status = ruda_manifest_parse(
            snapshot, RUDA_MANIFEST_MODULE, target->allocator,
            ruda_manifest_parse_default_options(), &target->manifest
        );
        if (parse_status != RUDA_MANIFEST_PARSE_OK) {
            status = parse_status == RUDA_MANIFEST_PARSE_LIMIT_REACHED
                ? RUDA_CLI_CHECK_TARGET_LIMIT_REACHED
                : (parse_status == RUDA_MANIFEST_PARSE_ALLOCATION_FAILED
                    ? RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED
                    : RUDA_CLI_CHECK_TARGET_INTERNAL_ERROR);
            goto cleanup;
        }
    }
    if (target->manifest.diagnostics.length != 0) {
        status = RUDA_CLI_CHECK_TARGET_SOURCE_INVALID;
        goto cleanup;
    }
    for (index = 0; index < target->manifest.entries.length; index += 1) {
        const RudaManifestEntry *entry = ruda_manifest_entry(
            &target->manifest, index
        );
        if (entry->kind == RUDA_MANIFEST_DIRECTIVE_MODULE) {
            module_count += 1;
            module = entry;
        } else if (entry->kind == RUDA_MANIFEST_DIRECTIVE_LANGUAGE) {
            language_count += 1;
            language_entry = entry;
        }
    }
    if (module_count != 1 || language_count != 1) {
        target->problem_code = RUDA_DIAGNOSTIC_MODULE_IDENTITY;
        target->problem_message =
            "ruda.mod must contain exactly one module and one language directive";
        target->problem_start = 0;
        target->problem_end = 0;
        status = RUDA_CLI_CHECK_TARGET_SOURCE_INVALID;
        goto cleanup;
    }
    if (!ruda_manifest_decode_string(
            &target->manifest, module->values[0], target->allocator,
            module_path
        ) || !ruda_manifest_decode_string(
            &target->manifest, language_entry->values[0], target->allocator,
            &language
        )) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (module_path->length == 0 || language.length == 0
        || memchr(module_path->data, 0, module_path->length) != NULL
        || memchr(language.data, 0, language.length) != NULL) {
        bool module_invalid = module_path->length == 0
            || memchr(module_path->data, 0, module_path->length) != NULL;
        target->problem_code = RUDA_DIAGNOSTIC_MODULE_IDENTITY;
        target->problem_message =
            "module and language identities must be non-empty and NUL-free";
        target->problem_start = module_invalid
            ? module->values[0].start : language_entry->values[0].start;
        target->problem_end = module_invalid
            ? module->values[0].end : language_entry->values[0].end;
        status = RUDA_CLI_CHECK_TARGET_SOURCE_INVALID;
    }
cleanup:
    ruda_owned_bytes_dispose(&language);
    ruda_owned_bytes_dispose(&bytes);
    ruda_owned_bytes_dispose(&manifest_path);
    return status;
}

static RudaCliCheckPackage *ruda_check_find_package(
    RudaCliCheckTarget *target,
    const RudaOwnedBytes *directory
) {
    size_t index = 0;
    for (index = 0; index < target->packages.length; index += 1) {
        RudaCliCheckPackage *package = ruda_vector_at(
            &target->packages, index
        );
        if (ruda_check_path_equal(&package->directory, directory)) {
            return package;
        }
    }
    return NULL;
}

static RudaCliCheckTargetStatus ruda_check_add_package(
    RudaCliCheckTarget *target,
    const RudaOwnedBytes *module_root,
    const RudaOwnedBytes *module_path,
    const RudaOwnedBytes *directory,
    RudaCliCheckPackage **output
) {
    RudaCliCheckPackage package;
    RudaCliCheckTargetStatus status = RUDA_CLI_CHECK_TARGET_OK;
    memset(&package, 0, sizeof(package));
    if (!ruda_check_path_copy(
            &package.directory, target->allocator,
            directory->data, directory->length
        )) {
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    if (ruda_check_path_equal(module_root, directory)) {
        if (!ruda_owned_bytes_copy(
                &package.import_path, target->allocator,
                module_path->data, module_path->length
            )) {
            status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else {
        RudaOwnedBytes relative;
        size_t relative_start = module_root->length;
        memset(&relative, 0, sizeof(relative));
        if (relative_start < directory->length
            && directory->data[relative_start] == '/') {
            relative_start += 1;
        }
        relative.data = directory->data + relative_start;
        relative.length = directory->length - relative_start;
        status = ruda_check_path_join(
            target->allocator, module_path, relative.data, relative.length,
            &package.import_path
        );
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            goto cleanup;
        }
    }
    if (!ruda_vector_init(
            &package.file_ids, target->allocator, sizeof(RudaFileId)
        ) || !ruda_vector_push(&target->packages, &package)) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(&package, 0, sizeof(package));
    *output = ruda_vector_at(
        &target->packages, target->packages.length - 1
    );
    return RUDA_CLI_CHECK_TARGET_OK;
cleanup:
    ruda_vector_dispose(&package.file_ids);
    ruda_owned_bytes_dispose(&package.import_path);
    ruda_owned_bytes_dispose(&package.directory);
    return status;
}

static RudaCliCheckTargetStatus ruda_check_load_sources(
    RudaCliCheckTarget *target,
    const RudaOwnedBytes *module_root,
    const RudaOwnedBytes *module_path,
    RudaVector *paths
) {
    size_t index = 0;
    for (index = 0; index < paths->length; index += 1) {
        RudaCliCheckPath *source_path = ruda_vector_at(paths, index);
        RudaOwnedBytes directory = {0};
        RudaOwnedBytes source = {0};
        RudaCliCheckPackage *package = NULL;
        RudaFileId file_id = 0;
        RudaCliCheckTargetStatus status;
        if (!ruda_check_path_copy(
                &directory, target->allocator,
                source_path->path.data, source_path->path.length
            )) {
            return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        status = ruda_check_path_parent(&directory);
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            ruda_owned_bytes_dispose(&directory);
            return status;
        }
        package = ruda_check_find_package(target, &directory);
        if (package == NULL) {
            status = ruda_check_add_package(
                target, module_root, module_path, &directory, &package
            );
            if (status != RUDA_CLI_CHECK_TARGET_OK) {
                ruda_owned_bytes_dispose(&directory);
                return status;
            }
        }
        status = ruda_check_file_status(ruda_file_read_all(
            target->allocator, (const char *)source_path->path.data,
            source_path->path.length, RUDA_CLI_CHECK_MAX_SOURCE_BYTES,
            &source
        ));
        if (status == RUDA_CLI_CHECK_TARGET_OK) {
            status = ruda_check_compiler_status(
                ruda_compiler_service_add_document(
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
        if (status == RUDA_CLI_CHECK_TARGET_OK
            && !ruda_vector_push(&package->file_ids, &file_id)) {
            status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        ruda_owned_bytes_dispose(&source);
        ruda_owned_bytes_dispose(&directory);
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            return status;
        }
    }
    return RUDA_CLI_CHECK_TARGET_OK;
}

static RudaCliCheckTargetStatus ruda_check_build_package_views(
    RudaCliCheckTarget *target,
    const RudaOwnedBytes *selected_directory
) {
    size_t index = 0;
    bool selected = target->module_wide;
    for (index = 0; index < target->packages.length; index += 1) {
        RudaCliCheckPackage *package = ruda_vector_at(&target->packages, index);
        RudaCompilerPackage view;
        view.import_path = (const char *)package->import_path.data;
        view.import_path_length = package->import_path.length;
        view.file_ids = package->file_ids.data;
        view.file_count = package->file_ids.length;
        if (!ruda_vector_push(&target->compiler_packages, &view)) {
            return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        if (!target->module_wide
            && ruda_check_path_equal(&package->directory, selected_directory)) {
            if (!ruda_owned_bytes_copy(
                    &target->selected_import_path,
                    target->allocator,
                    package->import_path.data,
                    package->import_path.length
                )) {
                return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            }
            selected = true;
        }
    }
    return selected ? RUDA_CLI_CHECK_TARGET_OK
        : RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
}

static RudaCliCheckTargetStatus ruda_check_build_standalone(
    RudaCliCheckTarget *target,
    const RudaOwnedBytes *path
) {
    static const char package_name[] = "script";
    RudaOwnedBytes source = {0};
    RudaCliCheckPackage package;
    RudaCompilerPackage view;
    RudaFileId file_id = 0;
    RudaCliCheckTargetStatus status = ruda_check_file_status(ruda_file_read_all(
        target->allocator, (const char *)path->data, path->length,
        RUDA_CLI_CHECK_MAX_SOURCE_BYTES, &source
    ));
    memset(&package, 0, sizeof(package));
    memset(&view, 0, sizeof(view));
    if (status == RUDA_CLI_CHECK_TARGET_OK) {
        status = ruda_check_compiler_status(ruda_compiler_service_add_document(
            &target->service, (const char *)path->data, path->length, 1,
            source.data, source.length, &file_id
        ));
    }
    if (status != RUDA_CLI_CHECK_TARGET_OK
        || !ruda_owned_bytes_copy(
            &package.import_path, target->allocator,
            package_name, sizeof(package_name) - 1
        ) || !ruda_owned_bytes_copy(
            &package.directory, target->allocator, path->data, path->length
        ) || !ruda_vector_init(
            &package.file_ids, target->allocator, sizeof(RudaFileId)
        ) || !ruda_vector_push(&package.file_ids, &file_id)
        || !ruda_vector_push(&target->packages, &package)) {
        if (status == RUDA_CLI_CHECK_TARGET_OK) {
            status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
        ruda_vector_dispose(&package.file_ids);
        ruda_owned_bytes_dispose(&package.import_path);
        ruda_owned_bytes_dispose(&package.directory);
        ruda_owned_bytes_dispose(&source);
        return status;
    }
    memset(&package, 0, sizeof(package));
    {
        RudaCliCheckPackage *stored = ruda_vector_at(&target->packages, 0);
        view.import_path = (const char *)stored->import_path.data;
        view.import_path_length = stored->import_path.length;
        view.file_ids = stored->file_ids.data;
        view.file_count = stored->file_ids.length;
        if (!ruda_vector_push(&target->compiler_packages, &view)
            || !ruda_owned_bytes_copy(
                &target->selected_import_path, target->allocator,
                package_name, sizeof(package_name) - 1
            )) {
            status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        }
    }
    ruda_owned_bytes_dispose(&source);
    return status;
}

RudaCliCheckTargetStatus ruda_cli_check_target_build(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaCliCheckTarget *target
) {
    RudaCompilerOptions compiler_options = ruda_compiler_default_options();
    RudaOwnedBytes current = {0};
    RudaOwnedBytes selected = {0};
    RudaOwnedBytes selected_directory = {0};
    RudaOwnedBytes module_root = {0};
    RudaOwnedBytes module_path = {0};
    RudaVector paths;
    RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_DIRECTORY;
    RudaCliCheckTargetStatus status = RUDA_CLI_CHECK_TARGET_OK;
    bool paths_initialized = false;
    memset(target, 0, sizeof(*target));
    target->allocator = session->allocator;
    compiler_options.max_documents = RUDA_CLI_CHECK_MAX_FILES;
    compiler_options.max_path_bytes = RUDA_CLI_CHECK_MAX_PATH_BYTES;
    compiler_options.max_source_bytes = RUDA_CLI_CHECK_MAX_SOURCE_BYTES;
    status = ruda_check_compiler_status(ruda_compiler_service_init(
        &target->service, target->allocator, compiler_options
    ));
    if (status != RUDA_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    target->service_initialized = true;
    if (!ruda_vector_init(
            &target->packages, target->allocator,
            sizeof(RudaCliCheckPackage)
        )) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->packages_initialized = true;
    if (!ruda_vector_init(
            &target->compiler_packages, target->allocator,
            sizeof(RudaCompilerPackage)
        )) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    target->compiler_packages_initialized = true;
    status = ruda_check_file_status(ruda_directory_current(
        target->allocator, RUDA_CLI_CHECK_MAX_PATH_BYTES, &current
    ));
    if (status != RUDA_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    if (options->target == NULL) {
        if (!ruda_check_path_copy(
                &selected, target->allocator, current.data, current.length
            )) {
            status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else {
        status = ruda_check_absolute_path(
            target->allocator, &current, options->target, &selected
        );
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            goto cleanup;
        }
        status = ruda_check_file_status(ruda_directory_kind(
            target->allocator, (const char *)selected.data,
            selected.length, &kind
        ));
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            goto cleanup;
        }
        if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
            if (!ruda_cli_source_path((const char *)selected.data)) {
                status = RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
            } else {
                status = ruda_check_build_standalone(target, &selected);
            }
            goto cleanup;
        }
        if (kind != RUDA_DIRECTORY_ENTRY_DIRECTORY) {
            status = RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
            goto cleanup;
        }
    }
    if (!ruda_check_path_copy(
            &selected_directory, target->allocator,
            selected.data, selected.length
        )) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    status = ruda_check_find_module_root(
        target->allocator, &selected_directory, &module_root
    );
    if (status != RUDA_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    target->module_wide = options->target == NULL
        || ruda_check_path_equal(&selected_directory, &module_root);
    status = ruda_check_manifest_identity(
        target, &module_root, &module_path
    );
    if (status != RUDA_CLI_CHECK_TARGET_OK) {
        goto cleanup;
    }
    if (!ruda_vector_init(&paths, target->allocator, sizeof(RudaCliCheckPath))) {
        status = RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        goto cleanup;
    }
    paths_initialized = true;
    status = ruda_check_collect_sources(
        target->allocator, &module_root, options->check_all_targets, &paths
    );
    if (status == RUDA_CLI_CHECK_TARGET_OK && paths.length == 0) {
        status = RUDA_CLI_CHECK_TARGET_INPUT_ERROR;
    }
    if (status == RUDA_CLI_CHECK_TARGET_OK) {
        status = ruda_check_load_sources(
            target, &module_root, &module_path, &paths
        );
    }
    if (status == RUDA_CLI_CHECK_TARGET_OK) {
        status = ruda_check_build_package_views(
            target, &selected_directory
        );
    }
cleanup:
    if (paths_initialized) {
        ruda_check_paths_dispose(&paths);
    }
    ruda_owned_bytes_dispose(&module_path);
    ruda_owned_bytes_dispose(&module_root);
    ruda_owned_bytes_dispose(&selected_directory);
    ruda_owned_bytes_dispose(&selected);
    ruda_owned_bytes_dispose(&current);
    return status;
}

void ruda_cli_check_target_dispose(RudaCliCheckTarget *target) {
    size_t index = 0;
    if (target == NULL) {
        return;
    }
    ruda_manifest_parse_result_dispose(&target->manifest);
    if (target->manifest_sources_initialized) {
        ruda_source_manager_dispose(&target->manifest_sources);
    }
    ruda_owned_bytes_dispose(&target->selected_import_path);
    if (target->compiler_packages_initialized) {
        ruda_vector_dispose(&target->compiler_packages);
    }
    if (target->packages_initialized) {
        for (index = 0; index < target->packages.length; index += 1) {
            RudaCliCheckPackage *package = ruda_vector_at(
                &target->packages, index
            );
            ruda_vector_dispose(&package->file_ids);
            ruda_owned_bytes_dispose(&package->directory);
            ruda_owned_bytes_dispose(&package->import_path);
        }
        ruda_vector_dispose(&target->packages);
    }
    if (target->service_initialized) {
        ruda_compiler_service_dispose(&target->service);
    }
    memset(target, 0, sizeof(*target));
}
