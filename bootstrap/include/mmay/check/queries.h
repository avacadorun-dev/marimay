#ifndef MMAY_CHECK_QUERIES_H
#define MMAY_CHECK_QUERIES_H

#include <stddef.h>

#include "mmay/check/result.h"

/* Borrowed result-scoped facts; immutable snapshots keep old results safe. */
const MarimayCheckedOccurrence *mmay_checked_occurrence(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedOccurrence *mmay_checked_occurrence_at(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset
);
const MarimayCheckedSymbol *mmay_checked_definition_from_reference(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset
);

/* Exact byte-prefix package-symbol search in canonical definition order. */
size_t mmay_checked_symbol_search_count(
    const MarimayCheckResult *result,
    const void *prefix,
    size_t prefix_length
);
const MarimayCheckedSymbol *mmay_checked_symbol_search(
    const MarimayCheckResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index
);

/* Exact resolver-owned candidates in canonical package/name/symbol order. */
size_t mmay_checked_completion_count(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol
);
const MarimayCheckedCompletion *mmay_checked_completion(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol,
    size_t match_index
);

#endif
