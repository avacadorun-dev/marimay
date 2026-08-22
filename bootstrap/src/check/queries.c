#include "mmay/check/queries.h"

#include <string.h>

const MarimayCheckedOccurrence *mmay_checked_occurrence(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->occurrences, index)
        : NULL;
}

const MarimayCheckedOccurrence *mmay_checked_occurrence_at(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset
) {
    size_t index = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return NULL;
    }
    for (index = 0; index < result->occurrences.length; index += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_vector_at_const(
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

const MarimayCheckedSymbol *mmay_checked_definition_from_reference(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset
) {
    const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence_at(
        result,
        snapshot,
        byte_offset
    );
    return occurrence == NULL ? NULL
        : mmay_checked_symbol_by_id(result, occurrence->symbol);
}

static bool mmay_checked_symbol_matches_prefix(
    const MarimayCheckedSymbol *symbol,
    const void *prefix,
    size_t prefix_length
) {
    return symbol != NULL && prefix_length <= symbol->name.length
        && (prefix_length == 0
            || memcmp(symbol->name.data, prefix, prefix_length) == 0);
}

size_t mmay_checked_symbol_search_count(
    const MarimayCheckResult *result,
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
        if (mmay_checked_symbol_matches_prefix(
                mmay_checked_symbol(result, index),
                prefix,
                prefix_length
            )) {
            count += 1;
        }
    }
    return count;
}

const MarimayCheckedSymbol *mmay_checked_symbol_search(
    const MarimayCheckResult *result,
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
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(result, index);
        if (!mmay_checked_symbol_matches_prefix(
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

size_t mmay_checked_completion_count(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol
) {
    size_t index = 0;
    size_t count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return 0;
    }
    for (index = 0; index < result->completions.length; index += 1) {
        const MarimayCheckedCompletion *completion = mmay_vector_at_const(
            &result->completions,
            index
        );
        if (completion != NULL && completion->snapshot == snapshot
            && mmay_symbol_id_equal(
                completion->receiver_symbol,
                receiver_symbol
            )) {
            count += 1;
        }
    }
    return count;
}

const MarimayCheckedCompletion *mmay_checked_completion(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol,
    size_t match_index
) {
    size_t index = 0;
    size_t count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return NULL;
    }
    for (index = 0; index < result->completions.length; index += 1) {
        const MarimayCheckedCompletion *completion = mmay_vector_at_const(
            &result->completions,
            index
        );
        if (completion == NULL || completion->snapshot != snapshot
            || !mmay_symbol_id_equal(
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
