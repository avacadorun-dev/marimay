#include "mmay/check/service.h"

#include <string.h>

static MarimayCompilerStatus mmay_compiler_query_current(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result
) {
    if (service == NULL || !service->initialized || result == NULL
        || !result->initialized) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    return mmay_compiler_result_is_current(service, result)
        ? MMAY_COMPILER_OK : MMAY_COMPILER_STALE_RESULT;
}

static MarimayCompilerStatus mmay_compiler_query_snapshot_current(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot
) {
    MarimayCompilerStatus status = mmay_compiler_query_current(service, result);
    size_t index = 0;
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    for (index = 0; index < result->units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_vector_at_const(
            &result->units,
            index
        );
        if (unit != NULL && unit->snapshot == snapshot) {
            return MMAY_COMPILER_OK;
        }
    }
    return MMAY_COMPILER_STALE_RESULT;
}

MarimayCompilerStatus mmay_compiler_service_definition_from_reference(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset,
    const MarimayCheckedSymbol **definition
) {
    MarimayCompilerStatus status;
    if (definition == NULL || snapshot == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    *definition = NULL;
    status = mmay_compiler_query_snapshot_current(
        service,
        result,
        snapshot
    );
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    *definition = mmay_checked_definition_from_reference(
        &result->checked,
        snapshot,
        byte_offset
    );
    return MMAY_COMPILER_OK;
}

MarimayCompilerStatus mmay_compiler_service_symbol_search_count(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t *count
) {
    MarimayCompilerStatus status;
    if (count == NULL || (prefix == NULL && prefix_length != 0)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    *count = 0;
    status = mmay_compiler_query_current(service, result);
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    *count = mmay_checked_symbol_search_count(
        &result->checked,
        prefix,
        prefix_length
    );
    return MMAY_COMPILER_OK;
}

MarimayCompilerStatus mmay_compiler_service_symbol_search(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index,
    const MarimayCheckedSymbol **symbol
) {
    MarimayCompilerStatus status;
    if (symbol == NULL || (prefix == NULL && prefix_length != 0)) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    *symbol = NULL;
    status = mmay_compiler_query_current(service, result);
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    *symbol = mmay_checked_symbol_search(
        &result->checked,
        prefix,
        prefix_length,
        match_index
    );
    return MMAY_COMPILER_OK;
}

MarimayCompilerStatus mmay_compiler_service_symbol_type_spelling(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    MarimaySymbolId symbol,
    MarimayAllocator allocator,
    MarimayTypeSpellingOptions options,
    MarimayOwnedBytes *spelling
) {
    const MarimayCheckedSymbol *checked = NULL;
    MarimayTypeSpellingStatus spelling_status;
    MarimayCompilerStatus status;
    if (spelling == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    memset(spelling, 0, sizeof(*spelling));
    status = mmay_compiler_query_current(service, result);
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    checked = mmay_checked_symbol_by_id(&result->checked, symbol);
    if (checked == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    spelling_status = mmay_checked_type_spelling(
        &result->checked, checked->type, allocator, options, spelling
    );
    if (spelling_status == MMAY_TYPE_SPELLING_OK) {
        return MMAY_COMPILER_OK;
    }
    if (spelling_status == MMAY_TYPE_SPELLING_ALLOCATION_FAILED) {
        return MMAY_COMPILER_ALLOCATION_FAILED;
    }
    if (spelling_status == MMAY_TYPE_SPELLING_LIMIT_REACHED) {
        return MMAY_COMPILER_LIMIT_REACHED;
    }
    if (spelling_status == MMAY_TYPE_SPELLING_UNAVAILABLE) {
        return MMAY_COMPILER_INCOMPLETE_LANGUAGE;
    }
    return MMAY_COMPILER_INVALID_ARGUMENT;
}

MarimayCompilerStatus mmay_compiler_service_completion_count(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol,
    size_t *count
) {
    MarimayCompilerStatus status;
    if (count == NULL || snapshot == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    *count = 0;
    status = mmay_compiler_query_snapshot_current(
        service,
        result,
        snapshot
    );
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    *count = mmay_checked_completion_count(
        &result->checked,
        snapshot,
        receiver_symbol
    );
    return MMAY_COMPILER_OK;
}

MarimayCompilerStatus mmay_compiler_service_completion(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol,
    size_t match_index,
    const MarimayCheckedCompletion **completion
) {
    MarimayCompilerStatus status;
    if (completion == NULL || snapshot == NULL) {
        return MMAY_COMPILER_INVALID_ARGUMENT;
    }
    *completion = NULL;
    status = mmay_compiler_query_snapshot_current(
        service,
        result,
        snapshot
    );
    if (status != MMAY_COMPILER_OK) {
        return status;
    }
    *completion = mmay_checked_completion(
        &result->checked,
        snapshot,
        receiver_symbol,
        match_index
    );
    return MMAY_COMPILER_OK;
}
