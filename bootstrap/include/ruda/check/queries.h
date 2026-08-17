#ifndef RUDA_CHECK_QUERIES_H
#define RUDA_CHECK_QUERIES_H

#include <stddef.h>

#include "ruda/check/result.h"

/* Borrowed result-scoped facts; immutable snapshots keep old results safe. */
const RudaCheckedOccurrence *ruda_checked_occurrence(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedOccurrence *ruda_checked_occurrence_at(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
);
const RudaCheckedSymbol *ruda_checked_definition_from_reference(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
);

/* Exact byte-prefix package-symbol search in canonical definition order. */
size_t ruda_checked_symbol_search_count(
    const RudaCheckResult *result,
    const void *prefix,
    size_t prefix_length
);
const RudaCheckedSymbol *ruda_checked_symbol_search(
    const RudaCheckResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index
);

/* Exact resolver-owned candidates in canonical package/name/symbol order. */
size_t ruda_checked_completion_count(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol
);
const RudaCheckedCompletion *ruda_checked_completion(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol,
    size_t match_index
);

#endif
