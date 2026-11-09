#include "workspace.h"
#include "positions.h"
#include "analysis_overrides.h"
#include "analysis_cache.h"
#include "source_locations.h"
#include "intrinsic_queries.h"

#include <string.h>

#include "../cli/cli_standard_packages.h"
#include "mmay/base/utf8.h"

enum {
    LSP_MAX_DOCUMENTS = 64,
    LSP_MAX_SOURCE_BYTES = 16 * 1024 * 1024,
    LSP_MAX_TOTAL_SOURCE_BYTES = 64 * 1024 * 1024
};

static bool lsp_bytes_equal(const MarimayOwnedBytes *bytes,
    const void *other, size_t length) {
    return bytes->length == length
        && (length == 0 || memcmp(bytes->data, other, length) == 0);
}

static void lsp_document_dispose(LspDocument *document) {
    mmay_owned_bytes_dispose(&document->uri);
    mmay_owned_bytes_dispose(&document->path);
    mmay_owned_bytes_dispose(&document->text);
    memset(document, 0, sizeof(*document));
}

bool lsp_workspace_init(LspWorkspace *workspace, MarimayAllocator allocator) {
    if (workspace == NULL || workspace->initialized || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) return false;
    memset(workspace, 0, sizeof(*workspace));
    workspace->allocator = allocator;
    if (!mmay_vector_init(&workspace->documents, allocator, sizeof(LspDocument)))
        return false;
    workspace->initialized = true;
    return true;
}

void lsp_workspace_dispose(LspWorkspace *workspace) {
    size_t index;
    if (workspace == NULL) return;
    lsp_workspace_invalidate(workspace);
    for (index = 0; index < workspace->documents.length; index += 1)
        lsp_document_dispose(mmay_vector_at(&workspace->documents, index));
    mmay_vector_dispose(&workspace->documents);
    memset(workspace, 0, sizeof(*workspace));
}

const LspDocument *lsp_workspace_document(const LspWorkspace *workspace,
    const char *uri, size_t uri_length) {
    size_t index;
    if (workspace == NULL || !workspace->initialized || uri == NULL) return NULL;
    for (index = 0; index < workspace->documents.length; index += 1) {
        const LspDocument *document = mmay_vector_at_const(&workspace->documents, index);
        if (lsp_bytes_equal(&document->uri, uri, uri_length)) return document;
    }
    return NULL;
}

LspStatus lsp_workspace_change(LspWorkspace *workspace,
    const char *uri, size_t uri_length, int64_t version,
    const void *text, size_t text_length) {
    size_t index;
    if (workspace == NULL || !workspace->initialized || uri == NULL
        || (text == NULL && text_length != 0)) return LSP_INVALID;
    for (index = 0; index < workspace->documents.length; index += 1) {
        LspDocument *document = mmay_vector_at(&workspace->documents, index);
        MarimayOwnedBytes replacement = {0};
        size_t new_total;
        if (!lsp_bytes_equal(&document->uri, uri, uri_length)) continue;
        if (version <= document->version) return LSP_STALE;
        if (text_length > LSP_MAX_SOURCE_BYTES || !mmay_size_add(
                workspace->total_source_bytes - document->text.length,
                text_length, &new_total) || new_total > LSP_MAX_TOTAL_SOURCE_BYTES)
            return LSP_LIMIT;
        if (!mmay_utf8_validate(text, text_length, NULL)) return LSP_INVALID;
        if (!mmay_owned_bytes_copy(&replacement, workspace->allocator, text, text_length))
            return LSP_ALLOCATION_FAILED;
        mmay_owned_bytes_dispose(&document->text);
        if (!lsp_source_is_view((const char *)document->path.data, document->path.length))
            lsp_workspace_invalidate(workspace);
        document->text = replacement;
        document->version = version;
        workspace->total_source_bytes = new_total;
        return LSP_OK;
    }
    return LSP_INVALID;
}

