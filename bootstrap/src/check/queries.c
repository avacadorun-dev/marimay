#include "ruda/check/queries.h"

#include <string.h>

const RudaCheckedOccurrence *ruda_checked_occurrence(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->occurrences, index)
        : NULL;
}

const RudaCheckedOccurrence *ruda_checked_occurrence_at(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
) {
    size_t index = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return NULL;
    }
    for (index = 0; index < result->occurrences.length; index += 1) {
        const RudaCheckedOccurrence *occurrence = ruda_vector_at_const(
            &result->occurrences,
            index
        );
        if (occurrence != NULL && occurrence->snapshot == snapshot
            && byte_offset >= occurrence->start
            && byte_offset < occurrence->end) {
            return occurrence;
        }
    }
    return NULL;
}

const RudaCheckedSymbol *ruda_checked_definition_from_reference(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
) {
    const RudaCheckedOccurrence *occurrence = ruda_checked_occurrence_at(
        result,
        snapshot,
        byte_offset
    );
    return occurrence == NULL ? NULL
        : ruda_checked_symbol_by_id(result, occurrence->symbol);
}

static bool ruda_checked_symbol_matches_prefix(
    const RudaCheckedSymbol *symbol,
    const void *prefix,
    size_t prefix_length
) {
    return symbol != NULL && prefix_length <= symbol->name.length
        && (prefix_length == 0
            || memcmp(symbol->name.data, prefix, prefix_length) == 0);
}

size_t ruda_checked_symbol_search_count(
    const RudaCheckResult *result,
    const void *prefix,
    size_t prefix_length
) {
    size_t index = 0;
    size_t count = 0;
    if (result == NULL || !result->initialized
        || (prefix == NULL && prefix_length != 0)) {
        return 0;
    }
    for (index = 0; index < result->symbols.length; index += 1) {
        if (ruda_checked_symbol_matches_prefix(
                ruda_checked_symbol(result, index),
                prefix,
                prefix_length
            )) {
            count += 1;
        }
    }
    return count;
}

const RudaCheckedSymbol *ruda_checked_symbol_search(
    const RudaCheckResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index
) {
    size_t index = 0;
    size_t count = 0;
    if (result == NULL || !result->initialized
        || (prefix == NULL && prefix_length != 0)) {
        return NULL;
    }
    for (index = 0; index < result->symbols.length; index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(result, index);
        if (!ruda_checked_symbol_matches_prefix(
                symbol,
                prefix,
                prefix_length
            )) {
            continue;
        }
        if (count == match_index) {
            return symbol;
        }
        count += 1;
    }
    return NULL;
}

size_t ruda_checked_completion_count(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol
) {
    size_t index = 0;
    size_t count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return 0;
    }
    for (index = 0; index < result->completions.length; index += 1) {
        const RudaCheckedCompletion *completion = ruda_vector_at_const(
            &result->completions,
            index
        );
        if (completion != NULL && completion->snapshot == snapshot
            && ruda_symbol_id_equal(
                completion->receiver_symbol,
                receiver_symbol
            )) {
            count += 1;
        }
    }
    return count;
}

const RudaCheckedCompletion *ruda_checked_completion(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol,
    size_t match_index
) {
    size_t index = 0;
    size_t count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return NULL;
    }
    for (index = 0; index < result->completions.length; index += 1) {
        const RudaCheckedCompletion *completion = ruda_vector_at_const(
            &result->completions,
            index
        );
        if (completion == NULL || completion->snapshot != snapshot
            || !ruda_symbol_id_equal(
                completion->receiver_symbol,
                receiver_symbol
            )) {
            continue;
        }
        if (count == match_index) {
            return completion;
        }
        count += 1;
    }
    return NULL;
}
