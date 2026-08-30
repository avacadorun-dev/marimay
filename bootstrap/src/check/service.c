#include "mmay/check/service.h"

#include <stdlib.h>
#include <string.h>

typedef struct MarimayCompilerSemanticTarget {
    const char *package_name;
    const MarimayCompilerPackage *packages;
    size_t package_count;
    const char *root_import_path;
    size_t root_import_path_length;
    bool is_graph;
    bool all_packages;
} MarimayCompilerSemanticTarget;

static bool mmay_compiler_allocator_valid(MarimayAllocator allocator) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL;
}

static MarimayCompilerDocument *mmay_compiler_find_document(
    MarimayCompilerService *service,
    MarimayFileId file_id
) {
    size_t index = 0;
    if (service == NULL) {
        return NULL;
    }
    for (index = 0; index < service->documents.length; index += 1) {
        MarimayCompilerDocument *document = mmay_vector_at(&service->documents, index);
        if (document != NULL && document->file_id == file_id) {
            return document;
        }
    }
    return NULL;
}

static const MarimayCompilerDocument *mmay_compiler_find_document_const(
    const MarimayCompilerService *service,
    MarimayFileId file_id
) {
    size_t index = 0;
    if (service == NULL) {
        return NULL;
    }
    for (index = 0; index < service->documents.length; index += 1) {
        const MarimayCompilerDocument *document = mmay_vector_at_const(
            &service->documents,
            index
        );
        if (document != NULL && document->file_id == file_id) {
            return document;
        }
    }
    return NULL;
}

static int mmay_compiler_compare_document_paths(
    const void *left_pointer,
    const void *right_pointer
) {
    const MarimayCompilerDocument *left = *(const MarimayCompilerDocument *const *)left_pointer;
    const MarimayCompilerDocument *right = *(const MarimayCompilerDocument *const *)right_pointer;
    const char *left_path = mmay_source_path(left->snapshot);
    const char *right_path = mmay_source_path(right->snapshot);
    size_t left_length = mmay_source_path_length(left->snapshot);
    size_t right_length = mmay_source_path_length(right->snapshot);
    size_t shared = left_length < right_length ? left_length : right_length;
    int comparison = shared == 0 ? 0 : memcmp(left_path, right_path, shared);
    if (comparison != 0) {
        return comparison;
    }
    if (left_length < right_length) {
        return -1;
    }
    if (left_length > right_length) {
        return 1;
    }
    return 0;
}

static MarimayCompilerStatus mmay_compiler_source_status(MarimaySourceStatus status) {
    switch (status) {
        case MMAY_SOURCE_OK:
            return MMAY_COMPILER_OK;
        case MMAY_SOURCE_INVALID_ARGUMENT:
            return MMAY_COMPILER_INVALID_ARGUMENT;
        case MMAY_SOURCE_ALLOCATION_FAILED:
            return MMAY_COMPILER_ALLOCATION_FAILED;
        case MMAY_SOURCE_DUPLICATE_PATH:
            return MMAY_COMPILER_DUPLICATE_PATH;
        case MMAY_SOURCE_UNKNOWN_FILE:
            return MMAY_COMPILER_UNKNOWN_DOCUMENT;
        case MMAY_SOURCE_ID_EXHAUSTED:
            return MMAY_COMPILER_LIMIT_REACHED;
    }
    return MMAY_COMPILER_INTERNAL_ERROR;
}

static MarimayCompilerStatus mmay_compiler_lex_status(MarimayLexStatus status) {
    switch (status) {
        case MMAY_LEX_OK:
            return MMAY_COMPILER_OK;
        case MMAY_LEX_INVALID_ARGUMENT:
            return MMAY_COMPILER_INVALID_ARGUMENT;
        case MMAY_LEX_ALLOCATION_FAILED:
            return MMAY_COMPILER_ALLOCATION_FAILED;
        case MMAY_LEX_LIMIT_REACHED:
            return MMAY_COMPILER_LIMIT_REACHED;
    }
    return MMAY_COMPILER_INTERNAL_ERROR;
}

static MarimayCompilerStatus mmay_compiler_parse_status(MarimayParseStatus status) {
    switch (status) {
        case MMAY_PARSE_OK:
            return MMAY_COMPILER_OK;
        case MMAY_PARSE_INVALID_ARGUMENT:
            return MMAY_COMPILER_INVALID_ARGUMENT;
        case MMAY_PARSE_ALLOCATION_FAILED:
            return MMAY_COMPILER_ALLOCATION_FAILED;
        case MMAY_PARSE_LIMIT_REACHED:
            return MMAY_COMPILER_LIMIT_REACHED;
    }
    return MMAY_COMPILER_INTERNAL_ERROR;
}