LspStatus lsp_workspace_open(LspWorkspace *workspace,
    const char *uri, size_t uri_length, int64_t version,
    const void *text, size_t text_length) {
    LspDocument document = {0};
    LspStatus status;
    size_t index, new_total;
    if (workspace == NULL || !workspace->initialized || uri == NULL
        || (text == NULL && text_length != 0)) return LSP_INVALID;
    if (lsp_workspace_document(workspace, uri, uri_length) != NULL)
        return LSP_INVALID;
    if (workspace->documents.length >= LSP_MAX_DOCUMENTS
        || text_length > LSP_MAX_SOURCE_BYTES
        || !mmay_size_add(workspace->total_source_bytes, text_length, &new_total)
        || new_total > LSP_MAX_TOTAL_SOURCE_BYTES) return LSP_LIMIT;
    if (!mmay_utf8_validate(text, text_length, NULL)) return LSP_INVALID;
    status = lsp_uri_to_path(workspace->allocator, uri, uri_length, &document.path);
    if (status != LSP_OK) return status;
    if (document.path.length < 3
        || memcmp(document.path.data + document.path.length - 3, ".my", 3) != 0) {
        status = LSP_INVALID;
        goto cleanup;
    }
    for (index = 0; index < workspace->documents.length; index += 1) {
        const LspDocument *existing = mmay_vector_at_const(&workspace->documents, index);
        if (lsp_bytes_equal(&existing->path, document.path.data, document.path.length)) {
            status = LSP_INVALID;
            goto cleanup;
        }
    }
    if (!mmay_owned_bytes_copy(&document.uri, workspace->allocator, uri, uri_length)
        || !mmay_owned_bytes_copy(&document.text, workspace->allocator, text, text_length)) {
        status = LSP_ALLOCATION_FAILED;
        goto cleanup;
    }
    document.version = version;
    if (!mmay_vector_push(&workspace->documents, &document)) {
        status = LSP_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!lsp_source_is_view((const char *)document.path.data, document.path.length))
        lsp_workspace_invalidate(workspace);
    workspace->total_source_bytes = new_total;
    return LSP_OK;
cleanup:
    lsp_document_dispose(&document);
    return status;
}

LspStatus lsp_workspace_close(LspWorkspace *workspace,
    const char *uri, size_t uri_length) {
    size_t index;
    if (workspace == NULL || !workspace->initialized || uri == NULL) return LSP_INVALID;
    for (index = 0; index < workspace->documents.length; index += 1) {
        LspDocument *document = mmay_vector_at(&workspace->documents, index);
        if (!lsp_bytes_equal(&document->uri, uri, uri_length)) continue;
        lsp_workspace_invalidate(workspace);
        workspace->total_source_bytes -= document->text.length;
        lsp_document_dispose(document);
        if (index + 1 < workspace->documents.length)
            memmove(document, document + 1,
                (workspace->documents.length - index - 1) * sizeof(*document));
        workspace->documents.length -= 1;
        return LSP_OK;
    }
    return LSP_INVALID;
}

static LspStatus lsp_target_status(MarimayCliCheckTargetStatus status) {
    if (status == MMAY_CLI_CHECK_TARGET_OK) return LSP_OK;
    if (status == MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED) return LSP_ALLOCATION_FAILED;
    if (status == MMAY_CLI_CHECK_TARGET_LIMIT_REACHED) return LSP_LIMIT;
    if (status == MMAY_CLI_CHECK_TARGET_INPUT_ERROR
        || status == MMAY_CLI_CHECK_TARGET_SOURCE_INVALID) return LSP_INPUT_ERROR;
    return LSP_INTERNAL_ERROR;
}

static LspStatus lsp_compiler_status(MarimayCompilerStatus status) {
    if (status == MMAY_COMPILER_OK || status == MMAY_COMPILER_INCOMPLETE_LANGUAGE)
        return LSP_OK;
    if (status == MMAY_COMPILER_ALLOCATION_FAILED) return LSP_ALLOCATION_FAILED;
    if (status == MMAY_COMPILER_LIMIT_REACHED) return LSP_LIMIT;
    return LSP_INTERNAL_ERROR;
}

static size_t lsp_parent_length(const unsigned char *path, size_t length) {
    while (length != 0 && path[length - 1] != '/') length -= 1;
    if (length == 0) return 0;
    if (length == 1 || (length == 3 && path[1] == ':')) return length;
    return length - 1;
}

