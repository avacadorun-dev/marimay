#ifndef RUDA_MANIFEST_MANIFEST_H
#define RUDA_MANIFEST_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/source/source_manager.h"

typedef enum RudaManifestKind {
    RUDA_MANIFEST_MODULE,
    RUDA_MANIFEST_WORKSPACE
} RudaManifestKind;

typedef enum RudaManifestDirectiveKind {
    RUDA_MANIFEST_DIRECTIVE_MODULE,
    RUDA_MANIFEST_DIRECTIVE_LANGUAGE,
    RUDA_MANIFEST_DIRECTIVE_REQUIRE,
    RUDA_MANIFEST_DIRECTIVE_CAPABILITY,
    RUDA_MANIFEST_DIRECTIVE_METADATA,
    RUDA_MANIFEST_DIRECTIVE_USE
} RudaManifestDirectiveKind;

typedef struct RudaManifestSpan {
    size_t start;
    size_t end;
} RudaManifestSpan;

typedef struct RudaManifestEntry {
    RudaManifestDirectiveKind kind;
    RudaManifestSpan name;
    RudaManifestSpan values[2];
    size_t value_count;
    size_t source_order;
} RudaManifestEntry;

typedef struct RudaManifestDiagnostic {
    const char *code;
    const char *message;
    size_t start;
    size_t end;
} RudaManifestDiagnostic;

typedef struct RudaManifestParseOptions {
    size_t max_entries;
    size_t max_diagnostics;
} RudaManifestParseOptions;

typedef enum RudaManifestParseStatus {
    RUDA_MANIFEST_PARSE_OK,
    RUDA_MANIFEST_PARSE_INVALID_ARGUMENT,
    RUDA_MANIFEST_PARSE_ALLOCATION_FAILED,
    RUDA_MANIFEST_PARSE_LIMIT_REACHED
} RudaManifestParseStatus;

typedef struct RudaManifestParseResult {
    RudaAllocator allocator;
    const RudaSourceSnapshot *snapshot;
    RudaManifestKind kind;
    RudaVector entries;
    RudaVector diagnostics;
    size_t suppressed_diagnostics;
    bool has_require_group;
    bool has_capability_group;
    bool initialized;
} RudaManifestParseResult;

RudaManifestParseOptions ruda_manifest_parse_default_options(void);
RudaManifestParseStatus ruda_manifest_parse(
    const RudaSourceSnapshot *snapshot,
    RudaManifestKind kind,
    RudaAllocator allocator,
    RudaManifestParseOptions options,
    RudaManifestParseResult *result
);
void ruda_manifest_parse_result_dispose(RudaManifestParseResult *result);
const RudaManifestEntry *ruda_manifest_entry(
    const RudaManifestParseResult *result,
    size_t index
);
const RudaManifestDiagnostic *ruda_manifest_diagnostic(
    const RudaManifestParseResult *result,
    size_t index
);

typedef struct RudaManifestFormatOptions {
    size_t max_decoded_bytes;
    size_t max_output_bytes;
} RudaManifestFormatOptions;

typedef enum RudaManifestFormatStatus {
    RUDA_MANIFEST_FORMAT_OK,
    RUDA_MANIFEST_FORMAT_INVALID_ARGUMENT,
    RUDA_MANIFEST_FORMAT_SOURCE_INVALID,
    RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED,
    RUDA_MANIFEST_FORMAT_LIMIT_REACHED
} RudaManifestFormatStatus;

typedef struct RudaManifestFormatResult {
    RudaOwnedBytes output;
    bool changed;
    bool initialized;
} RudaManifestFormatResult;

RudaManifestFormatOptions ruda_manifest_format_default_options(void);
RudaManifestFormatStatus ruda_manifest_format(
    const RudaManifestParseResult *parsed,
    RudaAllocator allocator,
    RudaManifestFormatOptions options,
    RudaManifestFormatResult *result
);
void ruda_manifest_format_result_dispose(RudaManifestFormatResult *result);

#endif
