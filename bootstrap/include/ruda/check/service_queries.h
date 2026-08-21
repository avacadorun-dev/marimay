#ifndef RUDA_CHECK_SERVICE_QUERIES_H
#define RUDA_CHECK_SERVICE_QUERIES_H

#include "ruda/check/queries.h"
#include "ruda/check/service_types.h"
#include "ruda/check/type_spelling.h"

/*
 * Borrowed result-scoped facts. Wrappers reject older result generations and
 * snapshot arguments that are not owned by the supplied current result.
 */
RudaCompilerStatus ruda_compiler_service_definition_from_reference(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset,
    const RudaCheckedSymbol **definition
);
RudaCompilerStatus ruda_compiler_service_symbol_search_count(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t *count
);
RudaCompilerStatus ruda_compiler_service_symbol_search(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const void *prefix,
    size_t prefix_length,
    size_t match_index,
    const RudaCheckedSymbol **symbol
);
/*
 * Current-result canonical type text for hover, signature, or completion
 * detail. spelling must be zero-initialized and is caller-owned on success.
 */
RudaCompilerStatus ruda_compiler_service_symbol_type_spelling(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    RudaSymbolId symbol,
    RudaAllocator allocator,
    RudaTypeSpellingOptions options,
    RudaOwnedBytes *spelling
);
RudaCompilerStatus ruda_compiler_service_completion_count(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol,
    size_t *count
);
RudaCompilerStatus ruda_compiler_service_completion(
    const RudaCompilerService *service,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver_symbol,
    size_t match_index,
    const RudaCheckedCompletion **completion
);

#endif
