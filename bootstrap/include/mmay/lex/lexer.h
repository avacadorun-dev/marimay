#ifndef MMAY_LEX_LEXER_H
#define MMAY_LEX_LEXER_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/lex/token.h"
#include "mmay/source/source_manager.h"

typedef enum MarimayLexStatus {
    MMAY_LEX_OK,
    MMAY_LEX_INVALID_ARGUMENT,
    MMAY_LEX_ALLOCATION_FAILED,
    MMAY_LEX_LIMIT_REACHED
} MarimayLexStatus;

typedef struct MarimayLexOptions {
    size_t max_tokens;
    size_t max_diagnostics;
    size_t max_nesting;
} MarimayLexOptions;

typedef struct MarimayLexDiagnostic {
    const char *code;
    const char *message;
    size_t start;
    size_t end;
} MarimayLexDiagnostic;

typedef struct MarimayLexResult {
    MarimayAllocator allocator;
    const MarimaySourceSnapshot *snapshot;
    MarimayVector tokens;
    MarimayVector diagnostics;
    size_t suppressed_diagnostics;
    bool initialized;
} MarimayLexResult;

MarimayLexOptions mmay_lex_default_options(void);
bool mmay_lex_options_valid(MarimayLexOptions options);

/*
 * Produces a lossless token stream ending in EOF. Lexical source errors are
 * retained as diagnostics and do not change MMAY_LEX_OK. Allocation failure is
 * failure-atomic. MMAY_LEX_LIMIT_REACHED retains the bounded partial stream.
 */
MarimayLexStatus mmay_lex_snapshot(
    const MarimaySourceSnapshot *snapshot,
    MarimayAllocator allocator,
    MarimayLexOptions options,
    MarimayLexResult *result
);

void mmay_lex_result_dispose(MarimayLexResult *result);

const MarimayToken *mmay_lex_token(const MarimayLexResult *result, size_t index);
const MarimayLexDiagnostic *mmay_lex_diagnostic(
    const MarimayLexResult *result,
    size_t index
);

#endif
