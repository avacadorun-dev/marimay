#ifndef MMAY_FORMAT_FORMATTER_H
#define MMAY_FORMAT_FORMATTER_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"

typedef enum MarimayFormatStatus {
    MMAY_FORMAT_OK,
    MMAY_FORMAT_INVALID_ARGUMENT,
    MMAY_FORMAT_SOURCE_INVALID,
    MMAY_FORMAT_ALLOCATION_FAILED,
    MMAY_FORMAT_LIMIT_REACHED
} MarimayFormatStatus;

typedef struct MarimayFormatOptions {
    size_t max_tokens;
    size_t max_document_parts;
    size_t max_output_bytes;
    size_t max_nesting;
    size_t max_render_steps;
} MarimayFormatOptions;

typedef struct MarimayFormatResult {
    MarimayOwnedBytes output;
    bool changed;
    bool initialized;
} MarimayFormatResult;

MarimayFormatOptions mmay_format_default_options(void);

/*
 * Formats one diagnostic-free source snapshot from its lossless parse tree.
 * No partial output is retained on failure. The result owns output bytes.
 */
MarimayFormatStatus mmay_format_source(
    const MarimayLexResult *lexed,
    const MarimayParseResult *parsed,
    MarimayAllocator allocator,
    MarimayFormatOptions options,
    MarimayFormatResult *result
);

void mmay_format_result_dispose(MarimayFormatResult *result);

#endif