static MarimayCompilerStatus mmay_compiler_ast_status(MarimayAstStatus status) {
    switch (status) {
        case MMAY_AST_OK:
            return MMAY_COMPILER_OK;
        case MMAY_AST_INVALID_ARGUMENT:
            return MMAY_COMPILER_INVALID_ARGUMENT;
        case MMAY_AST_ALLOCATION_FAILED:
            return MMAY_COMPILER_ALLOCATION_FAILED;
        case MMAY_AST_LIMIT_REACHED:
            return MMAY_COMPILER_LIMIT_REACHED;
    }
    return MMAY_COMPILER_INTERNAL_ERROR;
}

static MarimayCompilerStatus mmay_compiler_check_status(MarimayCheckStatus status) {
    switch (status) {
        case MMAY_CHECK_OK:
            return MMAY_COMPILER_OK;
        case MMAY_CHECK_INVALID_ARGUMENT:
            return MMAY_COMPILER_INVALID_ARGUMENT;
        case MMAY_CHECK_ALLOCATION_FAILED:
            return MMAY_COMPILER_ALLOCATION_FAILED;
        case MMAY_CHECK_LIMIT_REACHED:
            return MMAY_COMPILER_LIMIT_REACHED;
        case MMAY_CHECK_INCOMPLETE_LANGUAGE:
            return MMAY_COMPILER_INCOMPLETE_LANGUAGE;
        case MMAY_CHECK_INTERNAL_ERROR:
            return MMAY_COMPILER_INTERNAL_ERROR;
    }
    return MMAY_COMPILER_INTERNAL_ERROR;
}

static void mmay_compiler_unit_dispose(MarimayCompilerUnit *unit) {
    if (unit == NULL) {
        return;
    }
    mmay_ast_tree_dispose(&unit->ast);
    mmay_parse_result_dispose(&unit->parsed);
    mmay_lex_result_dispose(&unit->lexed);
    memset(unit, 0, sizeof(*unit));
}

static MarimayCompilerStatus mmay_compiler_result_fail(
    MarimayCompilerResult *result,
    MarimayCompilerStatus status
) {
    mmay_compiler_result_dispose(result);
    return status;
}

static bool mmay_compiler_request_cancelled(
    const MarimayCompilerCheckRequest *request
) {
    return request != NULL && request->cancel_poll != NULL
        && request->cancel_poll(request->cancel_context);
}

static const MarimayCompilerUnit *mmay_compiler_find_unit(
    const MarimayCompilerResult *result,
    MarimayFileId file_id
) {
    size_t index = 0;
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    for (index = 0; index < result->units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_vector_at_const(
            &result->units,
            index
        );
        if (unit != NULL && unit->file_id == file_id) {
            return unit;
        }
    }
    return NULL;
}

static bool mmay_compiler_graph_target_valid(
    const MarimayCompilerService *service,
    const MarimayCompilerSemanticTarget *target
) {
    MarimayCheckGraphOptions graph_options = mmay_check_graph_default_options();
    size_t package_index = 0;
    size_t total_files = 0;
    if (service == NULL || target == NULL || target->packages == NULL
        || target->package_count == 0
        || target->package_count > graph_options.max_packages
        || (!target->all_packages && (target->root_import_path == NULL
            || target->root_import_path_length == 0))) {
        return false;
    }
    for (package_index = 0; package_index < target->package_count;
        package_index += 1) {
        const MarimayCompilerPackage *package = &target->packages[package_index];
        size_t file_index = 0;
        if (package->import_path == NULL || package->import_path_length == 0
            || package->import_path_length
                > service->options.check_options.max_package_name_bytes
            || memchr(
                package->import_path,
                0,
                package->import_path_length
            ) != NULL || package->file_ids == NULL
            || package->file_count == 0
            || !mmay_size_add(total_files, package->file_count, &total_files)) {
            return false;
        }
        for (file_index = 0; file_index < package->file_count;
            file_index += 1) {
            size_t earlier_package = 0;
            if (mmay_compiler_find_document_const(
                    service,
                    package->file_ids[file_index]
                ) == NULL) {
                return false;
            }
            for (earlier_package = 0;
                earlier_package <= package_index; earlier_package += 1) {
                const MarimayCompilerPackage *earlier =
                    &target->packages[earlier_package];
                size_t earlier_file = 0;
                size_t limit = earlier_package == package_index
                    ? file_index : earlier->file_count;
                for (earlier_file = 0; earlier_file < limit;
                    earlier_file += 1) {
                    if (earlier->file_ids[earlier_file]
                        == package->file_ids[file_index]) {
                        return false;
                    }
                }
            }
        }
    }
    return total_files == service->documents.length;
}