static LspStatus lsp_package_views(MarimayCliCheckTarget *target) {
    size_t index;
    target->compiler_packages.length = 0;
    for (index = 0; index < target->packages.length; index += 1) {
        const MarimayCliCheckPackage *package = mmay_vector_at_const(&target->packages, index);
        MarimayCompilerPackage view;
        view.import_path = (const char *)package->import_path.data;
        view.import_path_length = package->import_path.length;
        view.file_ids = package->file_ids.data;
        view.file_count = package->file_ids.length;
        if (!mmay_vector_push(&target->compiler_packages, &view))
            return LSP_ALLOCATION_FAILED;
    }
    return LSP_OK;
}

static LspStatus lsp_memory_target(MarimayCliCheckTarget *target,
    MarimayAllocator allocator, const LspDocument *document) {
    MarimayCliCheckPackage package = {0};
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayFileId file = 0;
    LspStatus status;
    size_t parent_length = lsp_parent_length(document->path.data, document->path.length);
    memset(target, 0, sizeof(*target));
    target->allocator = allocator;
    options.max_path_bytes = MMAY_CLI_CHECK_MAX_PATH_BYTES;
    status = lsp_compiler_status(mmay_compiler_service_init(&target->service, allocator, options));
    if (status != LSP_OK) return status;
    target->service_initialized = true;
    target->packages_initialized = mmay_vector_init(&target->packages, allocator,
        sizeof(MarimayCliCheckPackage));
    target->compiler_packages_initialized = mmay_vector_init(&target->compiler_packages,
        allocator, sizeof(MarimayCompilerPackage));
    if (!target->packages_initialized || !target->compiler_packages_initialized
        || !mmay_owned_bytes_copy(&target->root_directory, allocator,
            document->path.data, parent_length)
        || !mmay_owned_bytes_copy(&target->selected_import_path, allocator, "script", 6)
        || !mmay_owned_bytes_copy(&package.import_path, allocator, "script", 6)
        || !mmay_owned_bytes_copy(&package.directory, allocator,
            document->path.data, parent_length)
        || !mmay_vector_init(&package.file_ids, allocator, sizeof(MarimayFileId))) {
        status = LSP_ALLOCATION_FAILED;
        goto cleanup;
    }
    status = lsp_compiler_status(mmay_compiler_service_add_document(&target->service,
        (const char *)document->path.data, document->path.length, 1,
        document->text.data, document->text.length, &file));
    if (status != LSP_OK) goto cleanup;
    if (!mmay_vector_push(&package.file_ids, &file)
        || !mmay_vector_push(&target->packages, &package)) {
        status = LSP_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(&package, 0, sizeof(package));
    status = lsp_target_status(mmay_cli_standard_packages_add(target));
    if (status == LSP_OK) status = lsp_package_views(target);
cleanup:
    mmay_vector_dispose(&package.file_ids);
    mmay_owned_bytes_dispose(&package.import_path);
    mmay_owned_bytes_dispose(&package.directory);
    return status;
}

static LspStatus lsp_create_module_package(MarimayCliCheckTarget *target,
    const LspDocument *document, size_t parent_length,
    MarimayCliCheckPackage **output) {
    MarimayCliCheckPackage package = {0};
    MarimayOwnedBytes module = {0};
    MarimayVector import_path = {0};
    size_t index, relative;
    LspStatus status = LSP_INPUT_ERROR;
    if (!target->manifest.initialized || target->root_directory.length > parent_length
        || memcmp(target->root_directory.data, document->path.data,
            target->root_directory.length) != 0
        || (target->root_directory.length < parent_length
            && document->path.data[target->root_directory.length] != '/')) return status;
    for (index = 0; index < target->manifest.entries.length; index += 1) {
        const MarimayManifestEntry *entry = mmay_manifest_entry(&target->manifest, index);
        if (entry->kind == MMAY_MANIFEST_DIRECTIVE_MODULE) {
            if (!mmay_manifest_decode_string(&target->manifest, entry->values[0],
                    target->allocator, &module)) return LSP_ALLOCATION_FAILED;
            break;
        }
    }
    if (module.data == NULL) return status;
    relative = target->root_directory.length;
    if (relative < parent_length && document->path.data[relative] == '/') relative += 1;
    status = LSP_ALLOCATION_FAILED;
    if (!mmay_vector_init(&import_path, target->allocator, 1)
        || !mmay_vector_append(&import_path, module.data, module.length)
        || (relative < parent_length && (!mmay_vector_append(&import_path, "/", 1)
            || !mmay_vector_append(&import_path, document->path.data + relative,
                parent_length - relative)))
        || !mmay_owned_bytes_copy(&package.import_path, target->allocator,
            import_path.data, import_path.length)
        || !mmay_owned_bytes_copy(&package.directory, target->allocator,
            document->path.data, parent_length)
        || !mmay_vector_init(&package.file_ids, target->allocator, sizeof(MarimayFileId))
        || !mmay_vector_push(&target->packages, &package)) goto cleanup;
    memset(&package, 0, sizeof(package));
    *output = mmay_vector_at(&target->packages, target->packages.length - 1);
    status = LSP_OK;
cleanup:
    mmay_vector_dispose(&package.file_ids);
    mmay_owned_bytes_dispose(&package.import_path);
    mmay_owned_bytes_dispose(&package.directory);
    mmay_owned_bytes_dispose(&module);
    mmay_vector_dispose(&import_path);
    return status;
}

static LspStatus lsp_add_new_document(MarimayCliCheckTarget *target,
    const LspDocument *document, bool required) {
    size_t parent_length = lsp_parent_length(document->path.data, document->path.length);
    size_t index;
    MarimayCliCheckPackage *package = NULL;
    MarimayFileId file = 0;
    LspStatus status;
    for (index = 0; index < target->packages.length; index += 1) {
        MarimayCliCheckPackage *candidate = mmay_vector_at(&target->packages, index);
        if (lsp_bytes_equal(&candidate->directory, document->path.data, parent_length)) {
            package = candidate;
            break;
        }
    }
    if (package == NULL) {
        status = lsp_create_module_package(target, document, parent_length, &package);
        if (status != LSP_OK) return required || status != LSP_INPUT_ERROR ? status : LSP_OK;
    }
    if (required && target->selected_import_path.length == 0
        && !mmay_owned_bytes_copy(&target->selected_import_path, target->allocator,
            package->import_path.data, package->import_path.length))
        return LSP_ALLOCATION_FAILED;
    status = lsp_compiler_status(mmay_compiler_service_add_document(&target->service,
        (const char *)document->path.data, document->path.length, 2,
        document->text.data, document->text.length, &file));
    if (status != LSP_OK) return status;
    if (!mmay_vector_push(&package->file_ids, &file)) return LSP_ALLOCATION_FAILED;
    return LSP_OK;
}

static LspStatus lsp_apply_documents(const LspWorkspace *workspace,
    const LspDocument *selected, MarimayCliCheckTarget *target) {
    size_t index;
    for (index = 0; index < workspace->documents.length; index += 1) {
        const LspDocument *document = mmay_vector_at_const(&workspace->documents, index);
        size_t source_index;
        bool found = false;
        if (lsp_source_is_view((const char *)document->path.data, document->path.length)) continue;
        for (source_index = 0; source_index < target->service.documents.length;
            source_index += 1) {
            const MarimayCompilerDocument *source = mmay_vector_at_const(
                &target->service.documents, source_index);
            if (document->path.length == mmay_source_path_length(source->snapshot)
                && memcmp(document->path.data, mmay_source_path(source->snapshot),
                    document->path.length) == 0) {
                LspStatus status = lsp_compiler_status(mmay_compiler_service_overlay(
                    &target->service, source->file_id, source->version + 1,
                    document->text.data, document->text.length));
                if (status != LSP_OK) return status;
                found = true;
                break;
            }
        }
        if (!found) {
            /* Standalone scripts do not implicitly merge other open scripts. */
            if (document != selected && !target->manifest.initialized) continue;
            LspStatus status = lsp_add_new_document(target, document, document == selected);
            if (status != LSP_OK) return status;
        }
    }
    return lsp_package_views(target);
}

LspStatus lsp_workspace_analyze_overrides(const LspWorkspace *workspace,
    const char *uri, size_t uri_length, const LspSourceOverride *overrides,
    size_t override_count, bool whole_module, LspAnalysis *analysis) {
    const LspDocument *document = lsp_workspace_document(workspace, uri, uri_length);
    MarimayOwnedBytes parent = {0};
    MarimaySession session;
    MarimayCliOptions options = {0};
    LspStatus status;
    if (document == NULL || analysis == NULL) return LSP_INVALID;
    memset(analysis, 0, sizeof(*analysis));
    if (!mmay_owned_bytes_copy(&parent, workspace->allocator, document->path.data,
            lsp_parent_length(document->path.data, document->path.length)))
        return LSP_ALLOCATION_FAILED;
    mmay_session_init(&session, workspace->allocator);
    options.target = (const char *)parent.data;
    options.check_all_targets = true;
    analysis->target_status = mmay_cli_check_target_build(&session, &options, &analysis->target);
    /* Discovery can finish the graph before noticing a new empty package. */
    if (analysis->target_status == MMAY_CLI_CHECK_TARGET_INPUT_ERROR
        && analysis->target.compiler_packages.length != 0
        && analysis->target.manifest.initialized
        && analysis->target.manifest.diagnostics.length == 0
        && analysis->target.selected_import_path.length == 0)
        analysis->target_status = MMAY_CLI_CHECK_TARGET_OK;
    if (analysis->target_status == MMAY_CLI_CHECK_TARGET_INPUT_ERROR) {
        mmay_cli_check_target_dispose(&analysis->target);
        options.target = (const char *)document->path.data;
        analysis->target_status = mmay_cli_check_target_build(&session, &options,
            &analysis->target);
        if (analysis->target_status == MMAY_CLI_CHECK_TARGET_INPUT_ERROR) {
            mmay_cli_check_target_dispose(&analysis->target);
            status = lsp_memory_target(&analysis->target, workspace->allocator, document);
            if (status != LSP_OK) goto cleanup;
            analysis->target_status = MMAY_CLI_CHECK_TARGET_OK;
        }
    }
    status = lsp_target_status(analysis->target_status);
    if (status != LSP_OK) goto cleanup;
    status = lsp_apply_documents(workspace, document, &analysis->target);
    if (status != LSP_OK) goto cleanup;
    status = lsp_apply_source_overrides(&analysis->target, overrides, override_count);
    if (status != LSP_OK) goto cleanup;
    if (whole_module && analysis->target.manifest.initialized)
        analysis->target.module_wide = true;
    analysis->compiler_status = analysis->target.module_wide
        ? mmay_compiler_service_check_module_graph(&analysis->target.service,
            analysis->target.compiler_packages.data,
            analysis->target.compiler_packages.length, &analysis->result)
        : mmay_compiler_service_check_graph(&analysis->target.service,
            analysis->target.compiler_packages.data,
            analysis->target.compiler_packages.length,
            (const char *)analysis->target.selected_import_path.data,
            analysis->target.selected_import_path.length, &analysis->result);
    status = lsp_compiler_status(analysis->compiler_status);
    if (status == LSP_OK) status = lsp_intrinsic_views_init(analysis);
cleanup:
    mmay_owned_bytes_dispose(&parent);
    return status;
}

LspStatus lsp_workspace_analyze(const LspWorkspace *workspace,
    const char *uri, size_t uri_length, LspAnalysis *analysis) {
    return lsp_workspace_analyze_overrides(workspace, uri, uri_length,
        NULL, 0, false, analysis);
}

void lsp_analysis_dispose(LspAnalysis *analysis) {
    if (analysis == NULL) return;
    if (analysis->intrinsic_sources_initialized)
        mmay_source_manager_dispose(&analysis->intrinsic_sources);
    mmay_compiler_result_dispose(&analysis->result);
    mmay_cli_check_target_dispose(&analysis->target);
    memset(analysis, 0, sizeof(*analysis));
}

const MarimaySourceSnapshot *lsp_analysis_snapshot(const LspAnalysis *analysis,
    const char *path, size_t path_length) {
    size_t index;
    if (analysis == NULL || path == NULL) return NULL;
    for (index = 0; index < analysis->result.units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&analysis->result, index);
        if (unit != NULL && path_length == mmay_source_path_length(unit->snapshot)
            && memcmp(path, mmay_source_path(unit->snapshot), path_length) == 0)
            return unit->snapshot;
    }
    return NULL;
}
