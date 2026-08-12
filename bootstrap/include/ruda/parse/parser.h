#ifndef RUDA_PARSE_PARSER_H
#define RUDA_PARSE_PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/lex/lexer.h"
#include "ruda/syntax/syntax.h"

typedef enum RudaParseStatus {
    RUDA_PARSE_OK,
    RUDA_PARSE_INVALID_ARGUMENT,
    RUDA_PARSE_ALLOCATION_FAILED,
    RUDA_PARSE_LIMIT_REACHED
} RudaParseStatus;

typedef struct RudaParseOptions {
    size_t max_events;
    size_t max_diagnostics;
    size_t max_nesting;
} RudaParseOptions;

typedef struct RudaParseDiagnostic {
    const char *code;
    const char *message;
    size_t start;
    size_t end;
} RudaParseDiagnostic;

typedef struct RudaParseResult {
    RudaAllocator allocator;
    RudaSyntaxTree tree;
    RudaVector diagnostics;
    size_t suppressed_diagnostics;
    bool initialized;
} RudaParseResult;

typedef struct RudaIncrementalParseStats {
    size_t reused_top_level_nodes;
    size_t reparsed_bytes;
} RudaIncrementalParseStats;

RudaParseOptions ruda_parse_default_options(void);

/*
 * Builds an immutable lossless green tree from a complete lexer result.
 * Syntax errors produce diagnostics and recovery nodes while status remains
 * RUDA_PARSE_OK. Allocation failure is failure-atomic.
 */
RudaParseStatus ruda_parse_file(
    const RudaLexResult *lexed,
    RudaAllocator allocator,
    RudaParseOptions options,
    RudaParseResult *result
);

/*
 * Parses a new overlay and reuses unchanged top-level green subtrees from the
 * previous result. The edit ranges describe [edit_start, previous_edit_end)
 * and [edit_start, new_edit_end); unchanged prefix/suffix bytes are verified.
 * Prefix nodes retain their offsets and are reusable for any edit. Suffix
 * nodes are pointer-reused when the edit does not shift byte offsets.
 */
RudaParseStatus ruda_parse_file_incremental(
    const RudaParseResult *previous,
    const RudaLexResult *lexed,
    size_t edit_start,
    size_t previous_edit_end,
    size_t new_edit_end,
    RudaAllocator allocator,
    RudaParseOptions options,
    RudaParseResult *result,
    RudaIncrementalParseStats *stats
);

void ruda_parse_result_dispose(RudaParseResult *result);
const RudaParseDiagnostic *ruda_parse_diagnostic(
    const RudaParseResult *result,
    size_t index
);

#endif
