#include "ruda/check/service.h"

#include <string.h>

static RudaCompilerStatus ruda_compiler_query_current(
    const RudaCompilerService *service,
    const RudaCompilerResult *result
) {
    if (service == NULL || !service->initialized || result == NULL
        || !result->initialized) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    return ruda_compiler_result_is_current(service, result)
        ? RUDA_COMPILER_OK : RUDA_COMPILER_STALE_RESULT;
}

static RudaCompilerStatus ruda_compiler_query_snapshot_current(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot
) {
    RudaCompilerStatus status = ruda_compiler_query_current(service, result);
    size_t index = 0;
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    for (index = 0; index < result->units.length; index += 1) {
        const RudaCompilerUnit *unit = ruda_vector_at_const(
            &result->units,
            index
        );
        if (unit != NULL && unit->snapshot == snapshot) {
            return RUDA_COMPILER_OK;
        }
    }
    return RUDA_COMPILER_STALE_RESULT;
}

RudaCompilerStatus ruda_compiler_service_definition_from_reference(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset,
    const RudaCheckedSymbol **definition
) {
    RudaCompilerStatus status;
    if (definition == NULL || snapshot == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    *definition = NULL;
    status = ruda_compiler_query_snapshot_current(
        service,
        result,
        snapshot
    );
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    *definition = ruda_checked_definition_from_reference(
        &result->checked,
        snapshot,
        byte_offset
    );
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_symbol_search_count(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t *count
) {
    RudaCompilerStatus status;
    if (count == NULL || (prefix == NULL && prefix_length != 0)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    *count = 0;
    status = ruda_compiler_query_current(service, result);
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    *count = ruda_checked_symbol_search_count(
        &result->checked,
        prefix,
        prefix_length
    );
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_symbol_search(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index,
    const RudaCheckedSymbol **symbol
) {
    RudaCompilerStatus status;
    if (symbol == NULL || (prefix == NULL && prefix_length != 0)) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    *symbol = NULL;
    status = ruda_compiler_query_current(service, result);
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    *symbol = ruda_checked_symbol_search(
        &result->checked,
        prefix,
        prefix_length,
        match_index
    );
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_symbol_type_spelling(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    RudaSymbolId symbol,
    RudaAllocator allocator,
    RudaTypeSpellingOptions options,
    RudaOwnedBytes *spelling
) {
    const RudaCheckedSymbol *checked = NULL;
    RudaTypeSpellingStatus spelling_status;
    RudaCompilerStatus status;
    if (spelling == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    memset(spelling, 0, sizeof(*spelling));
    status = ruda_compiler_query_current(service, result);
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    checked = ruda_checked_symbol_by_id(&result->checked, symbol);
    if (checked == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    spelling_status = ruda_checked_type_spelling(
        &result->checked, checked->type, allocator, options, spelling
    );
    if (spelling_status == RUDA_TYPE_SPELLING_OK) {
        return RUDA_COMPILER_OK;
    }
    if (spelling_status == RUDA_TYPE_SPELLING_ALLOCATION_FAILED) {
        return RUDA_COMPILER_ALLOCATION_FAILED;
    }
    if (spelling_status == RUDA_TYPE_SPELLING_LIMIT_REACHED) {
        return RUDA_COMPILER_LIMIT_REACHED;
    }
    if (spelling_status == RUDA_TYPE_SPELLING_UNAVAILABLE) {
        return RUDA_COMPILER_INCOMPLETE_LANGUAGE;
    }
    return RUDA_COMPILER_INVALID_ARGUMENT;
}

RudaCompilerStatus ruda_compiler_service_completion_count(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol,
    size_t *count
) {
    RudaCompilerStatus status;
    if (count == NULL || snapshot == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    *count = 0;
    status = ruda_compiler_query_snapshot_current(
        service,
        result,
        snapshot
    );
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    *count = ruda_checked_completion_count(
        &result->checked,
        snapshot,
        receiver_symbol
    );
    return RUDA_COMPILER_OK;
}

RudaCompilerStatus ruda_compiler_service_completion(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol,
    size_t match_index,
    const RudaCheckedCompletion **completion
) {
    RudaCompilerStatus status;
    if (completion == NULL || snapshot == NULL) {
        return RUDA_COMPILER_INVALID_ARGUMENT;
    }
    *completion = NULL;
    status = ruda_compiler_query_snapshot_current(
        service,
        result,
        snapshot
    );
    if (status != RUDA_COMPILER_OK) {
        return status;
    }
    *completion = ruda_checked_completion(
        &result->checked,
        snapshot,
        receiver_symbol,
        match_index
    );
    return RUDA_COMPILER_OK;
}
