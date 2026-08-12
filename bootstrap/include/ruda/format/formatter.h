#ifndef RUDA_FORMAT_FORMATTER_H
#define RUDA_FORMAT_FORMATTER_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"

typedef enum RudaFormatStatus {
    RUDA_FORMAT_OK,
    RUDA_FORMAT_INVALID_ARGUMENT,
    RUDA_FORMAT_SOURCE_INVALID,
    RUDA_FORMAT_ALLOCATION_FAILED,
    RUDA_FORMAT_LIMIT_REACHED
} RudaFormatStatus;

typedef struct RudaFormatOptions {
    size_t max_tokens;
    size_t max_document_parts;
    size_t max_output_bytes;
    size_t max_nesting;
    size_t max_render_steps;
} RudaFormatOptions;

typedef struct RudaFormatResult {
    RudaOwnedBytes output;
    bool changed;
    bool initialized;
} RudaFormatResult;

RudaFormatOptions ruda_format_default_options(void);

/*
 * Formats one diagnostic-free source snapshot from its lossless parse tree.
 * No partial output is retained on failure. The result owns output bytes.
 */
RudaFormatStatus ruda_format_source(
    const RudaLexResult *lexed,
    const RudaParseResult *parsed,
    RudaAllocator allocator,
    RudaFormatOptions options,
    RudaFormatResult *result
);

void ruda_format_result_dispose(RudaFormatResult *result);

#endif
