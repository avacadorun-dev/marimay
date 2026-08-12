#ifndef RUDA_LEX_LEXER_H
#define RUDA_LEX_LEXER_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/lex/token.h"
#include "ruda/source/source_manager.h"

typedef enum RudaLexStatus {
    RUDA_LEX_OK,
    RUDA_LEX_INVALID_ARGUMENT,
    RUDA_LEX_ALLOCATION_FAILED,
    RUDA_LEX_LIMIT_REACHED
} RudaLexStatus;

typedef struct RudaLexOptions {
    size_t max_tokens;
    size_t max_diagnostics;
    size_t max_nesting;
} RudaLexOptions;

typedef struct RudaLexDiagnostic {
    const char *code;
    const char *message;
    size_t start;
    size_t end;
} RudaLexDiagnostic;

typedef struct RudaLexResult {
    RudaAllocator allocator;
    const RudaSourceSnapshot *snapshot;
    RudaVector tokens;
    RudaVector diagnostics;
    size_t suppressed_diagnostics;
    bool initialized;
} RudaLexResult;

RudaLexOptions ruda_lex_default_options(void);

/*
 * Produces a lossless token stream ending in EOF. Lexical source errors are
 * retained as diagnostics and do not change RUDA_LEX_OK. Allocation failure is
 * failure-atomic. RUDA_LEX_LIMIT_REACHED retains the bounded partial stream.
 */
RudaLexStatus ruda_lex_snapshot(
    const RudaSourceSnapshot *snapshot,
    RudaAllocator allocator,
    RudaLexOptions options,
    RudaLexResult *result
);

void ruda_lex_result_dispose(RudaLexResult *result);

const RudaToken *ruda_lex_token(const RudaLexResult *result, size_t index);
const RudaLexDiagnostic *ruda_lex_diagnostic(
    const RudaLexResult *result,
    size_t index
);

#endif
