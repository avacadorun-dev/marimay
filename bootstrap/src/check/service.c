#include "ruda/check/service.h"

#include <stdlib.h>
#include <string.h>

typedef struct RudaCompilerSemanticTarget {
    const char *package_name;
    const RudaCompilerPackage *packages;
    size_t package_count;
    const char *root_import_path;
    size_t root_import_path_length;
    bool is_graph;
} RudaCompilerSemanticTarget;

static bool ruda_compiler_allocator_valid(RudaAllocator allocator) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL;
}

static RudaCompilerDocument *ruda_compiler_find_document(
    RudaCompilerService *service,
    RudaFileId file_id
) {
    size_t index = 0;
    if (service == NULL) {
        return NULL;
    }
    for (index = 0; index < service->documents.length; index += 1) {
        RudaCompilerDocument *document = ruda_vector_at(&service->documents, index);
        if (document != NULL && document->file_id == file_id) {
            return document;
        }
    }
    return NULL;
}

static const RudaCompilerDocument *ruda_compiler_find_document_const(
    const RudaCompilerService *service,
    RudaFileId file_id
) {
    size_t index = 0;
    if (service == NULL) {
        return NULL;
    }
    for (index = 0; index < service->documents.length; index += 1) {
        const RudaCompilerDocument *document = ruda_vector_at_const(
            &service->documents,
            index
        );
        if (document != NULL && document->file_id == file_id) {
            return document;
        }
    }
    return NULL;
}