static MarimayCompilerStatus mmay_compiler_check_graph_semantics(
    const MarimayCompilerService *service,
    const MarimayCompilerSemanticTarget *target,
    MarimayCompilerResult *built
) {
    MarimayVector graph_asts;
    MarimayVector graph_inputs;
    MarimayCheckGraphOptions options = mmay_check_graph_default_options();
    MarimayCheckStatus status = MMAY_CHECK_INTERNAL_ERROR;
    size_t package_index = 0;
    bool asts_initialized = false;
    bool inputs_initialized = false;
    memset(&graph_asts, 0, sizeof(graph_asts));
    memset(&graph_inputs, 0, sizeof(graph_inputs));
    asts_initialized = mmay_vector_init(
        &graph_asts,
        service->allocator,
        sizeof(MarimayAstTree *)
    );
    inputs_initialized = mmay_vector_init(
        &graph_inputs,
        service->allocator,
        sizeof(MarimayCheckPackageInput)
    );
    if (!asts_initialized || !inputs_initialized
        || !mmay_vector_reserve(&graph_asts, built->units.length)
        || !mmay_vector_reserve(&graph_inputs, target->package_count)) {
        if (inputs_initialized) {
            mmay_vector_dispose(&graph_inputs);
        }
        if (asts_initialized) {
            mmay_vector_dispose(&graph_asts);
        }
        return MMAY_COMPILER_ALLOCATION_FAILED;
    }
    for (package_index = 0; package_index < target->package_count;
        package_index += 1) {
        const MarimayCompilerPackage *package = &target->packages[package_index];
        MarimayCheckPackageInput input;
        size_t ast_start = graph_asts.length;
        size_t file_index = 0;
        memset(&input, 0, sizeof(input));
        for (file_index = 0; file_index < package->file_count;
            file_index += 1) {
            const MarimayCompilerUnit *unit = mmay_compiler_find_unit(
                built,
                package->file_ids[file_index]
            );
            const MarimayAstTree *ast = unit == NULL ? NULL : &unit->ast;
            if (ast == NULL || !mmay_vector_push(&graph_asts, &ast)) {
                mmay_vector_dispose(&graph_inputs);
                mmay_vector_dispose(&graph_asts);
                return ast == NULL ? MMAY_COMPILER_INTERNAL_ERROR
                    : MMAY_COMPILER_ALLOCATION_FAILED;
            }
        }
        input.import_path = package->import_path;
        input.import_path_length = package->import_path_length;
        input.asts = (const MarimayAstTree *const *)graph_asts.data + ast_start;
        input.ast_count = package->file_count;
        if (!mmay_vector_push(&graph_inputs, &input)) {
            mmay_vector_dispose(&graph_inputs);
            mmay_vector_dispose(&graph_asts);
            return MMAY_COMPILER_ALLOCATION_FAILED;
        }
    }
    options.package_options = service->options.check_options;
    status = target->all_packages
        ? mmay_check_module_graph(
            graph_inputs.data,
            graph_inputs.length,
            service->allocator,
            options,
            &built->checked
        )
        : mmay_check_package_graph(
            graph_inputs.data,
            graph_inputs.length,
            target->root_import_path,
            target->root_import_path_length,
            service->allocator,
            options,
            &built->checked
        );
    mmay_vector_dispose(&graph_inputs);
    mmay_vector_dispose(&graph_asts);
    return mmay_compiler_check_status(status);
}

MarimayCompilerOptions mmay_compiler_default_options(void) {
    MarimayCompilerOptions options;
    options.max_documents = 1024;
    options.max_snapshots_per_document = 1024;
    options.max_total_snapshots = 4096;
    options.max_path_bytes = 4096;
    options.max_source_bytes = 16U * 1024U * 1024U;
    options.max_total_source_bytes = 256U * 1024U * 1024U;
    options.lex_options = mmay_lex_default_options();
    options.parse_options = mmay_parse_default_options();
    options.ast_options = mmay_ast_default_options();
    options.check_options = mmay_check_default_options();
    return options;
}

