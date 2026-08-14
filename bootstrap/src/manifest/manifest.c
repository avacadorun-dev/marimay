#include "manifest_internal.h"

#include <string.h>

RudaManifestParseOptions ruda_manifest_parse_default_options(void) {
    RudaManifestParseOptions options;
    options.max_entries = 100000;
    options.max_diagnostics = 100;
    return options;
}

RudaManifestParseStatus ruda_manifest_parse(
    const RudaSourceSnapshot *snapshot,
    RudaManifestKind kind,
    RudaAllocator allocator,
    RudaManifestParseOptions options,
    RudaManifestParseResult *result
) {
    RudaManifestParser parser;
    if (result == NULL) {
        return RUDA_MANIFEST_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (snapshot == NULL
        || (kind != RUDA_MANIFEST_MODULE && kind != RUDA_MANIFEST_WORKSPACE)
        || options.max_entries == 0 || options.max_diagnostics == 0
        || !ruda_vector_init(&result->entries, allocator, sizeof(RudaManifestEntry))
        || !ruda_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(RudaManifestDiagnostic)
        )) {
        return RUDA_MANIFEST_PARSE_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->snapshot = snapshot;
    result->kind = kind;
    result->initialized = true;
    memset(&parser, 0, sizeof(parser));
    parser.snapshot = snapshot;
    parser.bytes = ruda_source_bytes(snapshot);
    parser.length = ruda_source_length(snapshot);
    parser.result = result;
    parser.options = options;
    return ruda_manifest_parse_content(&parser);
}

void ruda_manifest_parse_result_dispose(RudaManifestParseResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_vector_dispose(&result->diagnostics);
    ruda_vector_dispose(&result->entries);
    memset(result, 0, sizeof(*result));
}

const RudaManifestEntry *ruda_manifest_entry(
    const RudaManifestParseResult *result,
    size_t index
) {
    return result == NULL || !result->initialized
        ? NULL
        : ruda_vector_at_const(&result->entries, index);
}

const RudaManifestDiagnostic *ruda_manifest_diagnostic(
    const RudaManifestParseResult *result,
    size_t index
) {
    return result == NULL || !result->initialized
        ? NULL
        : ruda_vector_at_const(&result->diagnostics, index);
}
