#include "manifest_internal.h"

#include <string.h>

MarimayManifestParseOptions mmay_manifest_parse_default_options(void) {
    MarimayManifestParseOptions options;
    options.max_entries = 100000;
    options.max_diagnostics = 100;
    return options;
}

MarimayManifestParseStatus mmay_manifest_parse(
    const MarimaySourceSnapshot *snapshot,
    MarimayManifestKind kind,
    MarimayAllocator allocator,
    MarimayManifestParseOptions options,
    MarimayManifestParseResult *result
) {
    MarimayManifestParser parser;
    if (result == NULL) {
        return MMAY_MANIFEST_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (snapshot == NULL
        || (kind != MMAY_MANIFEST_MODULE && kind != MMAY_MANIFEST_WORKSPACE)
        || options.max_entries == 0 || options.max_diagnostics == 0
        || !mmay_vector_init(&result->entries, allocator, sizeof(MarimayManifestEntry))
        || !mmay_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(MarimayManifestDiagnostic)
        )) {
        return MMAY_MANIFEST_PARSE_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->snapshot = snapshot;
    result->kind = kind;
    result->initialized = true;
    memset(&parser, 0, sizeof(parser));
    parser.snapshot = snapshot;
    parser.bytes = mmay_source_bytes(snapshot);
    parser.length = mmay_source_length(snapshot);
    parser.result = result;
    parser.options = options;
    return mmay_manifest_parse_content(&parser);
}

void mmay_manifest_parse_result_dispose(MarimayManifestParseResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    mmay_vector_dispose(&result->diagnostics);
    mmay_vector_dispose(&result->entries);
    memset(result, 0, sizeof(*result));
}

const MarimayManifestEntry *mmay_manifest_entry(
    const MarimayManifestParseResult *result,
    size_t index
) {
    return result == NULL || !result->initialized
        ? NULL
        : mmay_vector_at_const(&result->entries, index);
}

const MarimayManifestDiagnostic *mmay_manifest_diagnostic(
    const MarimayManifestParseResult *result,
    size_t index
) {
    return result == NULL || !result->initialized
        ? NULL
        : mmay_vector_at_const(&result->diagnostics, index);
}
