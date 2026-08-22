#ifndef MMAY_MANIFEST_MANIFEST_H
#define MMAY_MANIFEST_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/source/source_manager.h"

typedef enum MarimayManifestKind {
    MMAY_MANIFEST_MODULE,
    MMAY_MANIFEST_WORKSPACE
} MarimayManifestKind;

typedef enum MarimayManifestDirectiveKind {
    MMAY_MANIFEST_DIRECTIVE_MODULE,
    MMAY_MANIFEST_DIRECTIVE_LANGUAGE,
    MMAY_MANIFEST_DIRECTIVE_REQUIRE,
    MMAY_MANIFEST_DIRECTIVE_CAPABILITY,
    MMAY_MANIFEST_DIRECTIVE_METADATA,
    MMAY_MANIFEST_DIRECTIVE_USE
} MarimayManifestDirectiveKind;

typedef struct MarimayManifestSpan {
    size_t start;
    size_t end;
} MarimayManifestSpan;

typedef struct MarimayManifestEntry {
    MarimayManifestDirectiveKind kind;
    MarimayManifestSpan name;
    MarimayManifestSpan values[2];
    size_t value_count;
    size_t source_order;
} MarimayManifestEntry;

typedef struct MarimayManifestDiagnostic {
    const char *code;
    const char *message;
    size_t start;
    size_t end;
} MarimayManifestDiagnostic;

typedef struct MarimayManifestParseOptions {
    size_t max_entries;
    size_t max_diagnostics;
} MarimayManifestParseOptions;

typedef enum MarimayManifestParseStatus {
    MMAY_MANIFEST_PARSE_OK,
    MMAY_MANIFEST_PARSE_INVALID_ARGUMENT,
    MMAY_MANIFEST_PARSE_ALLOCATION_FAILED,
    MMAY_MANIFEST_PARSE_LIMIT_REACHED
} MarimayManifestParseStatus;

typedef struct MarimayManifestParseResult {
    MarimayAllocator allocator;
    const MarimaySourceSnapshot *snapshot;
    MarimayManifestKind kind;
    MarimayVector entries;
    MarimayVector diagnostics;
    size_t suppressed_diagnostics;
    bool has_require_group;
    bool has_capability_group;
    bool initialized;
} MarimayManifestParseResult;

MarimayManifestParseOptions mmay_manifest_parse_default_options(void);
MarimayManifestParseStatus mmay_manifest_parse(
    const MarimaySourceSnapshot *snapshot,
    MarimayManifestKind kind,
    MarimayAllocator allocator,
    MarimayManifestParseOptions options,
    MarimayManifestParseResult *result
);
void mmay_manifest_parse_result_dispose(MarimayManifestParseResult *result);
const MarimayManifestEntry *mmay_manifest_entry(
    const MarimayManifestParseResult *result,
    size_t index
);
const MarimayManifestDiagnostic *mmay_manifest_diagnostic(
    const MarimayManifestParseResult *result,
    size_t index
);
/* Decodes one validated JSON-compatible string span into owned UTF-8 bytes. */
bool mmay_manifest_decode_string(
    const MarimayManifestParseResult *parsed,
    MarimayManifestSpan span,
    MarimayAllocator allocator,
    MarimayOwnedBytes *output
);

typedef struct MarimayManifestFormatOptions {
    size_t max_decoded_bytes;
    size_t max_output_bytes;
} MarimayManifestFormatOptions;

typedef enum MarimayManifestFormatStatus {
    MMAY_MANIFEST_FORMAT_OK,
    MMAY_MANIFEST_FORMAT_INVALID_ARGUMENT,
    MMAY_MANIFEST_FORMAT_SOURCE_INVALID,
    MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED,
    MMAY_MANIFEST_FORMAT_LIMIT_REACHED
} MarimayManifestFormatStatus;

typedef struct MarimayManifestFormatResult {
    MarimayOwnedBytes output;
    bool changed;
    bool initialized;
} MarimayManifestFormatResult;

MarimayManifestFormatOptions mmay_manifest_format_default_options(void);
MarimayManifestFormatStatus mmay_manifest_format(
    const MarimayManifestParseResult *parsed,
    MarimayAllocator allocator,
    MarimayManifestFormatOptions options,
    MarimayManifestFormatResult *result
);
void mmay_manifest_format_result_dispose(MarimayManifestFormatResult *result);

#endif
