#ifndef MMAY_PARSE_PARSER_H
#define MMAY_PARSE_PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/lex/lexer.h"
#include "mmay/syntax/syntax.h"

typedef enum MarimayParseStatus {
    MMAY_PARSE_OK,
    MMAY_PARSE_INVALID_ARGUMENT,
    MMAY_PARSE_ALLOCATION_FAILED,
    MMAY_PARSE_LIMIT_REACHED
} MarimayParseStatus;

typedef struct MarimayParseOptions {
    size_t max_events;
    size_t max_diagnostics;
    size_t max_nesting;
} MarimayParseOptions;

typedef struct MarimayParseDiagnostic {
    const char *code;
    const char *message;
    size_t start;
    size_t end;
} MarimayParseDiagnostic;

typedef struct MarimayParseResult {
    MarimayAllocator allocator;
    MarimaySyntaxTree tree;
    MarimayVector diagnostics;
    size_t suppressed_diagnostics;
    bool initialized;
} MarimayParseResult;

typedef struct MarimayIncrementalParseStats {
    size_t reused_top_level_nodes;
    size_t reparsed_bytes;
} MarimayIncrementalParseStats;

MarimayParseOptions mmay_parse_default_options(void);
bool mmay_parse_options_valid(MarimayParseOptions options);

/*
 * Builds an immutable lossless green tree from a complete lexer result.
 * Syntax errors produce diagnostics and recovery nodes while status remains
 * MMAY_PARSE_OK. Allocation failure is failure-atomic.
 */
MarimayParseStatus mmay_parse_file(
    const MarimayLexResult *lexed,
    MarimayAllocator allocator,
    MarimayParseOptions options,
    MarimayParseResult *result
);

/*
 * Parses a new overlay and reuses unchanged top-level green subtrees from the
 * previous result. The edit ranges describe [edit_start, previous_edit_end)
 * and [edit_start, new_edit_end); unchanged prefix/suffix bytes are verified.
 * Prefix nodes retain their offsets and are reusable for any edit. Suffix
 * nodes are pointer-reused when the edit does not shift byte offsets.
 */
MarimayParseStatus mmay_parse_file_incremental(
    const MarimayParseResult *previous,
    const MarimayLexResult *lexed,
    size_t edit_start,
    size_t previous_edit_end,
    size_t new_edit_end,
    MarimayAllocator allocator,
    MarimayParseOptions options,
    MarimayParseResult *result,
    MarimayIncrementalParseStats *stats
);

void mmay_parse_result_dispose(MarimayParseResult *result);
const MarimayParseDiagnostic *mmay_parse_diagnostic(
    const MarimayParseResult *result,
    size_t index
);

#endif