static int ruda_compiler_compare_document_paths(
    const void *left_pointer,
    const void *right_pointer
) {
    const RudaCompilerDocument *left = *(const RudaCompilerDocument *const *)left_pointer;
    const RudaCompilerDocument *right = *(const RudaCompilerDocument *const *)right_pointer;
    const char *left_path = ruda_source_path(left->snapshot);
    const char *right_path = ruda_source_path(right->snapshot);
    size_t left_length = ruda_source_path_length(left->snapshot);
    size_t right_length = ruda_source_path_length(right->snapshot);
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

static RudaCompilerStatus ruda_compiler_source_status(RudaSourceStatus status) {
    switch (status) {
        case RUDA_SOURCE_OK:
            return RUDA_COMPILER_OK;
        case RUDA_SOURCE_INVALID_ARGUMENT:
            return RUDA_COMPILER_INVALID_ARGUMENT;
        case RUDA_SOURCE_ALLOCATION_FAILED:
            return RUDA_COMPILER_ALLOCATION_FAILED;
        case RUDA_SOURCE_DUPLICATE_PATH:
            return RUDA_COMPILER_DUPLICATE_PATH;
        case RUDA_SOURCE_UNKNOWN_FILE:
            return RUDA_COMPILER_UNKNOWN_DOCUMENT;
        case RUDA_SOURCE_ID_EXHAUSTED:
            return RUDA_COMPILER_LIMIT_REACHED;
    }
    return RUDA_COMPILER_INTERNAL_ERROR;
}

static RudaCompilerStatus ruda_compiler_lex_status(RudaLexStatus status) {
    switch (status) {
        case RUDA_LEX_OK:
            return RUDA_COMPILER_OK;
        case RUDA_LEX_INVALID_ARGUMENT:
            return RUDA_COMPILER_INVALID_ARGUMENT;
        case RUDA_LEX_ALLOCATION_FAILED:
            return RUDA_COMPILER_ALLOCATION_FAILED;
        case RUDA_LEX_LIMIT_REACHED:
            return RUDA_COMPILER_LIMIT_REACHED;
    }
    return RUDA_COMPILER_INTERNAL_ERROR;
}

static RudaCompilerStatus ruda_compiler_parse_status(RudaParseStatus status) {
    switch (status) {
        case RUDA_PARSE_OK:
            return RUDA_COMPILER_OK;
        case RUDA_PARSE_INVALID_ARGUMENT:
            return RUDA_COMPILER_INVALID_ARGUMENT;
        case RUDA_PARSE_ALLOCATION_FAILED:
            return RUDA_COMPILER_ALLOCATION_FAILED;
        case RUDA_PARSE_LIMIT_REACHED:
            return RUDA_COMPILER_LIMIT_REACHED;
    }
    return RUDA_COMPILER_INTERNAL_ERROR;
}

static RudaCompilerStatus ruda_compiler_ast_status(RudaAstStatus status) {
    switch (status) {
        case RUDA_AST_OK:
            return RUDA_COMPILER_OK;
        case RUDA_AST_INVALID_ARGUMENT:
            return RUDA_COMPILER_INVALID_ARGUMENT;
        case RUDA_AST_ALLOCATION_FAILED:
            return RUDA_COMPILER_ALLOCATION_FAILED;
        case RUDA_AST_LIMIT_REACHED:
            return RUDA_COMPILER_LIMIT_REACHED;
    }
    return RUDA_COMPILER_INTERNAL_ERROR;
}

static RudaCompilerStatus ruda_compiler_check_status(RudaCheckStatus status) {
    switch (status) {
        case RUDA_CHECK_OK:
            return RUDA_COMPILER_OK;
        case RUDA_CHECK_INVALID_ARGUMENT:
            return RUDA_COMPILER_INVALID_ARGUMENT;
        case RUDA_CHECK_ALLOCATION_FAILED:
            return RUDA_COMPILER_ALLOCATION_FAILED;
        case RUDA_CHECK_LIMIT_REACHED:
            return RUDA_COMPILER_LIMIT_REACHED;
        case RUDA_CHECK_INCOMPLETE_LANGUAGE:
            return RUDA_COMPILER_INCOMPLETE_LANGUAGE;
        case RUDA_CHECK_INTERNAL_ERROR:
            return RUDA_COMPILER_INTERNAL_ERROR;
    }
    return RUDA_COMPILER_INTERNAL_ERROR;
}

static void ruda_compiler_unit_dispose(RudaCompilerUnit *unit) {
    if (unit == NULL) {
        return;
    }
    ruda_ast_tree_dispose(&unit->ast);
    ruda_parse_result_dispose(&unit->parsed);
    ruda_lex_result_dispose(&unit->lexed);
    memset(unit, 0, sizeof(*unit));
}

static RudaCompilerStatus ruda_compiler_result_fail(
    RudaCompilerResult *result,
    RudaCompilerStatus status
) {
    ruda_compiler_result_dispose(result);
    return status;
}

static bool ruda_compiler_request_cancelled(
    const RudaCompilerCheckRequest *request
) {
    return request != NULL && request->cancel_poll != NULL
        && request->cancel_poll(request->cancel_context);
}

static const RudaCompilerUnit *ruda_compiler_find_unit(
    const RudaCompilerResult *result,
    RudaFileId file_id
) {
    size_t index = 0;
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    for (index = 0; index < result->units.length; index += 1) {
        const RudaCompilerUnit *unit = ruda_vector_at_const(
            &result->units,
            index
        );
        if (unit != NULL && unit->file_id == file_id) {
            return unit;
        }
    }
    return NULL;
}

static bool ruda_compiler_graph_target_valid(
    const RudaCompilerService *service,
    const RudaCompilerSemanticTarget *target
) {
    RudaCheckGraphOptions graph_options = ruda_check_graph_default_options();
    size_t package_index = 0;
    size_t total_files = 0;
    if (service == NULL || target == NULL || target->packages == NULL
        || target->package_count == 0
        || target->package_count > graph_options.max_packages
        || target->root_import_path == NULL
        || target->root_import_path_length == 0) {
        return false;
    }
    for (package_index = 0; package_index < target->package_count;
        package_index += 1) {
        const RudaCompilerPackage *package = &target->packages[package_index];
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
            || !ruda_size_add(total_files, package->file_count, &total_files)) {
            return false;
        }
        for (file_index = 0; file_index < package->file_count;
            file_index += 1) {
            size_t earlier_package = 0;
            if (ruda_compiler_find_document_const(
                    service,
                    package->file_ids[file_index]
                ) == NULL) {
                return false;
            }
            for (earlier_package = 0;
                earlier_package <= package_index; earlier_package += 1) {
                const RudaCompilerPackage *earlier =
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

static RudaCompilerStatus ruda_compiler_check_graph_semantics(
    const RudaCompilerService *service,
    const RudaCompilerSemanticTarget *target,
    RudaCompilerResult *built
) {
    RudaVector graph_asts;
    RudaVector graph_inputs;
    RudaCheckGraphOptions options = ruda_check_graph_default_options();
    RudaCheckStatus status = RUDA_CHECK_INTERNAL_ERROR;
    size_t package_index = 0;
    bool asts_initialized = false;
    bool inputs_initialized = false;
    memset(&graph_asts, 0, sizeof(graph_asts));
    memset(&graph_inputs, 0, sizeof(graph_inputs));
    asts_initialized = ruda_vector_init(
        &graph_asts,
        service->allocator,
        sizeof(RudaAstTree *)
    );
    inputs_initialized = ruda_vector_init(
        &graph_inputs,
        service->allocator,
        sizeof(RudaCheckPackageInput)
    );
    if (!asts_initialized || !inputs_initialized
        || !ruda_vector_reserve(&graph_asts, built->units.length)
        || !ruda_vector_reserve(&graph_inputs, target->package_count)) {
        if (inputs_initialized) {
            ruda_vector_dispose(&graph_inputs);
        }
        if (asts_initialized) {
            ruda_vector_dispose(&graph_asts);
        }
        return RUDA_COMPILER_ALLOCATION_FAILED;
    }
    for (package_index = 0; package_index < target->package_count;
        package_index += 1) {
        const RudaCompilerPackage *package = &target->packages[package_index];
        RudaCheckPackageInput input;
        size_t ast_start = graph_asts.length;
        size_t file_index = 0;
        memset(&input, 0, sizeof(input));
        for (file_index = 0; file_index < package->file_count;
            file_index += 1) {
            const RudaCompilerUnit *unit = ruda_compiler_find_unit(
                built,
                package->file_ids[file_index]
            );
            const RudaAstTree *ast = unit == NULL ? NULL : &unit->ast;
            if (ast == NULL || !ruda_vector_push(&graph_asts, &ast)) {
                ruda_vector_dispose(&graph_inputs);
                ruda_vector_dispose(&graph_asts);
                return ast == NULL ? RUDA_COMPILER_INTERNAL_ERROR
                    : RUDA_COMPILER_ALLOCATION_FAILED;
            }
        }
        input.import_path = package->import_path;
        input.import_path_length = package->import_path_length;
        input.asts = (const RudaAstTree *const *)graph_asts.data + ast_start;
        input.ast_count = package->file_count;
        if (!ruda_vector_push(&graph_inputs, &input)) {
            ruda_vector_dispose(&graph_inputs);
            ruda_vector_dispose(&graph_asts);
            return RUDA_COMPILER_ALLOCATION_FAILED;
        }
    }
    options.package_options = service->options.check_options;
    status = ruda_check_package_graph(
        graph_inputs.data,
        graph_inputs.length,
        target->root_import_path,
        target->root_import_path_length,
        service->allocator,
        options,
        &built->checked
    );
    ruda_vector_dispose(&graph_inputs);
    ruda_vector_dispose(&graph_asts);
    return ruda_compiler_check_status(status);
}

RudaCompilerOptions ruda_compiler_default_options(void) {
    RudaCompilerOptions options;
    options.max_documents = 1024;
    options.max_snapshots_per_document = 1024;
    options.max_total_snapshots = 4096;
    options.max_path_bytes = 4096;
    options.max_source_bytes = 16U * 1024U * 1024U;
    options.max_total_source_bytes = 256U * 1024U * 1024U;
    options.lex_options = ruda_lex_default_options();
    options.parse_options = ruda_parse_default_options();
    options.ast_options = ruda_ast_default_options();
    options.check_options = ruda_check_default_options();
    return options;
}

RudaCompilerStatus ruda_compiler_service_init(
    RudaCompilerService *service,
    RudaAllocator allocator,
    RudaCompilerOptions options
) {
    RudaCompilerService candidate;
    RudaCompilerService *built = &candidate;
    if (service == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    memset(built, 0, sizeof(*built));
    if (!ruda_compiler_allocator_valid(allocator) || options.max_documents == 0
        || options.max_snapshots_per_document == 0
        || options.max_total_snapshots < options.max_documents
        || options.max_path_bytes == 0 || options.max_source_bytes == 0
        || options.max_total_source_bytes == 0
        || options.max_documents > options.check_options.max_source_files
        || !ruda_lex_options_valid(options.lex_options)
        || !ruda_parse_options_valid(options.parse_options)
        || !ruda_ast_options_valid(options.ast_options)
        || !ruda_check_options_valid(allocator, options.check_options)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    built->allocator = allocator;
    built->options = options;
    built->generation = 1;
    if (!ruda_source_manager_init(&built->sources, allocator)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    if (!ruda_vector_init(
            &built->documents,
            allocator,
            sizeof(RudaCompilerDocument)
        )) {
        ruda_source_manager_dispose(&built->sources);
        memset(built, 0, sizeof(*built));
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    built->initialized = true;
    ruda_compiler_service_dispose(service);
    *service = *built;
    memset(built, 0, sizeof(*built));
    return RUDA_COMPILER_OK;
}

void ruda_compiler_service_dispose(RudaCompilerService *service) {
    if (service == NULL) {
        return;
    }
    ruda_vector_dispose(&service->documents);
    ruda_source_manager_dispose(&service->sources);
    memset(service, 0, sizeof(*service));
}

RudaCompilerStatus ruda_compiler_service_add_document(
    RudaCompilerService *service,
    const char *path,
    size_t path_length,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    RudaFileId *file_id
) {
    const RudaSourceSnapshot *snapshot = NULL;
    RudaCompilerDocument document;
    RudaSourceStatus source_status;
    size_t retained_source_bytes = 0;
    if (file_id != NULL) {
        *file_id = 0;
    }
    memset(&document, 0, sizeof(document));
    if (service == NULL || !service->initialized || path == NULL
        || path_length == 0 || (source_bytes == NULL && source_length != 0)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    if (service->documents.length >= service->options.max_documents
        || service->generation == UINT64_MAX
        || service->retained_snapshot_count
            >= service->options.max_total_snapshots
        || path_length > service->options.max_path_bytes
        || source_length > service->options.max_source_bytes
        || !ruda_size_add(
            service->retained_source_bytes,
            source_length,
            &retained_source_bytes
        )
        || retained_source_bytes > service->options.max_total_source_bytes) {
        return RUDA_COMPILER_LIMIT_REACHED;
    }
    if (!ruda_vector_reserve(
            &service->documents,
            service->documents.length + 1
        )) {
        return RUDA_COMPILER_ALLOCATION_FAILED;
    }
    source_status = ruda_source_manager_add(
        &service->sources,
        path,
        path_length,
        source_bytes,
        source_length,
        &snapshot
    );
    if (source_status != RUDA_SOURCE_OK) {
        return ruda_compiler_source_status(source_status);
    }
    document.file_id = ruda_source_file_id(snapshot);
    document.version = version;
    document.snapshot = snapshot;
    document.snapshot_count = 1;
    if (!ruda_vector_push(&service->documents, &document)) {
        return RUDA_COMPILER_INTERNAL_ERROR;
    }
    service->retained_snapshot_count += 1;
    service->retained_source_bytes = retained_source_bytes;
    service->generation += 1;
    if (file_id != NULL) {
        *file_id = document.file_id;
    }
    return RUDA_COMPILER_OK;
}

static RudaCompilerStatus ruda_compiler_service_overlay_internal(
    RudaCompilerService *service,
    RudaFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    const RudaCompilerEdit *edit
) {
    RudaCompilerDocument *document = NULL;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaSourceStatus status;
    size_t retained_source_bytes = 0;
    if (service == NULL || !service->initialized
        || (source_bytes == NULL && source_length != 0)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    document = ruda_compiler_find_document(service, file_id);
    if (document == NULL) {
        return RUDA_COMPILER_UNKNOWN_DOCUMENT;
    }
    if (version <= document->version) {
        return RUDA_COMPILER_STALE_VERSION;
    }
    if (service->generation == UINT64_MAX
        || source_length > service->options.max_source_bytes
        || document->snapshot_count
            >= service->options.max_snapshots_per_document
        || service->retained_snapshot_count
            >= service->options.max_total_snapshots
        || !ruda_size_add(
            service->retained_source_bytes,
            source_length,
            &retained_source_bytes
        )
        || retained_source_bytes > service->options.max_total_source_bytes) {
        return RUDA_COMPILER_LIMIT_REACHED;
    }
    status = ruda_source_manager_overlay(
        &service->sources,
        file_id,
        source_bytes,
        source_length,
        &snapshot
    );
    if (status != RUDA_SOURCE_OK) {
        return ruda_compiler_source_status(status);
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
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_overlay(
    RudaCompilerService *service,
    RudaFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length
) {
    return ruda_compiler_service_overlay_internal(
        service,
        file_id,
        version,
        source_bytes,
        source_length,
        NULL
    );
}

RudaCompilerStatus ruda_compiler_service_overlay_edit(
    RudaCompilerService *service,
    RudaFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    RudaCompilerEdit edit
) {
    RudaCompilerDocument *document = NULL;
    const unsigned char *previous_bytes = NULL;
    const unsigned char *new_bytes = source_bytes;
    size_t previous_length = 0;
    size_t previous_suffix_length = 0;
    size_t new_suffix_length = 0;
    if (service == NULL || !service->initialized
        || (source_bytes == NULL && source_length != 0)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    document = ruda_compiler_find_document(service, file_id);
    if (document == NULL) {
        return RUDA_COMPILER_UNKNOWN_DOCUMENT;
    }
    if (edit.base_version != document->version || version <= document->version) {
        return RUDA_COMPILER_STALE_VERSION;
    }
    previous_length = ruda_source_length(document->snapshot);
    previous_bytes = ruda_source_bytes(document->snapshot);
    if (edit.start > edit.previous_end
        || edit.previous_end > previous_length
        || edit.start > edit.new_end || edit.new_end > source_length) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
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
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    return ruda_compiler_service_overlay_internal(
        service,
        file_id,
        version,
        source_bytes,
        source_length,
        &edit
    );
}

bool ruda_compiler_service_document(
    const RudaCompilerService *service,
    RudaFileId file_id,
    RudaCompilerDocument *document
) {
    const RudaCompilerDocument *found = NULL;
    if (document != NULL) {
        memset(document, 0, sizeof(*document));
    }
    if (service == NULL || !service->initialized || document == NULL) {
        return false;
    }
    found = ruda_compiler_find_document_const(service, file_id);
    if (found == NULL) {
        return false;
    }
    *document = *found;
    return true;
}

static RudaCompilerStatus ruda_compiler_service_check_target_request(
    const RudaCompilerService *service,
    const RudaCompilerSemanticTarget *target,
    const RudaCompilerCheckRequest *request,
    RudaCompilerResult *result
) {
    RudaCompilerResult candidate;
    RudaCompilerResult *built = &candidate;
    RudaVector sorted_documents;
    RudaVector asts;
    bool sorted_initialized = false;
    bool asts_initialized = false;
    size_t index = 0;
    RudaCompilerStatus failure = RUDA_COMPILER_OK;
    RudaCompilerCheckStats stats;
    memset(&stats, 0, sizeof(stats));
    if (result == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    memset(built, 0, sizeof(*built));
    if (service == NULL || !service->initialized || target == NULL
        || service->documents.length == 0
        || (!target->is_graph && (target->package_name == NULL
            || target->package_name[0] == '\0'))
        || (target->is_graph
            && !ruda_compiler_graph_target_valid(service, target))
        || (request != NULL && request->previous != NULL
            && !request->previous->initialized)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    if (ruda_compiler_request_cancelled(request)) {
        return RUDA_COMPILER_CANCELLED;
    }
    built->allocator = service->allocator;
    built->workspace_generation = service->generation;
    if (!ruda_vector_init(
            &built->units,
            service->allocator,
            sizeof(RudaCompilerUnit)
        )) {
        return RUDA_COMPILER_INTERNAL_ERROR;
    }
    built->initialized = true;
    sorted_initialized = ruda_vector_init(
        &sorted_documents,
        service->allocator,
        sizeof(RudaCompilerDocument *)
    );
    if (!sorted_initialized) {
        return ruda_compiler_result_fail(
            built,
            RUDA_COMPILER_INTERNAL_ERROR
        );
    }
    for (index = 0; index < service->documents.length; index += 1) {
        const RudaCompilerDocument *document = ruda_vector_at_const(
            &service->documents,
            index
        );
        if (document == NULL || !ruda_vector_push(&sorted_documents, &document)) {
            failure = document == NULL
                ? RUDA_COMPILER_INTERNAL_ERROR
                : RUDA_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    qsort(
        sorted_documents.data,
        sorted_documents.length,
        sorted_documents.element_size,
        ruda_compiler_compare_document_paths
    );
    for (index = 0; index < sorted_documents.length; index += 1) {
        const RudaCompilerDocument *const *slot = ruda_vector_at_const(
            &sorted_documents,
            index
        );
        const RudaCompilerDocument *document = slot != NULL ? *slot : NULL;
        RudaCompilerUnit unit;
        const RudaCompilerUnit *previous_unit = NULL;
        RudaIncrementalParseStats incremental_stats;
        RudaLexStatus lex_status;
        RudaParseStatus parse_status;
        memset(&unit, 0, sizeof(unit));
        memset(&incremental_stats, 0, sizeof(incremental_stats));
        if (ruda_compiler_request_cancelled(request)) {
            failure = RUDA_COMPILER_CANCELLED;
            goto cleanup;
        }
        if (document == NULL || document->snapshot == NULL) {
            failure = RUDA_COMPILER_INTERNAL_ERROR;
            goto cleanup;
        }
        unit.file_id = document->file_id;
        unit.version = document->version;
        unit.snapshot = document->snapshot;
        lex_status = ruda_lex_snapshot(
            unit.snapshot,
            service->allocator,
            service->options.lex_options,
            &unit.lexed
        );
        failure = ruda_compiler_lex_status(lex_status);
        if (failure != RUDA_COMPILER_OK) {
            ruda_compiler_unit_dispose(&unit);
            goto cleanup;
        }
        if (ruda_compiler_request_cancelled(request)) {
            ruda_compiler_unit_dispose(&unit);
            failure = RUDA_COMPILER_CANCELLED;
            goto cleanup;
        }
        previous_unit = request != NULL && request->previous != NULL
            ? ruda_compiler_find_unit(request->previous, document->file_id)
            : NULL;
        if (document->has_edit && previous_unit != NULL
            && previous_unit->version == document->edit.base_version
            && previous_unit->snapshot == document->previous_snapshot
            && previous_unit->parsed.initialized) {
            parse_status = ruda_parse_file_incremental(
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
            if (parse_status == RUDA_PARSE_INVALID_ARGUMENT) {
                ruda_compiler_unit_dispose(&unit);
                failure = RUDA_COMPILER_INTERNAL_ERROR;
                goto cleanup;
            }
            if (parse_status == RUDA_PARSE_OK) {
                size_t incremental_count = 0;
                size_t reused_count = 0;
                size_t reparsed_bytes = 0;
                if (!ruda_size_add(
                        stats.incremental_parse_count,
                        1,
                        &incremental_count
                    ) || !ruda_size_add(
                        stats.reused_top_level_nodes,
                        incremental_stats.reused_top_level_nodes,
                        &reused_count
                    ) || !ruda_size_add(
                        stats.reparsed_bytes,
                        incremental_stats.reparsed_bytes,
                        &reparsed_bytes
                    )) {
                    ruda_compiler_unit_dispose(&unit);
                    failure = RUDA_COMPILER_LIMIT_REACHED;
                    goto cleanup;
                }
                stats.incremental_parse_count = incremental_count;
                stats.reused_top_level_nodes = reused_count;
                stats.reparsed_bytes = reparsed_bytes;
            }
        } else {
            parse_status = ruda_parse_file(
                &unit.lexed,
                service->allocator,
                service->options.parse_options,
                &unit.parsed
            );
            if (parse_status == RUDA_PARSE_OK) {
                size_t full_count = 0;
                if (!ruda_size_add(
                        stats.full_parse_count,
                        1,
                        &full_count
                    )) {
                    ruda_compiler_unit_dispose(&unit);
                    failure = RUDA_COMPILER_LIMIT_REACHED;
                    goto cleanup;
                }
                stats.full_parse_count = full_count;
            }
        }
        failure = ruda_compiler_parse_status(parse_status);
        if (failure != RUDA_COMPILER_OK) {
            ruda_compiler_unit_dispose(&unit);
            goto cleanup;
        }
        if (ruda_compiler_request_cancelled(request)) {
            ruda_compiler_unit_dispose(&unit);
            failure = RUDA_COMPILER_CANCELLED;
            goto cleanup;
        }
        if (unit.lexed.diagnostics.length != 0
            || unit.parsed.diagnostics.length != 0) {
            built->has_frontend_errors = true;
        }
        if (!ruda_vector_push(&built->units, &unit)) {
            ruda_compiler_unit_dispose(&unit);
            failure = RUDA_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (built->has_frontend_errors) {
        if (ruda_compiler_request_cancelled(request)) {
            failure = RUDA_COMPILER_CANCELLED;
        }
        goto cleanup;
    }
    asts_initialized = ruda_vector_init(
        &asts,
        service->allocator,
        sizeof(RudaAstTree *)
    );
    if (!asts_initialized) {
        failure = RUDA_COMPILER_INTERNAL_ERROR;
        goto cleanup;
    }
    for (index = 0; index < built->units.length; index += 1) {
        RudaCompilerUnit *unit = ruda_vector_at(&built->units, index);
        RudaAstStatus ast_status;
        RudaAstTree *ast = NULL;
        if (ruda_compiler_request_cancelled(request)) {
            failure = RUDA_COMPILER_CANCELLED;
            goto cleanup;
        }
        if (unit == NULL) {
            failure = RUDA_COMPILER_INTERNAL_ERROR;
            goto cleanup;
        }
        ast_status = ruda_ast_project(
            &unit->parsed.tree,
            service->allocator,
            service->options.ast_options,
            &unit->ast
        );
        failure = ruda_compiler_ast_status(ast_status);
        if (failure != RUDA_COMPILER_OK) {
            goto cleanup;
        }
        if (ruda_compiler_request_cancelled(request)) {
            failure = RUDA_COMPILER_CANCELLED;
            goto cleanup;
        }
        ast = &unit->ast;
        if (!ruda_vector_push(&asts, &ast)) {
            failure = RUDA_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (ruda_compiler_request_cancelled(request)) {
        failure = RUDA_COMPILER_CANCELLED;
        goto cleanup;
    }
    failure = target->is_graph
        ? ruda_compiler_check_graph_semantics(service, target, built)
        : ruda_compiler_check_status(ruda_check_package(
            (const RudaAstTree *const *)asts.data,
            asts.length,
            target->package_name,
            service->allocator,
            service->options.check_options,
            &built->checked
        ));
    if (failure == RUDA_COMPILER_OK
        && ruda_compiler_request_cancelled(request)) {
        failure = RUDA_COMPILER_CANCELLED;
    }

cleanup:
    if (asts_initialized) {
        ruda_vector_dispose(&asts);
    }
    if (sorted_initialized) {
        ruda_vector_dispose(&sorted_documents);
    }
    if (failure != RUDA_COMPILER_OK) {
        return ruda_compiler_result_fail(built, failure);
    }
    ruda_compiler_result_dispose(result);
    *result = *built;
    memset(built, 0, sizeof(*built));
    if (request != NULL && request->stats != NULL) {
        *request->stats = stats;
    }
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_check_request(
    const RudaCompilerService *service,
    const char *package_name,
    const RudaCompilerCheckRequest *request,
    RudaCompilerResult *result
) {
    RudaCompilerSemanticTarget target;
    memset(&target, 0, sizeof(target));
    target.package_name = package_name;
    return ruda_compiler_service_check_target_request(
        service,
        &target,
        request,
        result
    );
}

RudaCompilerStatus ruda_compiler_service_check(
    const RudaCompilerService *service,
    const char *package_name,
    RudaCompilerResult *result
) {
    return ruda_compiler_service_check_request(
        service,
        package_name,
        NULL,
        result
    );
}

RudaCompilerStatus ruda_compiler_service_check_graph_request(
    const RudaCompilerService *service,
    const RudaCompilerPackage *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    const RudaCompilerCheckRequest *request,
    RudaCompilerResult *result
) {
    RudaCompilerSemanticTarget target;
    memset(&target, 0, sizeof(target));
    target.packages = packages;
    target.package_count = package_count;
    target.root_import_path = root_import_path;
    target.root_import_path_length = root_import_path_length;
    target.is_graph = true;
    return ruda_compiler_service_check_target_request(
        service,
        &target,
        request,
        result
    );
}

RudaCompilerStatus ruda_compiler_service_check_graph(
    const RudaCompilerService *service,
    const RudaCompilerPackage *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    RudaCompilerResult *result
) {
    return ruda_compiler_service_check_graph_request(
        service,
        packages,
        package_count,
        root_import_path,
        root_import_path_length,
        NULL,
        result
    );
}

bool ruda_compiler_result_is_current(
    const RudaCompilerService *service,
    const RudaCompilerResult *result
) {
    size_t index = 0;
    if (service == NULL || !service->initialized || result == NULL
        || !result->initialized
        || result->workspace_generation != service->generation
        || result->units.length != service->documents.length) {
        return false;
    }
    for (index = 0; index < result->units.length; index += 1) {
        const RudaCompilerUnit *unit = ruda_vector_at_const(
            &result->units,
            index
        );
        const RudaCompilerDocument *document = unit == NULL
            ? NULL
            : ruda_compiler_find_document_const(service, unit->file_id);
        if (document == NULL || unit->version != document->version
            || unit->snapshot != document->snapshot) {
            return false;
        }
    }
    return true;
}

const RudaCompilerUnit *ruda_compiler_result_unit(
    const RudaCompilerResult *result,
    size_t index
) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return ruda_vector_at_const(&result->units, index);
}

bool ruda_compiler_result_has_errors(const RudaCompilerResult *result) {
    return result != NULL && result->initialized
        && (result->has_frontend_errors
            || (result->checked.initialized
                && result->checked.diagnostics.length != 0));
}

void ruda_compiler_result_dispose(RudaCompilerResult *result) {
    size_t index = 0;
    if (result == NULL) {
        return;
    }
    ruda_check_result_dispose(&result->checked);
    for (index = 0; index < result->units.length; index += 1) {
        RudaCompilerUnit *unit = ruda_vector_at(&result->units, index);
        ruda_compiler_unit_dispose(unit);
    }
    ruda_vector_dispose(&result->units);
    memset(result, 0, sizeof(*result));
}