MarimayCompilerStatus mmay_compiler_service_init(
    MarimayCompilerService *service,
    MarimayAllocator allocator,
    MarimayCompilerOptions options
) {
    MarimayCompilerService candidate;
    MarimayCompilerService *built = &candidate;
    if (service == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    memset(built, 0, sizeof(*built));
    if (!mmay_compiler_allocator_valid(allocator) || options.max_documents == 0
        || options.max_snapshots_per_document == 0
        || options.max_total_snapshots < options.max_documents
        || options.max_path_bytes == 0 || options.max_source_bytes == 0
        || options.max_total_source_bytes == 0
        || options.max_documents > options.check_options.max_source_files
        || !mmay_lex_options_valid(options.lex_options)
        || !mmay_parse_options_valid(options.parse_options)
        || !mmay_ast_options_valid(options.ast_options)
        || !mmay_check_options_valid(allocator, options.check_options)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    built->allocator = allocator;
    built->options = options;
    built->generation = 1;
    if (!mmay_source_manager_init(&built->sources, allocator)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    if (!mmay_vector_init(
            &built->documents,
            allocator,
            sizeof(MarimayCompilerDocument)
        )) {
        mmay_source_manager_dispose(&built->sources);
        memset(built, 0, sizeof(*built));
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    built->initialized = true;
    mmay_compiler_service_dispose(service);
    *service = *built;
    memset(built, 0, sizeof(*built));
    return MMAY_COMPILER_OK;
}

void mmay_compiler_service_dispose(MarimayCompilerService *service) {
    if (service == NULL) {
        return;
    }
    mmay_vector_dispose(&service->documents);
    mmay_source_manager_dispose(&service->sources);
    memset(service, 0, sizeof(*service));
}

MarimayCompilerStatus mmay_compiler_service_add_document(
    MarimayCompilerService *service,
    const char *path,
    size_t path_length,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    MarimayFileId *file_id
) {
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayCompilerDocument document;
    MarimaySourceStatus source_status;
    size_t retained_source_bytes = 0;
    if (file_id != NULL) {
        *file_id = 0;
    }
    memset(&document, 0, sizeof(document));
    if (service == NULL || !service->initialized || path == NULL
        || path_length == 0 || (source_bytes == NULL && source_length != 0)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    if (service->documents.length >= service->options.max_documents
        || service->generation == UINT64_MAX
        || service->retained_snapshot_count
            >= service->options.max_total_snapshots
        || path_length > service->options.max_path_bytes
        || source_length > service->options.max_source_bytes
        || !mmay_size_add(
            service->retained_source_bytes,
            source_length,
            &retained_source_bytes
        )
        || retained_source_bytes > service->options.max_total_source_bytes) {
        return MMAY_COMPILER_LIMIT_REACHED;
    }
    if (!mmay_vector_reserve(
            &service->documents,
            service->documents.length + 1
        )) {
        return MMAY_COMPILER_ALLOCATION_FAILED;
    }
    source_status = mmay_source_manager_add(
        &service->sources,
        path,
        path_length,
        source_bytes,
        source_length,
        &snapshot
    );
    if (source_status != MMAY_SOURCE_OK) {
        return mmay_compiler_source_status(source_status);
    }
    document.file_id = mmay_source_file_id(snapshot);
    document.version = version;
    document.snapshot = snapshot;
    document.snapshot_count = 1;
    if (!mmay_vector_push(&service->documents, &document)) {
        return MMAY_COMPILER_INTERNAL_ERROR;
    }
    service->retained_snapshot_count += 1;
    service->retained_source_bytes = retained_source_bytes;
    service->generation += 1;
    if (file_id != NULL) {
        *file_id = document.file_id;
    }
    return MMAY_COMPILER_OK;
}

static MarimayCompilerStatus mmay_compiler_service_overlay_internal(
    MarimayCompilerService *service,
    MarimayFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    const MarimayCompilerEdit *edit
) {
    MarimayCompilerDocument *document = NULL;
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimaySourceStatus status;
    size_t retained_source_bytes = 0;
    if (service == NULL || !service->initialized
        || (source_bytes == NULL && source_length != 0)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    document = mmay_compiler_find_document(service, file_id);
    if (document == NULL) {
        return MMAY_COMPILER_UNKNOWN_DOCUMENT;
    }
    if (version <= document->version) {
        return MMAY_COMPILER_STALE_VERSION;
    }
    if (service->generation == UINT64_MAX
        || source_length > service->options.max_source_bytes
        || document->snapshot_count
            >= service->options.max_snapshots_per_document
        || service->retained_snapshot_count
            >= service->options.max_total_snapshots
        || !mmay_size_add(
            service->retained_source_bytes,
            source_length,
            &retained_source_bytes
        )
        || retained_source_bytes > service->options.max_total_source_bytes) {
        return MMAY_COMPILER_LIMIT_REACHED;
    }
    status = mmay_source_manager_overlay(
        &service->sources,
        file_id,
        source_bytes,
        source_length,
        &snapshot
    );
    if (status != MMAY_SOURCE_OK) {
        return mmay_compiler_source_status(status);
    }
    document->version = version;
    document->previous_snapshot = edit != NULL
        ? document->snapshot
        : NULL;
    document->snapshot = snapshot;
    document->snapshot_count += 1;
    if (edit != NULL) {
        document->edit = *edit;
        document->has_edit = true;
    } else {
        memset(&document->edit, 0, sizeof(document->edit));
        document->has_edit = false;
    }
    service->retained_snapshot_count += 1;
    service->retained_source_bytes = retained_source_bytes;
    service->generation += 1;
    return MMAY_COMPILER_OK;
}

MarimayCompilerStatus mmay_compiler_service_overlay(
    MarimayCompilerService *service,
    MarimayFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length
) {
    return mmay_compiler_service_overlay_internal(
        service,
        file_id,
        version,
        source_bytes,
        source_length,
        NULL
    );
}

MarimayCompilerStatus mmay_compiler_service_overlay_edit(
    MarimayCompilerService *service,
    MarimayFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    MarimayCompilerEdit edit
) {
    MarimayCompilerDocument *document = NULL;
    const unsigned char *previous_bytes = NULL;
    const unsigned char *new_bytes = source_bytes;
    size_t previous_length = 0;
    size_t previous_suffix_length = 0;
    size_t new_suffix_length = 0;
    if (service == NULL || !service->initialized
        || (source_bytes == NULL && source_length != 0)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    document = mmay_compiler_find_document(service, file_id);
    if (document == NULL) {
        return MMAY_COMPILER_UNKNOWN_DOCUMENT;
    }
    if (edit.base_version != document->version || version <= document->version) {
        return MMAY_COMPILER_STALE_VERSION;
    }
    previous_length = mmay_source_length(document->snapshot);
    previous_bytes = mmay_source_bytes(document->snapshot);
    if (edit.start > edit.previous_end
        || edit.previous_end > previous_length
        || edit.start > edit.new_end || edit.new_end > source_length) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    previous_suffix_length = previous_length - edit.previous_end;
    new_suffix_length = source_length - edit.new_end;
    if (previous_suffix_length != new_suffix_length
        || (edit.start != 0
            && memcmp(previous_bytes, new_bytes, edit.start) != 0)
        || (previous_suffix_length != 0
            && memcmp(
                previous_bytes + edit.previous_end,
                new_bytes + edit.new_end,
                previous_suffix_length
            ) != 0)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    return mmay_compiler_service_overlay_internal(
        service,
        file_id,
        version,
        source_bytes,
        source_length,
        &edit
    );
}

bool mmay_compiler_service_document(
    const MarimayCompilerService *service,
    MarimayFileId file_id,
    MarimayCompilerDocument *document
) {
    const MarimayCompilerDocument *found = NULL;
    if (document != NULL) {
        memset(document, 0, sizeof(*document));
    }
    if (service == NULL || !service->initialized || document == NULL) {
        return false;
    }
    found = mmay_compiler_find_document_const(service, file_id);
    if (found == NULL) {
        return false;
    }
    *document = *found;
    return true;
}

static MarimayCompilerStatus mmay_compiler_service_check_target_request(
    const MarimayCompilerService *service,
    const MarimayCompilerSemanticTarget *target,
    const MarimayCompilerCheckRequest *request,
    MarimayCompilerResult *result
) {
    MarimayCompilerResult candidate;
    MarimayCompilerResult *built = &candidate;
    MarimayVector sorted_documents;
    MarimayVector asts;
    bool sorted_initialized = false;
    bool asts_initialized = false;
    size_t index = 0;
    MarimayCompilerStatus failure = MMAY_COMPILER_OK;
    MarimayCompilerCheckStats stats;
    memset(&stats, 0, sizeof(stats));
    if (result == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    memset(built, 0, sizeof(*built));
    if (service == NULL || !service->initialized || target == NULL
        || service->documents.length == 0
        || (!target->is_graph && (target->package_name == NULL
            || target->package_name[0] == '\0'))
        || (target->is_graph
            && !mmay_compiler_graph_target_valid(service, target))
        || (request != NULL && request->previous != NULL
            && !request->previous->initialized)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    if (mmay_compiler_request_cancelled(request)) {
        return MMAY_COMPILER_CANCELLED;
    }
    built->allocator = service->allocator;
    built->workspace_generation = service->generation;
    if (!mmay_vector_init(
            &built->units,
            service->allocator,
            sizeof(MarimayCompilerUnit)
        )) {
        return MMAY_COMPILER_INTERNAL_ERROR;
    }
    built->initialized = true;
    sorted_initialized = mmay_vector_init(
        &sorted_documents,
        service->allocator,
        sizeof(MarimayCompilerDocument *)
    );
    if (!sorted_initialized) {
        return mmay_compiler_result_fail(
            built,
            MMAY_COMPILER_INTERNAL_ERROR
        );
    }
    for (index = 0; index < service->documents.length; index += 1) {
        const MarimayCompilerDocument *document = mmay_vector_at_const(
            &service->documents,
            index
        );
        if (document == NULL || !mmay_vector_push(&sorted_documents, &document)) {
            failure = document == NULL
                ? MMAY_COMPILER_INTERNAL_ERROR
                : MMAY_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    qsort(
        sorted_documents.data,
        sorted_documents.length,
        sorted_documents.element_size,
        mmay_compiler_compare_document_paths
    );
    for (index = 0; index < sorted_documents.length; index += 1) {
        const MarimayCompilerDocument *const *slot = mmay_vector_at_const(
            &sorted_documents,
            index
        );
        const MarimayCompilerDocument *document = slot != NULL ? *slot : NULL;
        MarimayCompilerUnit unit;
        const MarimayCompilerUnit *previous_unit = NULL;
        MarimayIncrementalParseStats incremental_stats;
        MarimayLexStatus lex_status;
        MarimayParseStatus parse_status;
        memset(&unit, 0, sizeof(unit));
        memset(&incremental_stats, 0, sizeof(incremental_stats));
        if (mmay_compiler_request_cancelled(request)) {
            failure = MMAY_COMPILER_CANCELLED;
            goto cleanup;
        }
        if (document == NULL || document->snapshot == NULL) {
            failure = MMAY_COMPILER_INTERNAL_ERROR;
            goto cleanup;
        }
        unit.file_id = document->file_id;
        unit.version = document->version;
        unit.snapshot = document->snapshot;
        lex_status = mmay_lex_snapshot(
            unit.snapshot,
            service->allocator,
            service->options.lex_options,
            &unit.lexed
        );
        failure = mmay_compiler_lex_status(lex_status);
        if (failure != MMAY_COMPILER_OK) {
            mmay_compiler_unit_dispose(&unit);
            goto cleanup;
        }
        if (mmay_compiler_request_cancelled(request)) {
            mmay_compiler_unit_dispose(&unit);
            failure = MMAY_COMPILER_CANCELLED;
            goto cleanup;
        }
        previous_unit = request != NULL && request->previous != NULL
            ? mmay_compiler_find_unit(request->previous, document->file_id)
            : NULL;
        if (document->has_edit && previous_unit != NULL
            && previous_unit->version == document->edit.base_version
            && previous_unit->snapshot == document->previous_snapshot
            && previous_unit->parsed.initialized) {
            parse_status = mmay_parse_file_incremental(
                &previous_unit->parsed,
                &unit.lexed,
                document->edit.start,
                document->edit.previous_end,
                document->edit.new_end,
                service->allocator,
                service->options.parse_options,
                &unit.parsed,
                &incremental_stats
            );
            if (parse_status == MMAY_PARSE_INVALID_ARGUMENT) {
                mmay_compiler_unit_dispose(&unit);
                failure = MMAY_COMPILER_INTERNAL_ERROR;
                goto cleanup;
            }
            if (parse_status == MMAY_PARSE_OK) {
                size_t incremental_count = 0;
                size_t reused_count = 0;
                size_t reparsed_bytes = 0;
                if (!mmay_size_add(
                        stats.incremental_parse_count,
                        1,
                        &incremental_count
                    ) || !mmay_size_add(
                        stats.reused_top_level_nodes,
                        incremental_stats.reused_top_level_nodes,
                        &reused_count
                    ) || !mmay_size_add(
                        stats.reparsed_bytes,
                        incremental_stats.reparsed_bytes,
                        &reparsed_bytes
                    )) {
                    mmay_compiler_unit_dispose(&unit);
                    failure = MMAY_COMPILER_LIMIT_REACHED;
                    goto cleanup;
                }
                stats.incremental_parse_count = incremental_count;
                stats.reused_top_level_nodes = reused_count;
                stats.reparsed_bytes = reparsed_bytes;
            }
        } else {
            parse_status = mmay_parse_file(
                &unit.lexed,
                service->allocator,
                service->options.parse_options,
                &unit.parsed
            );
            if (parse_status == MMAY_PARSE_OK) {
                size_t full_count = 0;
                if (!mmay_size_add(
                        stats.full_parse_count,
                        1,
                        &full_count
                    )) {
                    mmay_compiler_unit_dispose(&unit);
                    failure = MMAY_COMPILER_LIMIT_REACHED;
                    goto cleanup;
                }
                stats.full_parse_count = full_count;
            }
        }
        failure = mmay_compiler_parse_status(parse_status);
        if (failure != MMAY_COMPILER_OK) {
            mmay_compiler_unit_dispose(&unit);
            goto cleanup;
        }
        if (mmay_compiler_request_cancelled(request)) {
            mmay_compiler_unit_dispose(&unit);
            failure = MMAY_COMPILER_CANCELLED;
            goto cleanup;
        }
        if (unit.lexed.diagnostics.length != 0
            || unit.parsed.diagnostics.length != 0) {
            built->has_frontend_errors = true;
        }
        if (!mmay_vector_push(&built->units, &unit)) {
            mmay_compiler_unit_dispose(&unit);
            failure = MMAY_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (built->has_frontend_errors) {
        if (mmay_compiler_request_cancelled(request)) {
            failure = MMAY_COMPILER_CANCELLED;
        }
        goto cleanup;
    }
    asts_initialized = mmay_vector_init(
        &asts,
        service->allocator,
        sizeof(MarimayAstTree *)
    );
    if (!asts_initialized) {
        failure = MMAY_COMPILER_INTERNAL_ERROR;
        goto cleanup;
    }
    for (index = 0; index < built->units.length; index += 1) {
        MarimayCompilerUnit *unit = mmay_vector_at(&built->units, index);
        MarimayAstStatus ast_status;
        MarimayAstTree *ast = NULL;
        if (mmay_compiler_request_cancelled(request)) {
            failure = MMAY_COMPILER_CANCELLED;
            goto cleanup;
        }
        if (unit == NULL) {
            failure = MMAY_COMPILER_INTERNAL_ERROR;
            goto cleanup;
        }
        ast_status = mmay_ast_project(
            &unit->parsed.tree,
            service->allocator,
            service->options.ast_options,
            &unit->ast
        );
        failure = mmay_compiler_ast_status(ast_status);
        if (failure != MMAY_COMPILER_OK) {
            goto cleanup;
        }
        if (mmay_compiler_request_cancelled(request)) {
            failure = MMAY_COMPILER_CANCELLED;
            goto cleanup;
        }
        ast = &unit->ast;
        if (!mmay_vector_push(&asts, &ast)) {
            failure = MMAY_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (mmay_compiler_request_cancelled(request)) {
        failure = MMAY_COMPILER_CANCELLED;
        goto cleanup;
    }
    failure = target->is_graph
        ? mmay_compiler_check_graph_semantics(service, target, built)
        : mmay_compiler_check_status(mmay_check_package(
            (const MarimayAstTree *const *)asts.data,
            asts.length,
            target->package_name,
            service->allocator,
            service->options.check_options,
            &built->checked
        ));
    if (failure == MMAY_COMPILER_OK
        && mmay_compiler_request_cancelled(request)) {
        failure = MMAY_COMPILER_CANCELLED;
    }

cleanup:
    if (asts_initialized) {
        mmay_vector_dispose(&asts);
    }
    if (sorted_initialized) {
        mmay_vector_dispose(&sorted_documents);
    }
    if (failure != MMAY_COMPILER_OK) {
        if (failure == MMAY_COMPILER_INCOMPLETE_LANGUAGE
            && built->initialized && built->checked.initialized
            && built->checked.diagnostics.length != 0) {
            mmay_compiler_result_dispose(result);
            *result = *built;
            memset(built, 0, sizeof(*built));
            if (request != NULL && request->stats != NULL) {
                *request->stats = stats;
            }
            return failure;
        }
        return mmay_compiler_result_fail(built, failure);
    }
    mmay_compiler_result_dispose(result);
    *result = *built;
    memset(built, 0, sizeof(*built));
    if (request != NULL && request->stats != NULL) {
        *request->stats = stats;
    }
    return MMAY_COMPILER_OK;
}

MarimayCompilerStatus mmay_compiler_service_check_request(
    const MarimayCompilerService *service,
    const char *package_name,
    const MarimayCompilerCheckRequest *request,
    MarimayCompilerResult *result
) {
    MarimayCompilerSemanticTarget target;
    memset(&target, 0, sizeof(target));
    target.package_name = package_name;
    return mmay_compiler_service_check_target_request(
        service,
        &target,
        request,
        result
    );
}

MarimayCompilerStatus mmay_compiler_service_check(
    const MarimayCompilerService *service,
    const char *package_name,
    MarimayCompilerResult *result
) {
    return mmay_compiler_service_check_request(
        service,
        package_name,
        NULL,
        result
    );
}

MarimayCompilerStatus mmay_compiler_service_check_graph_request(
    const MarimayCompilerService *service,
    const MarimayCompilerPackage *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    const MarimayCompilerCheckRequest *request,
    MarimayCompilerResult *result
) {
    MarimayCompilerSemanticTarget target;
    memset(&target, 0, sizeof(target));
    target.packages = packages;
    target.package_count = package_count;
    target.root_import_path = root_import_path;
    target.root_import_path_length = root_import_path_length;
    target.is_graph = true;
    return mmay_compiler_service_check_target_request(
        service,
        &target,
        request,
        result
    );
}

MarimayCompilerStatus mmay_compiler_service_check_graph(
    const MarimayCompilerService *service,
    const MarimayCompilerPackage *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    MarimayCompilerResult *result
) {
    return mmay_compiler_service_check_graph_request(
        service,
        packages,
        package_count,
        root_import_path,
        root_import_path_length,
        NULL,
        result
    );
}

MarimayCompilerStatus mmay_compiler_service_check_module_graph(
    const MarimayCompilerService *service,
    const MarimayCompilerPackage *packages,
    size_t package_count,
    MarimayCompilerResult *result
) {
    MarimayCompilerSemanticTarget target;
    memset(&target, 0, sizeof(target));
    target.packages = packages;
    target.package_count = package_count;
    target.is_graph = true;
    target.all_packages = true;
    return mmay_compiler_service_check_target_request(
        service,
        &target,
        NULL,
        result
    );
}

bool mmay_compiler_result_is_current(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result
) {
    size_t index = 0;
    if (service == NULL || !service->initialized || result == NULL
        || !result->initialized
        || result->workspace_generation != service->generation
        || result->units.length != service->documents.length) {
        return false;
    }
    for (index = 0; index < result->units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_vector_at_const(
            &result->units,
            index
        );
        const MarimayCompilerDocument *document = unit == NULL
            ? NULL
            : mmay_compiler_find_document_const(service, unit->file_id);
        if (document == NULL || unit->version != document->version
            || unit->snapshot != document->snapshot) {
            return false;
        }
    }
    return true;
}

const MarimayCompilerUnit *mmay_compiler_result_unit(
    const MarimayCompilerResult *result,
    size_t index
) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return mmay_vector_at_const(&result->units, index);
}

bool mmay_compiler_result_has_errors(const MarimayCompilerResult *result) {
    return result != NULL && result->initialized
        && (result->has_frontend_errors
            || (result->checked.initialized
                && result->checked.diagnostics.length != 0));
}

void mmay_compiler_result_dispose(MarimayCompilerResult *result) {
    size_t index = 0;
    if (result == NULL) {
        return;
    }
    mmay_check_result_dispose(&result->checked);
    for (index = 0; index < result->units.length; index += 1) {
        MarimayCompilerUnit *unit = mmay_vector_at(&result->units, index);
        mmay_compiler_unit_dispose(unit);
    }
    mmay_vector_dispose(&result->units);
    memset(result, 0, sizeof(*result));
}
