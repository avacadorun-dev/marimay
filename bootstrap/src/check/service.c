#include "ruda/check/service.h"

#include <stdlib.h>
#include <string.h>

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
    if (service == NULL || !service->initialized || path == NULL
        || path_length == 0 || (source_bytes == NULL && source_length != 0)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    if (service->documents.length >= service->options.max_documents
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
    if (file_id != NULL) {
        *file_id = document.file_id;
    }
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_overlay(
    RudaCompilerService *service,
    RudaFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length
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
    if (source_length > service->options.max_source_bytes
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
    document->snapshot = snapshot;
    document->snapshot_count += 1;
    service->retained_snapshot_count += 1;
    service->retained_source_bytes = retained_source_bytes;
    return RUDA_COMPILER_OK;
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

RudaCompilerStatus ruda_compiler_service_check(
    const RudaCompilerService *service,
    const char *package_name,
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
    if (result == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    memset(built, 0, sizeof(*built));
    if (service == NULL || !service->initialized || package_name == NULL
        || package_name[0] == '\0' || service->documents.length == 0) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    built->allocator = service->allocator;
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
        RudaLexStatus lex_status;
        RudaParseStatus parse_status;
        memset(&unit, 0, sizeof(unit));
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
        parse_status = ruda_parse_file(
            &unit.lexed,
            service->allocator,
            service->options.parse_options,
            &unit.parsed
        );
        failure = ruda_compiler_parse_status(parse_status);
        if (failure != RUDA_COMPILER_OK) {
            ruda_compiler_unit_dispose(&unit);
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
        ast = &unit->ast;
        if (!ruda_vector_push(&asts, &ast)) {
            failure = RUDA_COMPILER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    failure = ruda_compiler_check_status(ruda_check_package(
        (const RudaAstTree *const *)asts.data,
        asts.length,
        package_name,
        service->allocator,
        service->options.check_options,
        &built->checked
    ));

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
    return RUDA_COMPILER_OK;
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
