#include "ruda/lex/lexer.h"

RudaLexOptions ruda_lex_default_options(void) {
    RudaLexOptions options;
    options.max_tokens = 1000000;
    options.max_diagnostics = 100;
    options.max_nesting = 256;
    return options;
}

bool ruda_lex_options_valid(RudaLexOptions options) {
    return options.max_tokens >= 2 && options.max_diagnostics != 0
        && options.max_nesting >= 2;
}

void ruda_lex_result_dispose(RudaLexResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_vector_dispose(&result->tokens);
    ruda_vector_dispose(&result->diagnostics);
    result->snapshot = NULL;
    result->suppressed_diagnostics = 0;
    result->initialized = false;
}

const RudaToken *ruda_lex_token(const RudaLexResult *result, size_t index) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return ruda_vector_at_const(&result->tokens, index);
}

const RudaLexDiagnostic *ruda_lex_diagnostic(
    const RudaLexResult *result,
    size_t index
) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return ruda_vector_at_const(&result->diagnostics, index);
}
