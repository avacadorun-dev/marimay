#ifndef MMAY_CHECK_SERVICE_QUERIES_H
#define MMAY_CHECK_SERVICE_QUERIES_H

#include "mmay/check/queries.h"
#include "mmay/check/service_types.h"
#include "mmay/check/type_spelling.h"

/*
 * Borrowed result-scoped facts. Wrappers reject older result generations and
 * snapshot arguments that are not owned by the supplied current result.
 */
MarimayCompilerStatus mmay_compiler_service_definition_from_reference(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset,
    const MarimayCheckedSymbol **definition
);
MarimayCompilerStatus mmay_compiler_service_symbol_search_count(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t *count
);
MarimayCompilerStatus mmay_compiler_service_symbol_search(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index,
    const MarimayCheckedSymbol **symbol
);
/*
 * Current-result canonical type text for hover, signature, or completion
 * detail. spelling must be zero-initialized and is caller-owned on success.
 */
MarimayCompilerStatus mmay_compiler_service_symbol_type_spelling(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    MarimaySymbolId symbol,
    MarimayAllocator allocator,
    MarimayTypeSpellingOptions options,
    MarimayOwnedBytes *spelling
);
MarimayCompilerStatus mmay_compiler_service_completion_count(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol,
    size_t *count
);
MarimayCompilerStatus mmay_compiler_service_completion(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver_symbol,
    size_t match_index,
    const MarimayCheckedCompletion **completion
);

#endif
